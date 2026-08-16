# Petit-Ami Graphics: Native Wayland Backend — Architectural Plan

Status: draft for review, rev 2 (post-audit residue swept; threading contract, test rig, and remote-backend interplay added)
Scope: Linux graphics module (`graphics.c`) targeting Wayland directly, replacing reliance on XWayland. The existing Xlib backend is retained.

---

## 1. Background and Goals

The current Linux backend talks Xlib to an X server — today, in practice, to XWayland on Wayland desktops. This works and will keep working for years, but carries XWayland's costs: blurry output under fractional scaling, approximated frame timing, an extra protocol hop, and long-term legacy status.

Goals of the native backend, in priority order:

1. **Correct rendering on scaled displays** (the visible XWayland deficiency).
2. **Clean frame timing** via Wayland frame callbacks.
3. **Longevity** — Wayland is the native protocol of every major desktop; Xorg is in maintenance mode.
4. **Leave a seam open for a future 3D viewport surface** (see §9). Raw speed is explicitly *not* a goal; XWayland overhead is small for Ami's workload.

Non-goals: GPU-accelerated 2D rasterization (see §5.3 rationale), network transparency (use waypipe externally), replacing the X backend.

## 2. Constraints and Design Principles

- **No interface stacking.** Ami sits directly on the platform protocol, as with Xlib and Cocoa. Libraries that *serve* Ami (rasterizer, font engine) are acceptable; frameworks that *wrap* the platform (GTK, SDL) are not. Wayland's client library is the platform here.
- **Coexistence.** Backend selected at startup: `$WAYLAND_DISPLAY` present → Wayland module; else fall back to Xlib. XWayland remains the safety net; no flag day.
- **API stability.** The `ami_` graphics API does not change. Where Wayland cannot honor an existing semantic (window positioning — §7), the call degrades gracefully rather than the API forking.
- **Progressive path preserved.** stdio → terminal → graphical promotion must work identically; nothing in this backend may require an event loop the program didn't opt into.
- **Multithread contract kept.** graphics.c is multithreadable by contract — locks encompass only their data, no outer serialization. libwayland-client is thread-aware but not casually thread-safe: per-thread `wl_event_queue`s and the prepare-read protocol are what keep the contract, and they are designed in from the skeleton (§5.1), not retrofitted.

## 3. Architecture Overview

```
  Ami application (C / Pascal / ...)
        |
  ami_ graphics API  (unchanged)
        |
  graphics_wl.c  ──────────────────────────────┐
        |                                      |
  +-----+---------+---------+--------+         |
  | window mgmt   | raster  | text   | input   |
  | (xdg-shell)   | (§5.3)  | (§5.4) | (xkb)   |
  +-----+---------+---------+--------+         |
        |                                      |
  libwayland-client  ← Unix socket →  compositor (Mutter/KWin/wlroots)
        |
  wl_shm buffers (mmap'd, CPU-filled)  → composited by GPU
```

Key inversion vs. Xlib: the X server rasterized and decorated; here the module owns rasterization, text, cursors, and (on GNOME) window frames. Wayland provides buffer transport, input delivery, and window state negotiation — nothing else.

## 4. Module Layout

- `graphics_wl.c` — the backend proper: connection, registry, surfaces, buffers, input, event translation to Ami events.
- `protocols/` — XML from `wayland-protocols` compiled by `wayland-scanner` at build time (or generated sources committed): `xdg-shell` (mandatory), `xdg-decoration-unstable-v1`, `fractional-scale-v1`, `viewporter`, `cursor-shape-v1` (optional niceties).
- Rasterization and text live behind an internal drawing interface so §5.3's rasterizer remains replaceable, and so a future 3D/EGL surface type (§9) can implement the same buffer-facing seam.

Build additions: `pkg-config` for `wayland-client`, `xkbcommon`, plus scanner invocation. All dependencies are ubiquitous on any Wayland-capable system.

## 5. Core Subsystems

### 5.1 Connection and Event Loop

`wl_display_connect(NULL)` → registry roundtrip → bind globals: `wl_compositor`, `wl_shm`, `xdg_wm_base`, `wl_seat`, `wl_output`, decoration manager if offered. Event dispatch integrates into Ami's existing loop via `wl_display_get_fd()` and the prepare-read/dispatch protocol (`wl_display_prepare_read` / `read_events` / `dispatch_pending`), poll-driven. This is a clean fd-based model — no Xlib-style hidden buffering surprises. The prepare-read protocol is used precisely because multiple Ami threads may drive the API concurrently; any helper thread that waits on protocol events gets its own `wl_event_queue`, honoring the multithread contract of §2. `xdg_wm_base` ping/pong must be answered promptly or the compositor marks the app unresponsive.

### 5.2 Windows

Per Ami window: `wl_surface` → `xdg_surface` → `xdg_toplevel`. The compositor drives size via `configure` events (proposed dimensions + states: maximized, tiled, activated); the module acks, resizes buffers, redraws, commits. Initial mapping requires the commit-without-buffer / configure / commit-with-buffer handshake. Ami menus and dropdowns map to `xdg_popup` with its grab semantics. Multiple toplevels per application are unrestricted. Minimize/maximize/fullscreen map directly to xdg_toplevel requests.

### 5.3 Buffers and Rasterization

**Transport:** `wl_shm`. A memfd-backed pool, mmap'd, ARGB8888, double-buffered per window; attach + damage(rects) + commit publishes a frame. Buffer release events recycle back buffers. This is the same mechanism VTE-class applications use; for widget/text/chart workloads CPU fill rates exceed requirements by orders of magnitude, and compositing of the result is GPU-accelerated by the compositor regardless.

**Rasterization — resolved: own rasterizer.** Audit of `linux/graphics.c` (17,922 lines) settled this. Primitives draw via core X calls into per-screen backing pixmaps (`sc->xbuf`) blitted to windows with `XCopyArea` — so a rasterizer must be acquired, and Cairo is disqualified: Ami's API exposes bitwise color-mix modes (mdxor/mdand/mdor via `mod2fnc[]` → X raster ops), and Cairo's Porter-Duff operator model cannot express bitwise ROPs. The backend therefore carries its own scanline rasterizer: lines with width, rectangles, arcs and filled arcs, scaled picture blits (loaded images draw with scaling), clipping — all funneled through a pixel-store function applying the current mix mode. The existing backing-store architecture (draw to buffer, blit damage) is already Wayland's shape; the pixmap becomes the mmap'd shm buffer and `XCopyArea`-to-window becomes attach/damage/commit. In-buffer scroll (`XCopyArea` on self) becomes a row copy ordered against the scroll direction — memmove handles overlap within a row, not across the row sequence.

**Not GPU rendering, deliberately:** GPU 2D is compositing of CPU-rasterized fragments in most real toolkits (glyphs are FreeType-rendered and atlased even in GPU terminals); adopting EGL for 2D adds context/shader/driver-variance burden while removing only the solved scanline part. The buffer seam keeps an EGL path possible later without API change.

### 5.4 Text

Largely already ported: the X backend renders glyphs with FreeType directly (`ft_draw_char`, rotated variant, glyph cache) and blits via a stipple-pixmap trick (`FillStippled` + `XFillRectangle`) so the server paints glyphs in the current color through the current raster op. On Wayland the final blit becomes simpler: blend the cached FreeType glyph bitmap into the shm buffer in a pixel loop applying color and mix mode. Cache, metrics, sizing, and rotation code move unchanged. Remaining work: the blit function, and confirming font discovery (fontconfig appears present for face lookup). HarfBuzz only if complex-script shaping ever becomes a requirement.

### 5.5 Input

`wl_seat` → `wl_pointer` + `wl_keyboard` (touch later if ever). Keyboard delivery is scancode + keymap fd; translation through **libxkbcommon** is mandatory (no server-side keysym translation exists). Key repeat is client-implemented from the seat's repeat-info — a timer in the event loop. Pointer events are surface-local; enter/leave/motion/button/axis map onto Ami's event model directly. Modifier state comes from xkb state updates, not per-event masks.

### 5.6 Cursors, Clipboard, Misc

- **Cursors:** loaded from the system theme via `libwayland-cursor` (or the newer `cursor-shape-v1` protocol where offered) and set on every pointer enter; otherwise the window inherits stale cursors.
- **Clipboard/selection:** `wl_data_device` — offer/request negotiation over pipes. Different shape from X selections, same idea; map to Ami's existing clipboard API.
- **Outputs:** track `wl_output` geometry/scale for §5.8.

### 5.7 Decoration

Compositor-drawn frames exist only as the `xdg-decoration` extension: KWin and wlroots compositors honor it; GNOME/Mutter refuses. Plan: request server-side decoration where available; where unavailable, use **libdecor** (exists precisely for non-toolkit clients). Since Ami already imitates GTK theming with own-drawn widgets — and already draws frames for MDI child windows, so the frame path is extension, not invention — an own-drawn header matching Ami's theme is a possible later refinement, and the only route that removes the one §10 dependency that is not universally stock. libdecor first keeps this out of the critical path.

### 5.8 Scaling and Frame Pacing

Integer per-output scale (`wl_output`/`wl_surface.set_buffer_scale`) at minimum — near-free and eliminates the blurry-XWayland motivation. Fractional scale via `fractional-scale-v1` + `viewporter` as a later increment. Frame callbacks throttle redraw: never render unthrottled; coalesce damage between callbacks.

### 5.9 Internal Window Tree (widget children)

The X backend defines widget children as X11 child windows, delegating clipping, occlusion, expose tracking, and event routing to the server. Wayland has no equivalent at widget granularity: subsurfaces are heavyweight (own buffer and commit lifecycle) and suited to a handful per toplevel (video panes, future 3D viewports), not per-widget use. The backend therefore carries a client-side window tree: per-child rectangles with z-order and clipping, damage accumulation per child, and pointer hit-testing that routes surface-local input to the correct child — a miniature reimplementation of the X child-window model minus the protocol.

Precedent: Qt 4.4 (alien widgets) and GTK 2.18→4 (client-side windows) made exactly this migration under stable APIs, for reasons that also justify it here — atomic whole-tree updates (no flicker at child boundaries during resize), no per-window protocol traffic, and correct rendering across child boundaries. Widget *content* rendering already exists in Ami; this layer replaces geometry management and event routing only.

Strategic option (Phase 0 decision): build the tree as a standalone layer usable by the X backend as well — one X window per toplevel, children client-side — so both backends share the widget-geometry layer, matching how GTK/Qt use X today. Converts port overhead into shared architecture.

## 6. Drawing-Model Reconciliation

Ami's API is immediate-mode: a program may invoke a primitive at any time. Wayland is frame-oriented. Reconciliation: primitives render immediately into the back buffer and accumulate damage rectangles; a pending flag schedules attach/damage/commit on the next frame callback (or immediately when none is outstanding). Programs perceive immediate drawing; the compositor receives complete frames. Same batching discipline as basht's line-oriented output layer — accumulate at the natural boundary, publish whole units.

## 7. API Semantic Gaps (Xlib features Wayland forbids)

- **Window positioning:** clients cannot place toplevels; no `set_position` exists. Ami positioning calls become no-ops (or map to initial-placement hints where a compositor offers them). Document as platform behavior. Menus and dropdowns keep exact placement through xdg_popup anchoring; free-floating windows do not — including the modal dialogs, which will land where the compositor places new toplevels (centered, on most desktops).
- **Global coordinates / window location queries:** not expressible; return best-effort or fixed origin.
- **Pointer warping:** absent (`pointer-constraints` covers only lock/confine for games).
- **Screen-wide operations** (grabs beyond popups, cross-window input synthesis, screenshots): compositor-mediated or unavailable; out of scope for Ami's API.

These are protocol philosophy, not implementation gaps; the audit of which Ami calls are affected is milestone 0 work.

## 8. Interplay with the Remote Backend

The remote protocol makes this port pay twice. graph_server is itself an Ami graphics client: its display side goes through the same backend selection, so the Wayland module extends remote serving to Wayland desktops with no protocol work at all.

More valuable in the other direction: the remote suite is a ready-made conformance harness. Every primitive, widget, event, and dialog the protocol carries is something the new backend must render identically, and the existing remote regressions (widget walk-throughs, sound, breakout, the line benchmark) already exercise nearly the whole API surface. Running that suite against a Wayland-backed graph_server and comparing captures against the X-backed golden results validates the backend with tests that already exist. This is the Phase 6 acceptance gate (§12). As a bonus, driving the application under test through the remote protocol needs no input synthesis on the compositor at all (§11).

## 9. Forward Seam: 3D Viewport Surface Type

Future direction, kept explicitly out of this plan's critical path: an Ami window (or subsurface within one) whose buffers come from EGL/GBM as dmabufs instead of shm, hosting a 3D context — compositor handles both identically. The internal buffer/present seam in §4 is the only accommodation this plan makes: the drawing interface abstracts "produce buffer, declare damage, present," so an EGL implementation can slot in beside the shm one. GL dispatch would go through vendor-neutral EGL/GLES (Mesa in practice, never bound to specifically); llvmpipe provides the no-GPU fallback. The eventual fixed-function-style `ami_` 3D API is a separate design effort.

## 10. Dependencies

| Component | Role | Notes |
|---|---|---|
| libwayland-client | protocol transport | core |
| wayland-protocols + wayland-scanner | xdg-shell etc. | build-time codegen |
| libxkbcommon | keyboard translation | mandatory |
| libwayland-cursor | themed cursors | small |
| libdecor | frames on GNOME | avoidable by own-drawn frames (§5.7) |
| FreeType + fontconfig | text rendering and face lookup | already in use by the X backend |

All are stock packages on every Wayland-running distribution, libdecor least universally so.

## 11. Test Rig

The current rig — Xvfb, mutter, `import` for capture, xdotool for input — is X to the bone; none of it exists under native Wayland. The replacement stands up in Phase 1 alongside the skeleton, not in polish: every later phase's "ends runnable" claim means runnable here, not just on a desktop.

- **Compositor host:** `weston --headless` (stock, scriptable) for CI; sway headless as the wlroots cross-check, since the two families differ where it matters (decoration, popup behavior).
- **Capture:** the screencopy protocol replaces `import` — weston's screenshooter, `grim` on wlroots.
- **Input synthesis:** virtual-pointer/virtual-keyboard protocols on wlroots; on weston, either the rig injects through a test seat or the application under test is driven through the remote protocol (§8), which needs no compositor-side synthesis at all.
- **Discipline unchanged:** golden captures and printed invariants, as on the X rig. Assertions are never masked to make a test pass; tests print what they measured.

## 12. Phasing

- **Phase 0 — Audit.** Determine §13 answers; enumerate Ami API calls hitting §7 gaps.
- **Phase 1 — Skeleton.** Connect, bind globals, one toplevel with shm double buffer, test pattern, configure/resize/close handled. (~500 lines; validates all plumbing.) The headless rig (§11) comes up with it — capture and input proven before any rendering code exists.
- **Phase 2 — Input.** xkbcommon keyboard with repeat, pointer, mapped to Ami events; interactive test program runs.
- **Phase 3 — Rasterization.** Drawing interface implemented by the own scanline rasterizer (§5.3); Ami primitive set renders; damage-tracked commits.
- **Phase 4 — Text.** Font selection, metrics, drawing through the ported FreeType stack; Ami text API complete.
- **Phase 5 — Windowing completeness.** Popups/menus, cursors, clipboard, decoration via xdg-decoration/libdecor, multiple windows.
- **Phase 6 — Polish and acceptance.** Integer scaling, frame pacing/coalescing, fractional scaling, stress and soak tests; runtime backend selection wired; demos ported; the remote conformance pass (§8) against X-backed golden results gates the flip.

Each phase ends runnable; the X backend remains default-off-Wayland throughout until Phase 6 flips selection.

## 13. Open Questions

1. ~~Which regime is `graphics.c` in?~~ **Answered:** core-X server-side primitives into backing pixmaps; own rasterizer required and chosen (§5.3, mix-mode rationale).
2. ~~Text stack today?~~ **Answered:** FreeType direct with own glyph cache; only the blit changes (§5.4).
3. **Child-window semantics in use:** which X child-window features Ami actually relies on (per-child event masks, window gravity, save-unders, background pixmaps, child-relative coordinates in the API) — determines the internal window tree's feature list (§5.9). Note: Ami already draws its own frame decoration for MDI child windows, so frame-drawing code exists and may extend to toplevel decoration (§5.7), reducing or removing the libdecor dependency.
4. **Positioning semantics:** which Ami demos/programs rely on window placement, and what degradation is acceptable — modal dialog placement included (§7).
5. **Decoration aesthetics:** is libdecor's generic frame acceptable long-term, or is an Ami-themed own-drawn frame wanted.

## 14. References

- Wayland book (Drew DeVault): https://wayland-book.com — the standard walkthrough of every §5 subsystem.
- Protocol specs: https://wayland.app (browsable, with per-compositor support matrices).
- xdg-shell, xdg-decoration, fractional-scale XML in `wayland-protocols`.
- libdecor: https://gitlab.freedesktop.org/libdecor/libdecor
- xkbcommon docs: https://xkbcommon.org
