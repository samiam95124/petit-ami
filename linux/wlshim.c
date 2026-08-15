/** ****************************************************************************
*                                                                              *
*                       X MODEL ON WAYLAND - SHIM                              *
*                                                                              *
*                       Copyright (C) 2026 Scott A. Franco                     *
*                                                                              *
* Implements the X window and drawing model on Wayland for the Petit-Ami       *
* Wayland graphics backend (graphics_wl.c). See wlshim.h for the contract.     *
*                                                                              *
* The model implemented:                                                       *
*                                                                              *
* - A window is a client-side record: a rectangle in its parent, a stacking    *
*   position, and a canvas holding its content. Root-parented windows are      *
*   Wayland xdg toplevels owning a wl_surface and a pair of shared memory      *
*   buffers; everything below them lives only in this process. The composite   *
*   of a toplevel's tree is assembled into its shm buffer and committed,       *
*   which is the plan's client-side child window tree: what the X server did   *
*   for child windows, done here.                                              *
*                                                                              *
* - Drawing is our own scanline rasterization into canvases, through a pixel   *
*   store honoring the GC function (copy/xor/and/or), which is the property    *
*   that disqualified off-the-shelf renderers. Stippled fills reproduce the    *
*   glyph path of the X backend exactly.                                       *
*                                                                              *
* - Events are synthesized in X shapes from Wayland input and shell events,    *
*   with request serials maintained so the backend's notify hunts (waitxevt    *
*   and friends) order correctly.                                              *
*                                                                              *
* - ConnectionNumber() returns an epoll descriptor combining the Wayland       *
*   socket with the shim's internal wake and repeat timers, so the backend's   *
*   select-based system event loop drives everything unchanged.                *
*                                                                              *
*                          BSD LICENSE INFORMATION                             *
*                                                                              *
* (same terms as graphics.c)                                                   *
*                                                                              *
*******************************************************************************/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <math.h>
#include <poll.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <sys/eventfd.h>
#include <fcntl.h>
#include <time.h>

#include <wayland-client.h>
#include "wlproto/xdg-shell-client-protocol.h"
#include <xkbcommon/xkbcommon.h>

#include "wlshim.h"

#define MAXATOM 256    /* max interned atoms */
#define DYNATOM 100    /* first dynamically interned atom id */

/* canvas: pixel content of a window or pixmap. Depth 1 canvases (glyph
   stipples) store one word per pixel holding 0 or 1; uniform addressing
   costs a little memory on tiny bitmaps and saves every op having two
   layouts */
typedef struct canvas {

    int       w, h;
    int       depth;
    uint32_t* px;

} canvas;

typedef enum { ot_win, ot_pix } otag;

typedef struct wlwin wlwin;

/* toplevel (root-parented window) data */
typedef struct wltop {

    struct wl_surface*  surf;
    struct xdg_surface* xsurf;
    struct xdg_toplevel* xtop;
    int    confw, confh;    /* compositor proposed size */
    int    configured;      /* first configure arrived */
    int    activated;       /* keyboard active state */
    int    maximized;
    int    mapnoted;        /* MapNotify synthesized */
    struct wl_buffer* buf[2];
    uint32_t* bufpx[2];
    size_t bufsz[2];
    int    bufw, bufh;
    int    bufbusy[2];
    int    curbuf;
    int    dmg;             /* damage accumulated */
    int    dx1, dy1, dx2, dy2;
    struct wl_callback* fcb; /* outstanding frame callback */
    Time   fcbtime;         /* when it was armed */
    int    commitpend;      /* damage waiting on frame callback */
    int    titx, tity, titw, tith; /* interactive move rectangle */

} wltop;

/* window record */
struct wlwin {

    otag    tag;
    wlwin*  parent;
    wlwin*  childs;   /* stacking order, head is bottom */
    wlwin*  sibnext;
    int     x, y;     /* parent relative */
    int     w, h;
    int     mapped;
    long    evtmask;
    canvas* can;
    wltop*  top;      /* toplevel data if root child */
    char*   title;
    Cursor  cursor;

};

/* pixmap record */
typedef struct {

    otag    tag;
    canvas* can;

} wlpix;

/* graphics context */
struct _XGC {

    uint32_t fg, bg;
    int      func;
    int      lw;
    int      lstyle;
    char     dashes[16];
    int      ndash;
    int      fillstyle;
    Pixmap   stipple;
    int      tsx, tsy;
    int      arcmode;

};

/* event queue entry */
typedef struct evq {

    XEvent      e;
    struct evq* next;

} evq;

/* the display */
struct _XDisplay {

    struct wl_display*    dpy;
    struct wl_registry*   reg;
    struct wl_compositor* comp;
    struct wl_shm*        shm;
    struct xdg_wm_base*   wm;
    struct wl_seat*       seat;
    struct wl_pointer*    ptr;
    struct wl_keyboard*   kbd;
    struct wl_output*     out;
    int    outw, outh;      /* output pixels */
    int    outmmw, outmmh;  /* output millimeters */
    int    epfd;            /* epoll handed out as the connection number */
    int    evfd;            /* eventfd waking the queue */
    int    rtfd;            /* key repeat timerfd */
    pthread_mutex_t lk;     /* shim lock (recursive) */
    unsigned long serial;   /* request serial counter */
    evq   *eqh, *eqt, *eqf; /* event queue head/tail/free */
    wlwin  root;
    /* pointer state */
    wlwin* ptrtop;          /* toplevel the pointer is on */
    wlwin* ptrwin;          /* leaf window under pointer */
    double ptrx, ptry;      /* surface coordinates */
    unsigned int modstate;  /* modifier and button mask, X encoding */
    wlwin* grab;            /* pointer grab window */
    uint32_t inserial;      /* last input serial, for interactive move */
    /* keyboard state */
    wlwin* focus;           /* explicit input focus window */
    wlwin* kbdtop;          /* toplevel with wayland keyboard focus */
    struct xkb_context* xkb;
    struct xkb_keymap*  keymap;
    struct xkb_state*   xst;
    int    rptdelay, rptrate; /* repeat delay/rate, ms */
    uint32_t rptkey;        /* repeating key (X keycode), 0 if none */
    /* atoms */
    char*  atomtab[MAXATOM];
    int    natom;
    XErrorHandler errh;
    int    dumpseq;
    int    injfd;           /* rig input injection stream, -1 if none */

};

static struct _XDisplay thedpy; /* the single connection */
static int    dpyopen;

/* forward */
static void pump(Display* d);
static void compose(Display* d, wlwin* t);
static void flushtops(Display* d);

/*******************************************************************************

Lock and serial helpers

*******************************************************************************/

#define LK(d)   pthread_mutex_lock(&(d)->lk)
#define ULK(d)  pthread_mutex_unlock(&(d)->lk)

static unsigned long nextserial(Display* d)
{ return (++d->serial); }

static Time nowms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ((Time)ts.tv_sec*1000+ts.tv_nsec/1000000);
}

/*******************************************************************************

Event queue

Events synthesized anywhere are appended here; the X-shaped read calls take
them off. The eventfd keeps the backend's select loop honest when an event
is enqueued without Wayland socket traffic (cross thread sends, repeats).

*******************************************************************************/

static void enq(Display* d, XEvent* e)
{
    evq* p;
    uint64_t one = 1;

    if (d->eqf) { p = d->eqf; d->eqf = p->next; }
    else p = malloc(sizeof(evq));
    p->e = *e;
    p->next = NULL;
    if (d->eqt) d->eqt->next = p; else d->eqh = p;
    d->eqt = p;
    if (write(d->evfd, &one, 8) < 0) { /* wake is advisory */ }
}

static int deq(Display* d, XEvent* e)
{
    evq* p;

    p = d->eqh;
    if (!p) return (0);
    d->eqh = p->next;
    if (!d->eqh) d->eqt = NULL;
    *e = p->e;
    p->next = d->eqf;
    d->eqf = p;
    return (1);
}

/* base event fill */
static void evbase(Display* d, XEvent* e, int type, wlwin* w)
{
    memset(e, 0, sizeof(XEvent));
    e->type = type;
    e->xany.serial = d->serial;
    e->xany.display = d;
    e->xany.window = (Window)w;
}

/*******************************************************************************

Canvas management and rasterization

*******************************************************************************/

static canvas* newcanvas(int w, int h, int depth)
{
    canvas* c;

    if (w < 1) w = 1;
    if (h < 1) h = 1;
    c = malloc(sizeof(canvas));
    c->w = w; c->h = h; c->depth = depth;
    c->px = calloc((size_t)w*h, 4);
    return (c);
}

static void freecanvas(canvas* c)
{
    if (!c) return;
    free(c->px);
    free(c);
}

/* window canvas on demand; windows begin life white, the paper color of the
   library */
static canvas* wincanvas(wlwin* w)
{
    int i;

    if (!w->can) {

        w->can = newcanvas(w->w, w->h, 32);
        for (i = 0; i < w->can->w*w->can->h; i++) w->can->px[i] = 0xffffff;

    }
    return (w->can);
}

/* resize a window canvas preserving overlapping content; new area white */
static void sizecanvas(wlwin* w, int nw, int nh)
{
    canvas* c;
    canvas* n;
    int x, y;

    c = w->can;
    if (!c) return; /* created at need at the new size */
    if (c->w == nw && c->h == nh) return;
    n = newcanvas(nw, nh, c->depth);
    for (y = 0; y < nh; y++)
        for (x = 0; x < nw; x++)
            n->px[y*nw+x] = (x < c->w && y < c->h)? c->px[y*c->w+x]: 0xffffff;
    freecanvas(c);
    w->can = n;
}

/* resolve a drawable to its canvas */
static canvas* drwcan(Drawable dr)
{
    wlwin* w;
    wlpix* p;

    if (!dr) return (NULL);
    if (*(otag*)dr == ot_win) {

        w = (wlwin*)dr;
        if (!w->parent && !w->top) return (NULL); /* the root draws nowhere */
        return (wincanvas(w));

    }
    p = (wlpix*)dr;
    return (p->can);
}

/* accumulate damage on the toplevel containing a window; rect in window
   coordinates */
static void dmg(Drawable dr, int x, int y, int w, int h)
{
    wlwin* p;
    wltop* t;
    int x2, y2;

    if (!dr || *(otag*)dr != ot_win) return;
    p = (wlwin*)dr;
    /* to surface coordinates, walking up to the toplevel (root child) */
    while (p->parent && p->parent->parent) { x += p->x; y += p->y; p = p->parent; }
    if (!p->parent) return; /* the root draws nowhere */
    t = p->top;
    if (!t) return;
    x2 = x+w; y2 = y+h;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (!t->dmg) { t->dx1 = x; t->dy1 = y; t->dx2 = x2; t->dy2 = y2; t->dmg = 1; }
    else {

        if (x < t->dx1) t->dx1 = x;
        if (y < t->dy1) t->dy1 = y;
        if (x2 > t->dx2) t->dx2 = x2;
        if (y2 > t->dy2) t->dy2 = y2;

    }
}

/* the pixel store: every drawing operation lands here, applying the GC
   function. This one function is why the backend's bitwise color mix modes
   survive the platform change */
static inline void plot(canvas* c, int x, int y, uint32_t col, int func)
{
    uint32_t* p;

    if (x < 0 || y < 0 || x >= c->w || y >= c->h) return;
    p = &c->px[(size_t)y*c->w+x];
    switch (func) {

        case GXcopy: *p = col; break;
        case GXxor:  *p ^= col&0xffffff; break;
        case GXand:  *p &= col|0xff000000; break;
        case GXor:   *p |= col&0xffffff; break;
        case GXnoop: break;
        case GXclear: *p = 0; break;
        default: *p = col; break;

    }
}

/* stipple test for stippled fills */
static int stipbit(GC gc, int x, int y)
{
    wlpix* sp;
    canvas* c;
    int sx, sy;

    sp = (wlpix*)gc->stipple;
    if (!sp || !sp->can) return (1);
    c = sp->can;
    sx = (x-gc->tsx)%c->w; if (sx < 0) sx += c->w;
    sy = (y-gc->tsy)%c->h; if (sy < 0) sy += c->h;
    return (c->px[(size_t)sy*c->w+sx] != 0);
}

/* filled rectangle, honoring fill style */
static void fillrect(canvas* c, GC gc, int x, int y, int w, int h)
{
    int i, j;

    for (j = y; j < y+h; j++)
        for (i = x; i < x+w; i++) {

            if (gc->fillstyle == FillStippled && !stipbit(gc, i, j)) continue;
            plot(c, i, j, gc->fg, gc->func);

        }
}

/* solid or dashed unit line via Bresenham; wide lines are quads below */
static void thinline(canvas* c, GC gc, int x1, int y1, int x2, int y2)
{
    int dx, dy, sx, sy, err, e2;
    int di, don, doff, dpos, pen;

    dx = abs(x2-x1); dy = -abs(y2-y1);
    sx = x1 < x2? 1: -1; sy = y1 < y2? 1: -1;
    err = dx+dy;
    /* dash bookkeeping */
    don = doff = 0;
    if (gc->lstyle != LineSolid && gc->ndash > 0) {

        don = gc->dashes[0];
        doff = gc->ndash > 1? gc->dashes[1]: gc->dashes[0];

    }
    dpos = 0; pen = 1; di = 0;
    for (;;) {

        if (don) { /* dashed */

            pen = dpos < don;
            if (++dpos >= don+doff) dpos = 0;

        }
        if (pen) plot(c, x1, y1, gc->fg, gc->func);
        if (x1 == x2 && y1 == y2) break;
        e2 = 2*err;
        if (e2 >= dy) { err += dy; x1 += sx; }
        if (e2 <= dx) { err += dx; y1 += sy; }
        (void)di;

    }
}

/* scanline even-odd polygon fill */
static void fillpoly(canvas* c, GC gc, XPoint* pt, int n)
{
    int   miny, maxy, y, i, j, k;
    float xs[64];
    int   nx;
    int   x1, x2;

    if (n < 3) return;
    miny = maxy = pt[0].y;
    for (i = 1; i < n; i++) {

        if (pt[i].y < miny) miny = pt[i].y;
        if (pt[i].y > maxy) maxy = pt[i].y;

    }
    for (y = miny; y <= maxy; y++) {

        nx = 0;
        j = n-1;
        for (i = 0; i < n; i++) {

            int yi = pt[i].y, yj = pt[j].y;
            if ((yi <= y && yj > y) || (yj <= y && yi > y)) {

                if (nx < 64)
                    xs[nx++] = pt[i].x+
                        (float)(y-yi)/(yj-yi)*(pt[j].x-pt[i].x);

            }
            j = i;

        }
        /* insertion sort */
        for (i = 1; i < nx; i++) {

            float v = xs[i];
            for (k = i-1; k >= 0 && xs[k] > v; k--) xs[k+1] = xs[k];
            xs[k+1] = v;

        }
        for (i = 0; i+1 < nx; i += 2) {

            x1 = (int)ceilf(xs[i]);
            x2 = (int)floorf(xs[i+1]);
            for (k = x1; k <= x2; k++) {

                if (gc->fillstyle == FillStippled && !stipbit(gc, k, y))
                    continue;
                plot(c, k, y, gc->fg, gc->func);

            }

        }

    }
}

/* wide line as a filled quad, butt capped */
static void wideline(canvas* c, GC gc, int x1, int y1, int x2, int y2)
{
    float  dx, dy, len, nx, ny, hw;
    XPoint q[4];

    dx = x2-x1; dy = y2-y1;
    len = sqrtf(dx*dx+dy*dy);
    if (len < 0.001f) { fillrect(c, gc, x1-gc->lw/2, y1-gc->lw/2, gc->lw, gc->lw); return; }
    hw = gc->lw/2.0f;
    nx = -dy/len*hw; ny = dx/len*hw;
    q[0].x = (short)lroundf(x1+nx); q[0].y = (short)lroundf(y1+ny);
    q[1].x = (short)lroundf(x2+nx); q[1].y = (short)lroundf(y2+ny);
    q[2].x = (short)lroundf(x2-nx); q[2].y = (short)lroundf(y2-ny);
    q[3].x = (short)lroundf(x1-nx); q[3].y = (short)lroundf(y1-ny);
    fillpoly(c, gc, q, 4);
}

static void anyline(canvas* c, GC gc, int x1, int y1, int x2, int y2)
{
    float dx, dy, len, t0, t1, don, doff, p;
    int   sx1, sy1, sx2, sy2;

    if (gc->lw <= 1) { thinline(c, gc, x1, y1, x2, y2); return; }
    if (gc->lstyle == LineSolid || gc->ndash < 1) {

        wideline(c, gc, x1, y1, x2, y2);
        return;

    }
    /* wide dashed: quads per dash segment */
    dx = x2-x1; dy = y2-y1;
    len = sqrtf(dx*dx+dy*dy);
    don = gc->dashes[0];
    doff = gc->ndash > 1? gc->dashes[1]: gc->dashes[0];
    p = 0;
    while (p < len) {

        t0 = p/len;
        t1 = (p+don) > len? 1.0f: (p+don)/len;
        sx1 = (int)lroundf(x1+dx*t0); sy1 = (int)lroundf(y1+dy*t0);
        sx2 = (int)lroundf(x1+dx*t1); sy2 = (int)lroundf(y1+dy*t1);
        wideline(c, gc, sx1, sy1, sx2, sy2);
        p += don+doff;

    }
}

/* arc path points, X semantics: bounding box, angles in 64ths of a degree,
   zero at three o'clock, positive counterclockwise. Returns point count */
static int arcpath(int x, int y, int w, int h, int a1, int a2,
                   XPoint* pt, int maxpt)
{
    float cx, cy, rx, ry, st, en, stp;
    int   n, i;

    cx = x+w/2.0f; cy = y+h/2.0f;
    rx = w/2.0f; ry = h/2.0f;
    st = a1/64.0f*(float)M_PI/180.0f;
    en = (a1+a2)/64.0f*(float)M_PI/180.0f;
    n = (int)(fabsf(en-st)*(rx > ry? rx: ry))+2;
    if (n > maxpt-2) n = maxpt-2;
    if (n < 2) n = 2;
    stp = (en-st)/(n-1);
    for (i = 0; i < n; i++) {

        pt[i].x = (short)lroundf(cx+rx*cosf(st+stp*i));
        /* X y axis grows downward; the angle convention is counterclockwise
           in right side up terms, so y subtracts */
        pt[i].y = (short)lroundf(cy-ry*sinf(st+stp*i));

    }
    return (n);
}

#define MAXARC 1024

/*******************************************************************************

Shim rendering entries (drawing API)

*******************************************************************************/

static int gcnoop(GC gc)
{ return (gc->func == GXnoop); }

int XDrawPoint(Display* d, Drawable dr, GC gc, int x, int y)
{
    canvas* c;

    LK(d); nextserial(d);
    c = drwcan(dr);
    if (c && !gcnoop(gc)) {

        plot(c, x, y, gc->fg, gc->func);
        dmg(dr, x, y, 1, 1);

    }
    ULK(d);
    return (0);
}

int XDrawLine(Display* d, Drawable dr, GC gc, int x1, int y1, int x2, int y2)
{
    canvas* c;
    int lx, ly, hx, hy, b;

    LK(d); nextserial(d);
    c = drwcan(dr);
    if (c && !gcnoop(gc)) {

        anyline(c, gc, x1, y1, x2, y2);
        lx = x1 < x2? x1: x2; hx = x1 > x2? x1: x2;
        ly = y1 < y2? y1: y2; hy = y1 > y2? y1: y2;
        b = gc->lw/2+1;
        dmg(dr, lx-b, ly-b, hx-lx+2*b+1, hy-ly+2*b+1);

    }
    ULK(d);
    return (0);
}

int XDrawRectangle(Display* d, Drawable dr, GC gc, int x, int y,
                   unsigned int w, unsigned int h)
{
    canvas* c;

    LK(d); nextserial(d);
    c = drwcan(dr);
    if (c && !gcnoop(gc)) {

        anyline(c, gc, x, y, x+w, y);
        anyline(c, gc, x+w, y, x+w, y+h);
        anyline(c, gc, x+w, y+h, x, y+h);
        anyline(c, gc, x, y+h, x, y);
        dmg(dr, x-gc->lw, y-gc->lw, w+2*gc->lw+1, h+2*gc->lw+1);

    }
    ULK(d);
    return (0);
}

int XFillRectangle(Display* d, Drawable dr, GC gc, int x, int y,
                   unsigned int w, unsigned int h)
{
    canvas* c;

    LK(d); nextserial(d);
    c = drwcan(dr);
    if (c && !gcnoop(gc)) {

        fillrect(c, gc, x, y, w, h);
        dmg(dr, x, y, w, h);

    }
    ULK(d);
    return (0);
}

int XDrawArc(Display* d, Drawable dr, GC gc, int x, int y,
             unsigned int w, unsigned int h, int a1, int a2)
{
    canvas* c;
    XPoint  pt[MAXARC];
    int     n, i;

    LK(d); nextserial(d);
    c = drwcan(dr);
    if (c && !gcnoop(gc)) {

        n = arcpath(x, y, w, h, a1, a2, pt, MAXARC);
        for (i = 0; i+1 < n; i++)
            anyline(c, gc, pt[i].x, pt[i].y, pt[i+1].x, pt[i+1].y);
        dmg(dr, x-gc->lw, y-gc->lw, w+2*gc->lw+1, h+2*gc->lw+1);

    }
    ULK(d);
    return (0);
}

int XFillArc(Display* d, Drawable dr, GC gc, int x, int y,
             unsigned int w, unsigned int h, int a1, int a2)
{
    canvas* c;
    XPoint  pt[MAXARC];
    int     n;

    LK(d); nextserial(d);
    c = drwcan(dr);
    if (c && !gcnoop(gc)) {

        n = arcpath(x, y, w, h, a1, a2, pt, MAXARC-1);
        if (abs(a2) < 360*64 && gc->arcmode == ArcPieSlice) {

            /* close through the center */
            pt[n].x = (short)(x+w/2);
            pt[n].y = (short)(y+h/2);
            n++;

        }
        fillpoly(c, gc, pt, n);
        dmg(dr, x, y, w+1, h+1);

    }
    ULK(d);
    return (0);
}

int XFillPolygon(Display* d, Drawable dr, GC gc, XPoint* points,
                 int npoints, int shape, int mode)
{
    canvas* c;
    int     i, lx, ly, hx, hy;

    LK(d); nextserial(d);
    c = drwcan(dr);
    if (c && !gcnoop(gc) && npoints > 0) {

        fillpoly(c, gc, points, npoints);
        lx = hx = points[0].x; ly = hy = points[0].y;
        for (i = 1; i < npoints; i++) {

            if (points[i].x < lx) lx = points[i].x;
            if (points[i].x > hx) hx = points[i].x;
            if (points[i].y < ly) ly = points[i].y;
            if (points[i].y > hy) hy = points[i].y;

        }
        dmg(dr, lx, ly, hx-lx+1, hy-ly+1);

    }
    ULK(d);
    return (0);
}

int XCopyArea(Display* d, Drawable src, Drawable dst, GC gc, int sx, int sy,
              unsigned int w, unsigned int h, int dx, int dy)
{
    canvas* sc;
    canvas* dc;
    int     iw, ih, y, x;
    int     ydir;

    LK(d); nextserial(d);
    sc = drwcan(src);
    dc = drwcan(dst);
    if (sc && dc) {

        iw = w; ih = h;
        /* clip both rectangles into their canvases */
        if (sx < 0) { iw += sx; dx -= sx; sx = 0; }
        if (sy < 0) { ih += sy; dy -= sy; sy = 0; }
        if (dx < 0) { iw += dx; sx -= dx; dx = 0; }
        if (dy < 0) { ih += dy; sy -= dy; dy = 0; }
        if (sx+iw > sc->w) iw = sc->w-sx;
        if (sy+ih > sc->h) ih = sc->h-sy;
        if (dx+iw > dc->w) iw = dc->w-dx;
        if (dy+ih > dc->h) ih = dc->h-dy;
        if (iw > 0 && ih > 0) {

            /* row order against the copy direction: overlapping vertical
               self-copies must not read rows already written */
            ydir = (sc == dc && dy > sy)? -1: 1;
            for (y = ydir > 0? 0: ih-1; ydir > 0? y < ih: y >= 0; y += ydir) {

                if (gc->func == GXcopy || !gc)
                    memmove(&dc->px[(size_t)(dy+y)*dc->w+dx],
                            &sc->px[(size_t)(sy+y)*sc->w+sx], (size_t)iw*4);
                else
                    for (x = 0; x < iw; x++)
                        plot(dc, dx+x, dy+y,
                             sc->px[(size_t)(sy+y)*sc->w+sx+x], gc->func);

            }
            dmg(dst, dx, dy, iw, ih);

        }

    }
    ULK(d);
    return (0);
}

int XClearArea(Display* d, Window w, int x, int y, unsigned int width,
               unsigned int height, Bool exposures)
{
    wlwin*  win;
    canvas* c;
    int     i, j;
    XEvent  e;

    LK(d); nextserial(d);
    win = (wlwin*)w;
    c = drwcan(w);
    if (c) {

        if (!width) width = c->w-x;
        if (!height) height = c->h-y;
        for (j = y; j < (int)(y+height); j++)
            for (i = x; i < (int)(x+width); i++)
                plot(c, i, j, 0xffffff, GXcopy);
        dmg(w, x, y, width, height);
        if (exposures) {

            evbase(d, &e, Expose, win);
            e.xexpose.x = x; e.xexpose.y = y;
            e.xexpose.width = width; e.xexpose.height = height;
            enq(d, &e);

        }

    }
    ULK(d);
    return (0);
}

/*******************************************************************************

Images

*******************************************************************************/

XImage* XCreateImage(Display* d, Visual* v, unsigned int depth, int format,
                     int offset, char* data, unsigned int width,
                     unsigned int height, int bitmap_pad, int bytes_per_line)
{
    XImage* i;

    (void)v; (void)offset; (void)bitmap_pad;
    i = malloc(sizeof(XImage));
    i->width = width; i->height = height;
    i->format = format; i->depth = depth;
    i->bits_per_pixel = depth == 1? 1: 32;
    i->bytes_per_line = bytes_per_line? bytes_per_line:
                        (depth == 1? (int)(width+7)/8: (int)width*4);
    i->data = data;
    if (!i->data) i->data = calloc((size_t)i->bytes_per_line*height, 1);
    return (i);
}

int XDestroyImage(XImage* img)
{
    free(img->data);
    free(img);
    return (0);
}

unsigned long XGetPixel(XImage* img, int x, int y)
{
    unsigned char* r;

    if (x < 0 || y < 0 || x >= img->width || y >= img->height) return (0);
    r = (unsigned char*)img->data+(size_t)y*img->bytes_per_line;
    if (img->depth == 1) return ((r[x/8] >> (x%8))&1);
    return (((uint32_t*)r)[x]);
}

int XPutPixel(XImage* img, int x, int y, unsigned long pixel)
{
    unsigned char* r;

    if (x < 0 || y < 0 || x >= img->width || y >= img->height) return (0);
    r = (unsigned char*)img->data+(size_t)y*img->bytes_per_line;
    if (img->depth == 1) {

        if (pixel) r[x/8] |= 1 << (x%8);
        else r[x/8] &= ~(1 << (x%8));

    } else ((uint32_t*)r)[x] = (uint32_t)pixel;
    return (0);
}

XImage* XGetImage(Display* d, Drawable dr, int x, int y, unsigned int w,
                  unsigned int h, unsigned long plane_mask, int format)
{
    canvas* c;
    XImage* i;
    unsigned int xx, yy;

    (void)plane_mask;
    LK(d); nextserial(d);
    c = drwcan(dr);
    i = XCreateImage(d, NULL, 24, format? format: ZPixmap, 0, NULL, w, h, 32, 0);
    if (c)
        for (yy = 0; yy < h; yy++)
            for (xx = 0; xx < w; xx++)
                XPutPixel(i, xx, yy,
                    (x+(int)xx >= 0 && y+(int)yy >= 0 &&
                     x+(int)xx < c->w && y+(int)yy < c->h)?
                        c->px[(size_t)(y+yy)*c->w+x+xx]: 0);
    ULK(d);
    return (i);
}

int XPutImage(Display* d, Drawable dr, GC gc, XImage* img, int sx, int sy,
              int dx, int dy, unsigned int w, unsigned int h)
{
    canvas* c;
    unsigned int x, y;
    unsigned long p;

    LK(d); nextserial(d);
    c = drwcan(dr);
    if (c) {

        for (y = 0; y < h; y++)
            for (x = 0; x < w; x++) {

                p = XGetPixel(img, sx+x, sy+y);
                if (c->depth == 1) {

                    if (dx+(int)x >= 0 && dy+(int)y >= 0 &&
                        dx+(int)x < c->w && dy+(int)y < c->h)
                        c->px[(size_t)(dy+y)*c->w+dx+x] = p? 1: 0;

                } else plot(c, dx+x, dy+y, (uint32_t)p,
                            gc? gc->func: GXcopy);

            }
        dmg(dr, dx, dy, w, h);

    }
    ULK(d);
    return (0);
}

/*******************************************************************************

Pixmaps and graphics contexts

*******************************************************************************/

Pixmap XCreatePixmap(Display* d, Drawable ref, unsigned int w,
                     unsigned int h, unsigned int depth)
{
    wlpix* p;

    (void)ref;
    LK(d); nextserial(d);
    p = malloc(sizeof(wlpix));
    p->tag = ot_pix;
    p->can = newcanvas(w, h, depth == 1? 1: 32);
    ULK(d);
    return ((Pixmap)p);
}

int XFreePixmap(Display* d, Pixmap px)
{
    wlpix* p;

    LK(d); nextserial(d);
    p = (wlpix*)px;
    if (p) { freecanvas(p->can); free(p); }
    ULK(d);
    return (0);
}

GC XCreateGC(Display* d, Drawable ref, unsigned long mask, XGCValues* v)
{
    GC gc;

    (void)ref;
    LK(d); nextserial(d);
    gc = calloc(1, sizeof(struct _XGC));
    gc->fg = 0;
    gc->bg = 0xffffff;
    gc->func = GXcopy;
    gc->lw = 1;
    gc->lstyle = LineSolid;
    gc->fillstyle = FillSolid;
    gc->arcmode = ArcPieSlice;
    if (v) {

        if (mask&GCFunction) gc->func = v->function;
        if (mask&GCForeground) gc->fg = (uint32_t)v->foreground;
        if (mask&GCBackground) gc->bg = (uint32_t)v->background;

    }
    ULK(d);
    return (gc);
}

int XFreeGC(Display* d, GC gc)
{
    LK(d); nextserial(d);
    free(gc);
    ULK(d);
    return (0);
}

int XSetForeground(Display* d, GC gc, unsigned long fg)
{ LK(d); gc->fg = (uint32_t)fg; ULK(d); return (0); }

int XSetBackground(Display* d, GC gc, unsigned long bg)
{ LK(d); gc->bg = (uint32_t)bg; ULK(d); return (0); }

int XSetFunction(Display* d, GC gc, int func)
{ LK(d); gc->func = func; ULK(d); return (0); }

int XSetLineAttributes(Display* d, GC gc, unsigned int width, int ls,
                       int cs, int js)
{
    (void)cs; (void)js;
    LK(d);
    gc->lw = width? width: 1;
    gc->lstyle = ls;
    ULK(d);
    return (0);
}

int XSetDashes(Display* d, GC gc, int off, const char* list, int n)
{
    int i;

    (void)off;
    LK(d);
    if (n > 16) n = 16;
    for (i = 0; i < n; i++) gc->dashes[i] = list[i];
    gc->ndash = n;
    ULK(d);
    return (0);
}

int XSetFillStyle(Display* d, GC gc, int style)
{ LK(d); gc->fillstyle = style; ULK(d); return (0); }

int XSetStipple(Display* d, GC gc, Pixmap stipple)
{ LK(d); gc->stipple = stipple; ULK(d); return (0); }

int XSetTSOrigin(Display* d, GC gc, int x, int y)
{ LK(d); gc->tsx = x; gc->tsy = y; ULK(d); return (0); }

int XSetClipMask(Display* d, GC gc, Pixmap mask)
{ (void)d; (void)gc; (void)mask; return (0); } /* only ever cleared */

int XSetArcMode(Display* d, GC gc, int mode)
{ LK(d); gc->arcmode = mode; ULK(d); return (0); }

/*******************************************************************************

Window tree

*******************************************************************************/

/* stack child on top (X initial placement) */
static void linkchild(wlwin* parent, wlwin* w)
{
    wlwin** p;

    w->parent = parent;
    w->sibnext = NULL;
    p = &parent->childs;
    while (*p) p = &(*p)->sibnext;
    *p = w;
}

static void unlinkchild(wlwin* w)
{
    wlwin** p;

    if (!w->parent) return;
    p = &w->parent->childs;
    while (*p && *p != w) p = &(*p)->sibnext;
    if (*p) *p = w->sibnext;
    w->sibnext = NULL;
}

/* the toplevel above a window (the root child containing it) */
static wlwin* winTop(wlwin* w)
{
    if (!w || !w->parent) return (NULL);
    while (w->parent && w->parent->parent) w = w->parent;
    return (w->top? w: NULL);
}

/* surface coordinates of a window's origin */
static void winorg(wlwin* w, int* x, int* y)
{
    *x = 0; *y = 0;
    while (w && w->parent) { *x += w->x; *y += w->y; w = w->parent; }
}

/* deepest mapped window containing surface point; xo/yo receive the
   window-local coordinates */
static wlwin* hittest(wlwin* top, int sx, int sy, int* xo, int* yo)
{
    wlwin* w;
    wlwin* c;
    wlwin* best;
    int    bx, by;

    w = top; bx = sx; by = sy;
    for (;;) {

        best = NULL;
        /* later siblings are higher in the stack; take the last hit */
        for (c = w->childs; c; c = c->sibnext)
            if (c->mapped && bx >= c->x && by >= c->y &&
                bx < c->x+c->w && by < c->y+c->h) best = c;
        if (!best) break;
        bx -= best->x; by -= best->y;
        w = best;

    }
    *xo = bx; *yo = by;
    return (w);
}

/* forward: shell listeners */
static void mktoplevel(Display* d, wlwin* w);

Window XCreateWindow(Display* d, Window parent, int x, int y,
                     unsigned int w, unsigned int h, unsigned int bw,
                     int depth, unsigned int cls, Visual* visual,
                     unsigned long valuemask, XSetWindowAttributes* attr)
{
    wlwin* win;

    (void)bw; (void)depth; (void)cls; (void)visual; (void)valuemask;
    (void)attr;
    LK(d); nextserial(d);
    win = calloc(1, sizeof(wlwin));
    win->tag = ot_win;
    win->x = x; win->y = y;
    win->w = w? w: 1; win->h = h? h: 1;
    linkchild(parent? (wlwin*)parent: &d->root, win);
    ULK(d);
    return ((Window)win);
}

Window XCreateSimpleWindow(Display* d, Window parent, int x, int y,
                           unsigned int w, unsigned int h, unsigned int bw,
                           unsigned long border, unsigned long background)
{
    (void)border; (void)background;
    return (XCreateWindow(d, parent, x, y, w, h, bw, 0, InputOutput, NULL,
                          0, NULL));
}

static void droptop(Display* d, wlwin* w)
{
    wltop* t;
    int    i;

    t = w->top;
    if (!t) return;
    if (t->fcb) wl_callback_destroy(t->fcb);
    for (i = 0; i < 2; i++)
        if (t->buf[i]) {

            wl_buffer_destroy(t->buf[i]);
            munmap(t->bufpx[i], t->bufsz[i]);

        }
    if (t->xtop) xdg_toplevel_destroy(t->xtop);
    if (t->xsurf) xdg_surface_destroy(t->xsurf);
    if (t->surf) wl_surface_destroy(t->surf);
    free(t);
    w->top = NULL;
    wl_display_flush(d->dpy);
}

int XDestroyWindow(Display* d, Window w)
{
    wlwin* win;
    wlwin* c;

    LK(d); nextserial(d);
    win = (wlwin*)w;
    while ((c = win->childs)) { ULK(d); XDestroyWindow(d, (Window)c); LK(d); }
    if (d->ptrwin == win) d->ptrwin = NULL;
    if (d->ptrtop == win) d->ptrtop = NULL;
    if (d->focus == win) d->focus = NULL;
    if (d->kbdtop == win) d->kbdtop = NULL;
    if (d->grab == win) d->grab = NULL;
    droptop(d, win);
    unlinkchild(win);
    freecanvas(win->can);
    free(win->title);
    free(win);
    ULK(d);
    return (0);
}

int XSelectInput(Display* d, Window w, long mask)
{
    LK(d); nextserial(d);
    ((wlwin*)w)->evtmask = mask;
    ULK(d);
    return (0);
}

int XMapWindow(Display* d, Window w)
{
    wlwin* win;
    XEvent e;

    LK(d); nextserial(d);
    win = (wlwin*)w;
    if (!win->mapped) {

        win->mapped = 1;
        if (win->parent == &d->root) {

            /* toplevel: begin the map handshake; MapNotify follows the
               first configure */
            if (!win->top) mktoplevel(d, win);

        } else {

            /* child: immediate */
            evbase(d, &e, MapNotify, win);
            e.xmap.window = (Window)win;
            e.xmap.event = (Window)win;
            enq(d, &e);
            evbase(d, &e, Expose, win);
            e.xexpose.width = win->w; e.xexpose.height = win->h;
            enq(d, &e);
            dmg(w, 0, 0, win->w, win->h);

        }

    }
    ULK(d);
    return (0);
}

int XUnmapWindow(Display* d, Window w)
{
    wlwin* win;
    XEvent e;
    wlwin* t;

    LK(d); nextserial(d);
    win = (wlwin*)w;
    if (win->mapped) {

        win->mapped = 0;
        if (win->parent == &d->root) droptop(d, win);
        else {

            t = winTop(win);
            if (t) { int ox, oy; winorg(win, &ox, &oy);
                     if (t->top) { t->top->dmg = 1; t->top->dx1 = 0; t->top->dy1 = 0;
                                   t->top->dx2 = t->w; t->top->dy2 = t->h; } }

        }
        evbase(d, &e, UnmapNotify, win);
        e.xunmap.window = (Window)win;
        e.xunmap.event = (Window)win;
        enq(d, &e);

    }
    ULK(d);
    return (0);
}

int XMoveWindow(Display* d, Window w, int x, int y)
{
    wlwin* win;
    wlwin* t;

    LK(d); nextserial(d);
    win = (wlwin*)w;
    if (win->parent == &d->root) {

        /* a toplevel does not place itself under this protocol; the call
           records the wish and nothing moves */
        win->x = x; win->y = y;

    } else {

        win->x = x; win->y = y;
        t = winTop(win);
        if (t && t->top) { /* the whole ancestor surface recomposes */

            t->top->dmg = 1; t->top->dx1 = 0; t->top->dy1 = 0;
            t->top->dx2 = t->w; t->top->dy2 = t->h;

        }

    }
    ULK(d);
    return (0);
}

static void sizebufs(Display* d, wlwin* win); /* forward */

int XResizeWindow(Display* d, Window w, unsigned int width,
                  unsigned int height)
{
    wlwin* win;
    wlwin* t;

    LK(d); nextserial(d);
    win = (wlwin*)w;
    win->w = width; win->h = height;
    sizecanvas(win, width, height);
    if (win->parent == &d->root && win->top) sizebufs(d, win);
    t = winTop(win);
    if (!t && win->top) t = win;
    if (t && t->top) {

        t->top->dmg = 1; t->top->dx1 = 0; t->top->dy1 = 0;
        t->top->dx2 = t->w; t->top->dy2 = t->h;

    }
    ULK(d);
    return (0);
}

int XMoveResizeWindow(Display* d, Window w, int x, int y,
                      unsigned int width, unsigned int height)
{
    XMoveWindow(d, w, x, y);
    return (XResizeWindow(d, w, width, height));
}

int XConfigureWindow(Display* d, Window w, unsigned int mask,
                     XWindowChanges* ch)
{
    wlwin* win;

    LK(d); nextserial(d);
    win = (wlwin*)w;
    if (mask&(CWWidth|CWHeight)) {

        ULK(d);
        XResizeWindow(d, w, mask&CWWidth? ch->width: win->w,
                            mask&CWHeight? ch->height: win->h);
        LK(d);

    }
    if (mask&CWStackMode) {

        ULK(d);
        if (ch->stack_mode == Above) XRaiseWindow(d, w);
        else XLowerWindow(d, w);
        LK(d);

    }
    ULK(d);
    return (0);
}

int XRaiseWindow(Display* d, Window w)
{
    wlwin* win;
    wlwin* t;

    LK(d); nextserial(d);
    win = (wlwin*)w;
    if (win->parent && win->parent != &d->root) {

        unlinkchild(win);
        linkchild(win->parent? win->parent: &d->root, win);
        t = winTop(win);
        if (t && t->top) {

            t->top->dmg = 1; t->top->dx1 = 0; t->top->dy1 = 0;
            t->top->dx2 = t->w; t->top->dy2 = t->h;

        }

    }
    ULK(d);
    return (0);
}

int XLowerWindow(Display* d, Window w)
{
    wlwin* win;
    wlwin* t;

    LK(d); nextserial(d);
    win = (wlwin*)w;
    if (win->parent && win->parent != &d->root) {

        unlinkchild(win);
        win->sibnext = win->parent->childs;
        win->parent->childs = win;
        t = winTop(win);
        if (t && t->top) {

            t->top->dmg = 1; t->top->dx1 = 0; t->top->dy1 = 0;
            t->top->dx2 = t->w; t->top->dy2 = t->h;

        }

    }
    ULK(d);
    return (0);
}

Status XGetWindowAttributes(Display* d, Window w, XWindowAttributes* wa)
{
    wlwin* win;
    wlwin* p;

    LK(d); nextserial(d);
    win = (wlwin*)w;
    memset(wa, 0, sizeof(XWindowAttributes));
    if (win == &d->root) {

        wa->width = d->outw; wa->height = d->outh;
        wa->map_state = IsViewable;

    } else {

        wa->x = win->x; wa->y = win->y;
        wa->width = win->w; wa->height = win->h;
        wa->map_state = IsViewable;
        for (p = win; p && p != &d->root; p = p->parent)
            if (!p->mapped) wa->map_state = IsUnmapped;

    }
    wa->depth = 24;
    wa->root = (Window)&d->root;
    ULK(d);
    return (1);
}

Status XQueryTree(Display* d, Window w, Window* root, Window* parent,
                  Window** children, unsigned int* nchildren)
{
    wlwin*  win;
    wlwin*  c;
    unsigned n;
    Window* l;

    LK(d); nextserial(d);
    win = (wlwin*)w;
    *root = (Window)&d->root;
    *parent = (Window)(win->parent? win->parent: NULL);
    n = 0;
    for (c = win->childs; c; c = c->sibnext) n++;
    l = malloc((n? n: 1)*sizeof(Window));
    n = 0;
    for (c = win->childs; c; c = c->sibnext) l[n++] = (Window)c;
    *children = l;
    *nchildren = n;
    ULK(d);
    return (1);
}

int XStoreName(Display* d, Window w, const char* name)
{
    wlwin* win;

    LK(d); nextserial(d);
    win = (wlwin*)w;
    free(win->title);
    win->title = strdup(name? name: "");
    if (win->top && win->top->xtop)
        xdg_toplevel_set_title(win->top->xtop, win->title);
    ULK(d);
    return (0);
}

int XSetIconName(Display* d, Window w, const char* name)
{ (void)d; (void)w; (void)name; return (0); }

void XSetWMNormalHints(Display* d, Window w, XSizeHints* hints)
{ (void)d; (void)w; (void)hints; }

Status XSetWMProtocols(Display* d, Window w, Atom* protocols, int count)
{ (void)d; (void)w; (void)protocols; (void)count; return (1); }

int XSetInputFocus(Display* d, Window focus, int revert_to, Time time)
{
    wlwin* nw;
    wlwin* ow;
    XEvent e;

    (void)revert_to; (void)time;
    LK(d); nextserial(d);
    nw = (focus == None || focus == (Window)&d->root)? NULL: (wlwin*)focus;
    ow = d->focus;
    if (nw != ow) {

        d->focus = nw;
        if (ow) { evbase(d, &e, FocusOut, ow); enq(d, &e); }
        if (nw) { evbase(d, &e, FocusIn, nw); enq(d, &e); }

    }
    ULK(d);
    return (0);
}

Bool XQueryPointer(Display* d, Window w, Window* root, Window* child,
                   int* rx, int* ry, int* wx, int* wy, unsigned int* mask)
{
    wlwin* win;
    int    ox, oy;

    LK(d); nextserial(d);
    win = (wlwin*)w;
    *root = (Window)&d->root;
    *child = None;
    *rx = (int)d->ptrx; *ry = (int)d->ptry;
    winorg(win, &ox, &oy);
    *wx = (int)d->ptrx-ox; *wy = (int)d->ptry-oy;
    *mask = d->modstate;
    ULK(d);
    return (d->ptrtop && winTop(win) == d->ptrtop);
}

int XGrabPointer(Display* d, Window grab, Bool owner_events,
                 unsigned int event_mask, int pointer_mode,
                 int keyboard_mode, Window confine_to, Cursor cursor,
                 Time time)
{
    (void)owner_events; (void)event_mask; (void)pointer_mode;
    (void)keyboard_mode; (void)confine_to; (void)cursor; (void)time;
    LK(d); nextserial(d);
    d->grab = (wlwin*)grab;
    ULK(d);
    return (GrabSuccess);
}

int XUngrabPointer(Display* d, Time time)
{
    (void)time;
    LK(d); nextserial(d);
    d->grab = NULL;
    ULK(d);
    return (0);
}

/*******************************************************************************

Atoms and properties

*******************************************************************************/

Atom XInternAtom(Display* d, const char* name, Bool only_if_exists)
{
    int i;

    (void)only_if_exists;
    LK(d); nextserial(d);
    for (i = 0; i < d->natom; i++)
        if (!strcmp(d->atomtab[i], name)) { ULK(d); return (DYNATOM+i); }
    if (d->natom < MAXATOM) {

        d->atomtab[d->natom] = strdup(name);
        i = d->natom++;
        ULK(d);
        return (DYNATOM+i);

    }
    ULK(d);
    return (None);
}

char* XGetAtomName(Display* d, Atom a)
{
    char* s;

    LK(d);
    if (a >= DYNATOM && a < DYNATOM+(unsigned)d->natom)
        s = strdup(d->atomtab[a-DYNATOM]);
    else if (a == XA_ATOM) s = strdup("ATOM");
    else if (a == XA_CARDINAL) s = strdup("CARDINAL");
    else s = strdup("?");
    ULK(d);
    return (s);
}

int XChangeProperty(Display* d, Window w, Atom property, Atom type,
                    int format, int mode, const unsigned char* data,
                    int nelements)
{
    (void)d; (void)w; (void)property; (void)type; (void)format;
    (void)mode; (void)data; (void)nelements;
    return (0);
}

int XGetWindowProperty(Display* d, Window w, Atom property, long off,
                       long len, Bool delete, Atom req_type,
                       Atom* actual_type, int* actual_format,
                       unsigned long* nitems, unsigned long* bytes_after,
                       unsigned char** prop)
{
    wlwin* win;
    wlwin* t;
    Atom*  al;
    unsigned long n;
    char*  nm;

    (void)off; (void)len; (void)delete; (void)req_type;
    LK(d); nextserial(d);
    win = (wlwin*)w;
    *actual_type = XA_ATOM;
    *actual_format = 32;
    *nitems = 0;
    *bytes_after = 0;
    *prop = NULL;
    nm = (property >= DYNATOM && property < DYNATOM+(unsigned)d->natom)?
         d->atomtab[property-DYNATOM]: "";
    if (!strcmp(nm, "_NET_WM_STATE")) {

        t = win->top? win: winTop(win);
        al = malloc(4*sizeof(Atom));
        n = 0;
        if (t && t->top) {

            ULK(d);
            if (t->top->activated)
                al[n++] = XInternAtom(d, "_NET_WM_STATE_FOCUSED", 1);
            if (t->top->maximized) {

                al[n++] = XInternAtom(d, "_NET_WM_STATE_MAXIMIZED_HORZ", 1);
                al[n++] = XInternAtom(d, "_NET_WM_STATE_MAXIMIZED_VERT", 1);

            }
            LK(d);

        }
        *prop = (unsigned char*)al;
        *nitems = n;

    }
    ULK(d);
    return (0); /* Success */
}

int XFree(void* data)
{ free(data); return (0); }

/*******************************************************************************

Cursors (recorded; a headless rig has no one to show them to, and the
themed-cursor pass is a later phase)

*******************************************************************************/

Cursor XCreateFontCursor(Display* d, unsigned int shape)
{ (void)d; return ((Cursor)shape+1); }

int XDefineCursor(Display* d, Window w, Cursor c)
{
    LK(d);
    ((wlwin*)w)->cursor = c;
    ULK(d);
    return (0);
}

/*******************************************************************************

Toplevel surface management

*******************************************************************************/

static void framedone(void* data, struct wl_callback* cb, uint32_t t);

static const struct wl_callback_listener frame_lis = { framedone };

static void bufrelease(void* data, struct wl_buffer* b)
{
    wltop* t = data;
    int    i;

    for (i = 0; i < 2; i++) if (t->buf[i] == b) t->bufbusy[i] = 0;
}

static const struct wl_buffer_listener buf_lis = { bufrelease };

static int mkshmfd(size_t size)
{
    int fd;

    fd = memfd_create("ami-wl", 0);
    if (fd < 0) return (-1);
    if (ftruncate(fd, size) < 0) { close(fd); return (-1); }
    return (fd);
}

/* (re)create the buffer pair at the window's size */
static void sizebufs(Display* d, wlwin* win)
{
    wltop* t;
    int    i, fd;
    size_t sz;
    struct wl_shm_pool* pool;

    t = win->top;
    if (!t) return;
    if (t->bufw == win->w && t->bufh == win->h && t->buf[0]) return;
    for (i = 0; i < 2; i++)
        if (t->buf[i]) {

            wl_buffer_destroy(t->buf[i]);
            munmap(t->bufpx[i], t->bufsz[i]);
            t->buf[i] = NULL;

        }
    t->bufw = win->w; t->bufh = win->h;
    sz = (size_t)win->w*win->h*4;
    for (i = 0; i < 2; i++) {

        fd = mkshmfd(sz);
        if (fd < 0) return;
        t->bufpx[i] = mmap(NULL, sz, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
        t->bufsz[i] = sz;
        pool = wl_shm_create_pool(d->shm, fd, sz);
        t->buf[i] = wl_shm_pool_create_buffer(pool, 0, win->w, win->h,
                                              win->w*4, WL_SHM_FORMAT_XRGB8888);
        wl_buffer_add_listener(t->buf[i], &buf_lis, t);
        wl_shm_pool_destroy(pool);
        close(fd);
        t->bufbusy[i] = 0;

    }
    /* full redraw next commit */
    t->dmg = 1; t->dx1 = 0; t->dy1 = 0; t->dx2 = win->w; t->dy2 = win->h;
}

/* recursive blit of the window tree into the composition buffer */
static void blittree(wlwin* w, uint32_t* dst, int dw, int dh, int ox, int oy,
                     int cx1, int cy1, int cx2, int cy2)
{
    wlwin*  c;
    canvas* cv;
    int     x1, y1, x2, y2, y;

    if (!w->mapped && w->parent) return;
    cv = w->can;
    if (cv) {

        x1 = ox; y1 = oy; x2 = ox+w->w; y2 = oy+w->h;
        if (x1 < cx1) x1 = cx1;
        if (y1 < cy1) y1 = cy1;
        if (x2 > cx2) x2 = cx2;
        if (y2 > cy2) y2 = cy2;
        if (x2 > x1 && y2 > y1)
            for (y = y1; y < y2; y++)
                memcpy(&dst[(size_t)y*dw+x1],
                       &cv->px[(size_t)(y-oy)*cv->w+(x1-ox)],
                       (size_t)(x2-x1)*4);

    }
    for (c = w->childs; c; c = c->sibnext)
        blittree(c, dst, dw, dh, ox+c->x, oy+c->y, cx1, cy1, cx2, cy2);
}

/* compose and commit a toplevel's damage */
static void compose(Display* d, wlwin* win)
{
    wltop* t;
    int    b;
    int    x1, y1, x2, y2;

    t = win->top;
    if (!t || !t->configured || !win->mapped || !t->dmg) return;
    if (t->fcb) { t->commitpend = 1; return; } /* frame pacing */
    sizebufs(d, win);
    b = t->curbuf;
    if (t->bufbusy[b]) b = 1-b;
    if (t->bufbusy[b]) { t->commitpend = 1; return; }
    if (!t->buf[b]) return;
    x1 = t->dx1; y1 = t->dy1; x2 = t->dx2; y2 = t->dy2;
    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 > win->w) x2 = win->w;
    if (y2 > win->h) y2 = win->h;
    /* both buffers must carry the history; compose damage into both is
       avoided by composing the union of this and the other buffer's staleness:
       simplest correct form, compose full frame when switching buffers */
    if (b != t->curbuf) { x1 = 0; y1 = 0; x2 = win->w; y2 = win->h; }
    if (x2 <= x1 || y2 <= y1) { t->dmg = 0; return; }
    blittree(win, t->bufpx[b], t->bufw, t->bufh, 0, 0, x1, y1, x2, y2);
    wl_surface_attach(t->surf, t->buf[b], 0, 0);
    wl_surface_damage(t->surf, x1, y1, x2-x1, y2-y1);
    t->fcb = wl_surface_frame(t->surf);
    t->fcbtime = nowms();
    wl_callback_add_listener(t->fcb, &frame_lis, win);
    wl_surface_commit(t->surf);
    t->bufbusy[b] = 1;
    t->curbuf = b;
    t->dmg = 0;
    t->commitpend = 0;
    /* the rig's capture path: record exactly what was committed. Written
       here, a teardown flush cannot overwrite a good frame with the
       dismantled tree */
    {
        const char* dump = getenv("AMI_WL_DUMP");
        char fn[256];
        FILE* f;
        int x, y, i;
        uint32_t p;
        wlwin* rc;

        if (dump) {

            i = 0;
            for (rc = d->root.childs; rc && rc != win; rc = rc->sibnext) i++;
            /* one file per commit: teardown commits a black frame as the
               windows dismantle, and sequencing keeps every earlier frame
               inspectable */
            snprintf(fn, sizeof(fn), "%s-%d-%04d.ppm", dump, i,
                     d->dumpseq++);
            f = fopen(fn, "w");
            if (f) {

                fprintf(f, "P6 %d %d 255\n", t->bufw, t->bufh);
                for (y = 0; y < t->bufh; y++)
                    for (x = 0; x < t->bufw; x++) {

                        p = t->bufpx[b][(size_t)y*t->bufw+x];
                        fputc(p>>16&0xff, f); fputc(p>>8&0xff, f);
                        fputc(p&0xff, f);

                    }
                fclose(f);

            }

        }
    }
}

static void framedone(void* data, struct wl_callback* cb, uint32_t tm)
{
    wlwin* win = data;
    Display* d = &thedpy;

    (void)tm;
    if (win->top && win->top->fcb == cb) win->top->fcb = NULL;
    wl_callback_destroy(cb);
    if (win->top && (win->top->commitpend || win->top->dmg)) {

        win->top->dmg = 1;
        compose(d, win);

    }
}

static void flushtops(Display* d)
{
    wlwin* c;

    for (c = d->root.childs; c; c = c->sibnext)
        if (c->top && c->top->dmg) {

            /* A compositor throttles a surface it is not showing by
               withholding frame callbacks; damage must still land
               eventually or a hidden window never updates and a
               callback-less compositor wedges everything. Past a beat,
               commit without the pacing */
            if (c->top->fcb && nowms()-c->top->fcbtime > 100) {

                wl_callback_destroy(c->top->fcb);
                c->top->fcb = NULL;

            }
            compose(d, c);

        }
    wl_display_flush(d->dpy);
}

/* xdg surface configure */
static void xsconf(void* data, struct xdg_surface* s, uint32_t serial)
{
    wlwin*   win = data;
    Display* d = &thedpy;
    wltop*   t;
    XEvent   e;

    t = win->top;
    if (!t) return;
    xdg_surface_ack_configure(s, serial);
    t->configured = 1;
    if (t->confw > 0 && t->confh > 0 &&
        (t->confw != win->w || t->confh != win->h)) {

        /* the compositor resized us; report as the window manager would */
        evbase(d, &e, ConfigureNotify, win);
        e.xconfigure.window = (Window)win;
        e.xconfigure.x = win->x; e.xconfigure.y = win->y;
        e.xconfigure.width = t->confw; e.xconfigure.height = t->confh;
        enq(d, &e);

    }
    if (!t->mapnoted) {

        t->mapnoted = 1;
        evbase(d, &e, MapNotify, win);
        e.xmap.window = (Window)win;
        e.xmap.event = (Window)win;
        enq(d, &e);
        evbase(d, &e, Expose, win);
        e.xexpose.width = win->w; e.xexpose.height = win->h;
        enq(d, &e);
        /* first content: whatever has been drawn shows now */
        t->dmg = 1; t->dx1 = 0; t->dy1 = 0; t->dx2 = win->w; t->dy2 = win->h;
        compose(d, win);

    }
}

static const struct xdg_surface_listener xsurf_lis = { xsconf };

static void xtconf(void* data, struct xdg_toplevel* xt, int32_t w, int32_t h,
                   struct wl_array* states)
{
    wlwin*   win = data;
    Display* d = &thedpy;
    wltop*   t;
    uint32_t* st;
    int      act, max;
    XEvent   e;

    t = win->top;
    if (!t) return;
    t->confw = w; t->confh = h;
    act = 0; max = 0;
    wl_array_for_each(st, states) {

        if (*st == XDG_TOPLEVEL_STATE_ACTIVATED) act = 1;
        if (*st == XDG_TOPLEVEL_STATE_MAXIMIZED) max = 1;

    }
    if (act != t->activated || max != t->maximized) {

        t->activated = act;
        t->maximized = max;
        /* the backend watches for state changes as property notifies */
        evbase(d, &e, PropertyNotify, win);
        e.xproperty.window = (Window)win;
        e.xproperty.atom = XInternAtom(d, "_NET_WM_STATE", 0);
        e.xproperty.state = PropertyNewValue;
        enq(d, &e);
        evbase(d, &e, act? FocusIn: FocusOut, win);
        enq(d, &e);

    }
}

static void xtclose(void* data, struct xdg_toplevel* xt)
{
    wlwin*   win = data;
    Display* d = &thedpy;
    XEvent   e;

    evbase(d, &e, ClientMessage, win);
    e.xclient.message_type = XInternAtom(d, "WM_PROTOCOLS", 0);
    e.xclient.format = 32;
    e.xclient.data.l[0] = XInternAtom(d, "WM_DELETE_WINDOW", 0);
    enq(d, &e);
}

static const struct xdg_toplevel_listener xtop_lis = { xtconf, xtclose };

static void mktoplevel(Display* d, wlwin* win)
{
    wltop* t;

    t = calloc(1, sizeof(wltop));
    win->top = t;
    t->surf = wl_compositor_create_surface(d->comp);
    wl_surface_set_user_data(t->surf, win);
    t->xsurf = xdg_wm_base_get_xdg_surface(d->wm, t->surf);
    xdg_surface_add_listener(t->xsurf, &xsurf_lis, win);
    t->xtop = xdg_surface_get_toplevel(t->xsurf);
    xdg_toplevel_add_listener(t->xtop, &xtop_lis, win);
    xdg_toplevel_set_title(t->xtop, win->title? win->title: "ami");
    wl_surface_commit(t->surf); /* the no-buffer commit of the handshake */
    wl_display_flush(d->dpy);
}

/*******************************************************************************

Input listeners

*******************************************************************************/

static unsigned btnmask(int b)
{
    switch (b) {

        case Button1: return (Button1Mask);
        case Button2: return (Button2Mask);
        case Button3: return (Button3Mask);

    }
    return (0);
}

/* route a pointer event: honor the grab, else hit test */
static wlwin* ptrroute(Display* d, int* x, int* y)
{
    wlwin* w;
    int    ox, oy;

    if (d->grab) {

        winorg(d->grab, &ox, &oy);
        *x = (int)d->ptrx-ox; *y = (int)d->ptry-oy;
        return (d->grab);

    }
    if (!d->ptrtop) return (NULL);
    w = hittest(d->ptrtop, (int)d->ptrx, (int)d->ptry, x, y);
    return (w);
}

static void crossevt(Display* d, wlwin* w, int type)
{
    XEvent e;
    int    ox, oy;

    if (!w) return;
    evbase(d, &e, type, w);
    winorg(w, &ox, &oy);
    e.xcrossing.x = (int)d->ptrx-ox;
    e.xcrossing.y = (int)d->ptry-oy;
    e.xcrossing.x_root = (int)d->ptrx;
    e.xcrossing.y_root = (int)d->ptry;
    e.xcrossing.time = nowms();
    e.xcrossing.state = d->modstate;
    enq(d, &e);
}

static void ptrenter(void* data, struct wl_pointer* p, uint32_t serial,
                     struct wl_surface* surf, wl_fixed_t sx, wl_fixed_t sy)
{
    Display* d = data;
    wlwin*   win;
    int      x, y;

    d->inserial = serial;
    win = surf? wl_surface_get_user_data(surf): NULL;
    d->ptrtop = win;
    d->ptrx = wl_fixed_to_double(sx);
    d->ptry = wl_fixed_to_double(sy);
    d->ptrwin = ptrroute(d, &x, &y);
    crossevt(d, d->ptrwin, EnterNotify);
}

static void ptrleave(void* data, struct wl_pointer* p, uint32_t serial,
                     struct wl_surface* surf)
{
    Display* d = data;

    (void)serial; (void)surf;
    crossevt(d, d->ptrwin, LeaveNotify);
    d->ptrtop = NULL;
    d->ptrwin = NULL;
}

static void ptrmotion(void* data, struct wl_pointer* p, uint32_t time,
                      wl_fixed_t sx, wl_fixed_t sy)
{
    Display* d = data;
    wlwin*   nw;
    int      x, y;
    XEvent   e;

    d->ptrx = wl_fixed_to_double(sx);
    d->ptry = wl_fixed_to_double(sy);
    nw = ptrroute(d, &x, &y);
    if (nw != d->ptrwin) {

        crossevt(d, d->ptrwin, LeaveNotify);
        crossevt(d, nw, EnterNotify);
        d->ptrwin = nw;

    }
    if (nw) {

        evbase(d, &e, MotionNotify, nw);
        e.xmotion.x = x; e.xmotion.y = y;
        e.xmotion.x_root = (int)d->ptrx; e.xmotion.y_root = (int)d->ptry;
        e.xmotion.state = d->modstate;
        e.xmotion.time = time;
        enq(d, &e);

    }
}

static void ptrbutton(void* data, struct wl_pointer* p, uint32_t serial,
                      uint32_t time, uint32_t button, uint32_t state)
{
    Display* d = data;
    wlwin*   w;
    wlwin*   t;
    int      x, y, b;
    XEvent   e;

    d->inserial = serial;
    /* linux button codes: BTN_LEFT 0x110, RIGHT 0x111, MIDDLE 0x112 */
    b = button == 0x110? Button1: button == 0x111? Button3:
        button == 0x112? Button2: 0;
    if (!b) return;
    w = ptrroute(d, &x, &y);
    if (state) d->modstate |= btnmask(b);
    else d->modstate &= ~btnmask(b);
    /* a press in a toplevel's declared title bar starts a compositor-side
       interactive move */
    t = d->ptrtop;
    if (state && b == Button1 && !d->grab && t && t->top &&
        t->top->titw > 0 &&
        (int)d->ptrx >= t->top->titx &&
        (int)d->ptrx < t->top->titx+t->top->titw &&
        (int)d->ptry >= t->top->tity &&
        (int)d->ptry < t->top->tity+t->top->tith) {

        xdg_toplevel_move(t->top->xtop, d->seat, serial);
        return;

    }
    if (w) {

        evbase(d, &e, state? ButtonPress: ButtonRelease, w);
        e.xbutton.button = b;
        e.xbutton.x = x; e.xbutton.y = y;
        e.xbutton.x_root = (int)d->ptrx; e.xbutton.y_root = (int)d->ptry;
        e.xbutton.state = d->modstate;
        e.xbutton.time = time;
        enq(d, &e);

    }
}

static void ptraxis(void* data, struct wl_pointer* p, uint32_t time,
                    uint32_t axis, wl_fixed_t value)
{
    Display* d = data;
    wlwin*   w;
    int      x, y, b;
    XEvent   e;

    if (axis != WL_POINTER_AXIS_VERTICAL_SCROLL) return;
    b = wl_fixed_to_double(value) < 0? Button4: Button5;
    w = ptrroute(d, &x, &y);
    if (w) {

        evbase(d, &e, ButtonPress, w);
        e.xbutton.button = b;
        e.xbutton.x = x; e.xbutton.y = y;
        e.xbutton.state = d->modstate;
        e.xbutton.time = time;
        enq(d, &e);
        e.type = ButtonRelease;
        e.xbutton.serial = d->serial;
        enq(d, &e);

    }
}

static const struct wl_pointer_listener ptr_lis = {
    ptrenter, ptrleave, ptrmotion, ptrbutton, ptraxis
};

/* keyboard */

static void kbmap(void* data, struct wl_keyboard* k, uint32_t fmt, int fd,
                  uint32_t size)
{
    Display* d = data;
    char*    map;

    if (fmt != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) { close(fd); return; }
    map = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (map != MAP_FAILED) {

        if (d->keymap) xkb_keymap_unref(d->keymap);
        if (d->xst) xkb_state_unref(d->xst);
        d->keymap = xkb_keymap_new_from_string(d->xkb, map,
                        XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
        d->xst = d->keymap? xkb_state_new(d->keymap): NULL;
        munmap(map, size);

    }
    close(fd);
}

/* keyboard focus goes to the entered toplevel unless the backend set an
   explicit focus inside it */
static void kbenter(void* data, struct wl_keyboard* k, uint32_t serial,
                    struct wl_surface* surf, struct wl_array* keys)
{
    Display* d = data;
    wlwin*   win;
    XEvent   e;

    (void)keys;
    d->inserial = serial;
    win = surf? wl_surface_get_user_data(surf): NULL;
    d->kbdtop = win;
    if (win) { evbase(d, &e, FocusIn, win); enq(d, &e); }
}

static void kbleave(void* data, struct wl_keyboard* k, uint32_t serial,
                    struct wl_surface* surf)
{
    Display* d = data;
    XEvent   e;

    (void)serial; (void)surf;
    if (d->kbdtop) { evbase(d, &e, FocusOut, d->kbdtop); enq(d, &e); }
    d->kbdtop = NULL;
    d->rptkey = 0;
    { struct itimerspec z = {0}; timerfd_settime(d->rtfd, 0, &z, NULL); }
}

/* the window key events deliver to */
static wlwin* keywin(Display* d)
{
    if (d->focus) return (d->focus);
    return (d->kbdtop);
}

static void keyevt(Display* d, int type, uint32_t xkc, uint32_t time)
{
    wlwin* w;
    XEvent e;

    w = keywin(d);
    if (!w) return;
    evbase(d, &e, type, w);
    e.xkey.keycode = xkc;
    e.xkey.state = d->modstate;
    e.xkey.time = time;
    e.xkey.x = (int)d->ptrx; e.xkey.y = (int)d->ptry;
    enq(d, &e);
}

static void kbkey(void* data, struct wl_keyboard* k, uint32_t serial,
                  uint32_t time, uint32_t key, uint32_t state)
{
    Display* d = data;
    uint32_t xkc;
    struct itimerspec its;

    d->inserial = serial;
    xkc = key+8; /* evdev to X keycode convention */
    keyevt(d, state? KeyPress: KeyRelease, xkc, time);
    /* client-side repeat: the compositor sends one press */
    if (state && d->keymap && d->rptrate > 0 &&
        xkb_keymap_key_repeats(d->keymap, xkc)) {

        d->rptkey = xkc;
        memset(&its, 0, sizeof(its));
        its.it_value.tv_sec = d->rptdelay/1000;
        its.it_value.tv_nsec = (long)(d->rptdelay%1000)*1000000;
        its.it_interval.tv_sec = 0;
        its.it_interval.tv_nsec = d->rptrate > 0? 1000000000L/d->rptrate: 0;
        timerfd_settime(d->rtfd, 0, &its, NULL);

    } else if (!state && xkc == d->rptkey) {

        struct itimerspec z = {0};
        d->rptkey = 0;
        timerfd_settime(d->rtfd, 0, &z, NULL);

    }
}

static void kbmod(void* data, struct wl_keyboard* k, uint32_t serial,
                  uint32_t dep, uint32_t lat, uint32_t lock, uint32_t grp)
{
    Display* d = data;

    (void)serial;
    if (d->xst) {

        xkb_state_update_mask(d->xst, dep, lat, lock, 0, 0, grp);
        d->modstate &= ~(ShiftMask|ControlMask|Mod1Mask|LockMask);
        if (xkb_state_mod_name_is_active(d->xst, XKB_MOD_NAME_SHIFT,
            XKB_STATE_MODS_EFFECTIVE) > 0) d->modstate |= ShiftMask;
        if (xkb_state_mod_name_is_active(d->xst, XKB_MOD_NAME_CTRL,
            XKB_STATE_MODS_EFFECTIVE) > 0) d->modstate |= ControlMask;
        if (xkb_state_mod_name_is_active(d->xst, XKB_MOD_NAME_ALT,
            XKB_STATE_MODS_EFFECTIVE) > 0) d->modstate |= Mod1Mask;
        if (xkb_state_mod_name_is_active(d->xst, XKB_MOD_NAME_CAPS,
            XKB_STATE_MODS_EFFECTIVE) > 0) d->modstate |= LockMask;

    }
}

static void kbrpt(void* data, struct wl_keyboard* k, int32_t rate,
                  int32_t delay)
{
    Display* d = data;

    d->rptrate = rate;
    d->rptdelay = delay;
}

static const struct wl_keyboard_listener kbd_lis = {
    kbmap, kbenter, kbleave, kbkey, kbmod, kbrpt
};

static void seatcaps(void* data, struct wl_seat* s, uint32_t caps)
{
    Display* d = data;

    if ((caps&WL_SEAT_CAPABILITY_POINTER) && !d->ptr) {

        d->ptr = wl_seat_get_pointer(s);
        wl_pointer_add_listener(d->ptr, &ptr_lis, d);

    }
    if ((caps&WL_SEAT_CAPABILITY_KEYBOARD) && !d->kbd) {

        d->kbd = wl_seat_get_keyboard(s);
        wl_keyboard_add_listener(d->kbd, &kbd_lis, d);

    }
}

static void seatname(void* data, struct wl_seat* s, const char* n)
{ (void)data; (void)s; (void)n; }

static const struct wl_seat_listener seat_lis = { seatcaps, seatname };

/* output */

static void outgeom(void* data, struct wl_output* o, int32_t x, int32_t y,
                    int32_t pw, int32_t ph, int32_t subpix, const char* make,
                    const char* model, int32_t transform)
{
    Display* d = data;

    (void)o; (void)x; (void)y; (void)subpix; (void)make; (void)model;
    (void)transform;
    if (pw > 0) d->outmmw = pw;
    if (ph > 0) d->outmmh = ph;
}

static void outmode(void* data, struct wl_output* o, uint32_t flags,
                    int32_t w, int32_t h, int32_t refresh)
{
    Display* d = data;

    (void)o; (void)refresh;
    if (flags&WL_OUTPUT_MODE_CURRENT) { d->outw = w; d->outh = h; }
}

static void outdone(void* data, struct wl_output* o) { (void)data; (void)o; }
static void outscale(void* data, struct wl_output* o, int32_t f)
{ (void)data; (void)o; (void)f; }

static const struct wl_output_listener out_lis = {
    outgeom, outmode, outdone, outscale
};

/* shell ping */
static void wmping(void* data, struct xdg_wm_base* wm, uint32_t serial)
{ (void)data; xdg_wm_base_pong(wm, serial); }

static const struct xdg_wm_base_listener wm_lis = { wmping };

/* registry */
static void regglobal(void* data, struct wl_registry* reg, uint32_t name,
                      const char* iface, uint32_t ver)
{
    Display* d = data;

    if (!strcmp(iface, wl_compositor_interface.name))
        d->comp = wl_registry_bind(reg, name, &wl_compositor_interface, 4);
    else if (!strcmp(iface, wl_shm_interface.name))
        d->shm = wl_registry_bind(reg, name, &wl_shm_interface, 1);
    else if (!strcmp(iface, xdg_wm_base_interface.name)) {

        d->wm = wl_registry_bind(reg, name, &xdg_wm_base_interface, 1);
        xdg_wm_base_add_listener(d->wm, &wm_lis, d);

    } else if (!strcmp(iface, wl_seat_interface.name)) {

        d->seat = wl_registry_bind(reg, name, &wl_seat_interface, 5);
        wl_seat_add_listener(d->seat, &seat_lis, d);

    } else if (!strcmp(iface, wl_output_interface.name) && !d->out) {

        d->out = wl_registry_bind(reg, name, &wl_output_interface, 2);
        wl_output_add_listener(d->out, &out_lis, d);

    }
}

static void regremove(void* data, struct wl_registry* reg, uint32_t name)
{ (void)data; (void)reg; (void)name; }

static const struct wl_registry_listener reg_lis = { regglobal, regremove };

/*******************************************************************************

Connection management, pump, and the event read calls

*******************************************************************************/

Display* XOpenDisplay(const char* name)
{
    Display* d;
    struct epoll_event ev;
    pthread_mutexattr_t ma;

    if (dpyopen) return (&thedpy);
    d = &thedpy;
    memset(d, 0, sizeof(thedpy));
    d->dpy = wl_display_connect(name);
    if (!d->dpy) return (NULL);
    pthread_mutexattr_init(&ma);
    pthread_mutexattr_settype(&ma, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&d->lk, &ma);
    d->root.tag = ot_win;
    d->root.mapped = 1;
    d->outw = 1920; d->outh = 1080;   /* until the output reports */
    d->outmmw = 508; d->outmmh = 285; /* 96 dpi, the desktop fiction */
    d->reg = wl_display_get_registry(d->dpy);
    wl_registry_add_listener(d->reg, &reg_lis, d);
    wl_display_roundtrip(d->dpy); /* globals */
    wl_display_roundtrip(d->dpy); /* seat caps, keymap, output modes */
    /* An output reporting nonsense physical size (headless compositors
       report millimeters equal to pixels; broken EDID reports zeros) would
       shrink or explode every font. Outside plausible density, keep the
       96 dpi desktop fiction */
    if (d->outmmw < 1 || d->outmmh < 1 ||
        d->outw*25/d->outmmw < 50 || d->outw*25/d->outmmw > 500 ||
        d->outh*25/d->outmmh < 50 || d->outh*25/d->outmmh > 500) {

        d->outmmw = d->outw*254/960; /* mm = px * 25.4 / 96 */
        d->outmmh = d->outh*254/960;

    }
    if (!d->comp || !d->shm || !d->wm) {

        fprintf(stderr, "wlshim: compositor lacks required globals\n");
        return (NULL);

    }
    d->xkb = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    d->rptdelay = 400; d->rptrate = 25;
    d->injfd = -1;
    /* the combined wait fd */
    d->epfd = epoll_create1(EPOLL_CLOEXEC);
    d->evfd = eventfd(0, EFD_NONBLOCK|EFD_CLOEXEC);
    d->rtfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK|TFD_CLOEXEC);
    memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLIN;
    ev.data.fd = wl_display_get_fd(d->dpy);
    epoll_ctl(d->epfd, EPOLL_CTL_ADD, wl_display_get_fd(d->dpy), &ev);
    ev.data.fd = d->evfd;
    epoll_ctl(d->epfd, EPOLL_CTL_ADD, d->evfd, &ev);
    ev.data.fd = d->rtfd;
    epoll_ctl(d->epfd, EPOLL_CTL_ADD, d->rtfd, &ev);
    dpyopen = 1;
    return (d);
}

int XCloseDisplay(Display* d)
{
    if (!dpyopen) return (0);
    wl_display_disconnect(d->dpy);
    close(d->epfd);
    close(d->evfd);
    close(d->rtfd);
    dpyopen = 0;
    return (0);
}

int wlshim_fd(Display* d) { return (d->epfd); }
int wlshim_screen(Display* d) { (void)d; return (0); }
int wlshim_width(Display* d) { return (d->outw); }
int wlshim_height(Display* d) { return (d->outh); }
int wlshim_widthmm(Display* d) { return (d->outmmw); }
int wlshim_heightmm(Display* d) { return (d->outmmh); }
int wlshim_depth(Display* d) { (void)d; return (24); }
Window wlshim_root(Display* d) { return ((Window)&d->root); }
Visual* wlshim_visual(Display* d) { (void)d; return (NULL); }
unsigned long wlshim_nextreq(Display* d) { return (d->serial+1); }

Status XInitThreads(void) { return (1); }

XErrorHandler XSetErrorHandler(XErrorHandler h)
{
    XErrorHandler o;

    o = thedpy.errh;
    thedpy.errh = h;
    return (o);
}

int XGetErrorText(Display* d, int code, char* buf, int len)
{
    (void)d;
    snprintf(buf, len, "wlshim error %d", code);
    return (0);
}

int XSynchronize(Display* d, Bool onoff)
{ (void)d; (void)onoff; return (0); }

/* Rig input injection: AMI_WL_INPUT names a fifo; lines arriving there
   synthesize input as if the seat delivered it, which is what lets a
   compositor without virtual input protocols (headless weston) drive
   interactive tests. Commands:
       key <xkeycode>        press and release
       keydown <xkeycode>    press only
       keyup <xkeycode>      release only
       move <x> <y>          pointer motion, surface coordinates
       btn <1|2|3> <x> <y>   move, press, release */
static void injline(Display* d, char* ln)
{
    int a, x, y;
    wlwin* c;

    /* a pointer target: the first mapped toplevel */
    if (!d->ptrtop) {

        for (c = d->root.childs; c; c = c->sibnext)
            if (c->mapped && c->top) { d->ptrtop = c; break; }

    }
    if (!d->kbdtop) d->kbdtop = d->ptrtop;
    if (sscanf(ln, "key %d", &a) == 1) {

        keyevt(d, KeyPress, a, nowms());
        keyevt(d, KeyRelease, a, nowms());

    } else if (sscanf(ln, "keydown %d", &a) == 1)
        keyevt(d, KeyPress, a, nowms());
    else if (sscanf(ln, "keyup %d", &a) == 1)
        keyevt(d, KeyRelease, a, nowms());
    else if (sscanf(ln, "move %d %d", &x, &y) == 2)
        ptrmotion(d, NULL, (uint32_t)nowms(), wl_fixed_from_int(x),
                  wl_fixed_from_int(y));
    else if (sscanf(ln, "btn %d %d %d", &a, &x, &y) == 3) {

        int bc = a == 1? 0x110: a == 2? 0x112: 0x111;

        ptrmotion(d, NULL, (uint32_t)nowms(), wl_fixed_from_int(x),
                  wl_fixed_from_int(y));
        ptrbutton(d, NULL, 0, (uint32_t)nowms(), bc, 1);
        ptrbutton(d, NULL, 0, (uint32_t)nowms(), bc, 0);

    }
}

static void injpoll(Display* d)
{
    static char buf[256];
    static int  len;
    const char* fn;
    char        c;
    struct epoll_event ev;

    if (d->injfd < 0) {

        fn = getenv("AMI_WL_INPUT");
        if (!fn) return;
        d->injfd = open(fn, O_RDONLY|O_NONBLOCK);
        if (d->injfd < 0) return;
        memset(&ev, 0, sizeof(ev));
        ev.events = EPOLLIN;
        ev.data.fd = d->injfd;
        epoll_ctl(d->epfd, EPOLL_CTL_ADD, d->injfd, &ev);

    }
    while (read(d->injfd, &c, 1) == 1) {

        if (c == '\n') { buf[len] = 0; len = 0; injline(d, buf); }
        else if (len < (int)sizeof(buf)-1) buf[len++] = c;

    }
}

/* drain wayland traffic and timers without blocking; the wake fds clear as
   a side effect. Callers hold the lock */
static void pump(Display* d)
{
    struct pollfd pf;
    uint64_t exp;
    int      i, n;

    if (!d->dpy) return;
    injpoll(d); /* rig-injected input, when enabled */
    /* the prepare-read protocol: we are the only reader thread by
       construction (the backend's event loop), but the protocol keeps us
       honest against future queue users */
    while (wl_display_prepare_read(d->dpy) != 0)
        wl_display_dispatch_pending(d->dpy);
    wl_display_flush(d->dpy);
    pf.fd = wl_display_get_fd(d->dpy);
    pf.events = POLLIN;
    if (poll(&pf, 1, 0) > 0) wl_display_read_events(d->dpy);
    else wl_display_cancel_read(d->dpy);
    wl_display_dispatch_pending(d->dpy);
    /* clear the queue wake */
    { uint64_t c; while (read(d->evfd, &c, 8) > 0); }
    /* key repeat expirations synthesize presses */
    n = 0;
    if (read(d->rtfd, &exp, 8) == 8 && d->rptkey) n = (int)exp;
    for (i = 0; i < n && i < 10; i++)
        keyevt(d, KeyPress, d->rptkey, nowms());
    /* opportunistic commit of deferred damage */
    flushtops(d);
}

void wlshim_pump(Display* d)
{
    LK(d);
    pump(d);
    ULK(d);
}

int XFlush(Display* d)
{
    LK(d);
    flushtops(d);
    ULK(d);
    return (0);
}

int XSync(Display* d, Bool discard)
{
    wlwin* c;
    int    pend, i;

    (void)discard;
    /* a sync completes the round trip: pump until every toplevel's damage
       has been committed (frame callbacks release deferred commits), with
       a bound so a stalled compositor cannot wedge the caller */
    for (i = 0; i < 200; i++) {

        LK(d);
        flushtops(d);
        pump(d);
        pend = 0;
        for (c = d->root.childs; c; c = c->sibnext)
            if (c->top && (c->top->dmg || c->top->commitpend)) pend = 1;
        ULK(d);
        if (!pend) break;
        usleep(1000);

    }
    return (0);
}

int XPending(Display* d)
{
    int n;
    evq* p;

    LK(d);
    pump(d);
    n = 0;
    for (p = d->eqh; p; p = p->next) n++;
    ULK(d);
    return (n);
}

int XNextEvent(Display* d, XEvent* e)
{
    int got;

    for (;;) {

        LK(d);
        pump(d);
        got = deq(d, e);
        ULK(d);
        if (got) return (0);
        usleep(1000);

    }
}

int XPeekEvent(Display* d, XEvent* e)
{
    for (;;) {

        LK(d);
        pump(d);
        if (d->eqh) { *e = d->eqh->e; ULK(d); return (0); }
        ULK(d);
        usleep(1000);

    }
}

Bool XCheckTypedEvent(Display* d, int type, XEvent* e)
{
    evq** pp;
    evq*  p;

    LK(d);
    pump(d);
    pp = &d->eqh;
    while (*pp) {

        if ((*pp)->e.type == type) {

            p = *pp;
            *e = p->e;
            *pp = p->next;
            if (d->eqt == p) { d->eqt = NULL; for (p = d->eqh; p; p = p->next) d->eqt = p; }
            else p->next = d->eqf, d->eqf = p;
            ULK(d);
            return (True);

        }
        pp = &(*pp)->next;

    }
    ULK(d);
    return (False);
}

Bool XCheckTypedWindowEvent(Display* d, Window w, int type, XEvent* e)
{
    evq** pp;
    evq*  p;

    LK(d);
    pump(d);
    pp = &d->eqh;
    while (*pp) {

        if ((*pp)->e.type == type && (*pp)->e.xany.window == w) {

            p = *pp;
            *e = p->e;
            *pp = p->next;
            if (d->eqt == p) { d->eqt = NULL; for (p = d->eqh; p; p = p->next) d->eqt = p; }
            else p->next = d->eqf, d->eqf = p;
            ULK(d);
            return (True);

        }
        pp = &(*pp)->next;

    }
    ULK(d);
    return (False);
}

Status XSendEvent(Display* d, Window w, Bool propagate, long mask, XEvent* e)
{
    XEvent c;

    (void)propagate; (void)mask;
    LK(d); nextserial(d);
    c = *e;
    c.xany.serial = d->serial;
    c.xany.send_event = True;
    c.xany.window = w;
    enq(d, &c);
    ULK(d);
    return (1);
}

KeySym XLookupKeysym(XKeyEvent* e, int index)
{
    Display* d = &thedpy;
    const xkb_keysym_t* syms;
    int n;

    /* a seat with no keyboard (headless) never delivered a keymap; rig
       injection still needs translation, so fall back to the default
       rules (pc105/us) */
    if (!d->keymap && d->xkb) {

        struct xkb_rule_names rn;

        memset(&rn, 0, sizeof(rn));
        d->keymap = xkb_keymap_new_from_names(d->xkb, &rn,
                                              XKB_KEYMAP_COMPILE_NO_FLAGS);
        if (d->keymap) d->xst = xkb_state_new(d->keymap);

    }
    if (!d->keymap) return (0);
    n = xkb_keymap_key_get_syms_by_level(d->keymap, e->keycode, 0, index,
                                         &syms);
    if (n < 1) return (0);
    return (syms[0]);
}

/*******************************************************************************

Wayland specific hooks

*******************************************************************************/

void wlshim_titlebar(Display* d, Window w, int x, int y, int wd, int ht)
{
    wlwin* win;

    LK(d);
    win = (wlwin*)w;
    if (win->top) {

        win->top->titx = x; win->top->tity = y;
        win->top->titw = wd; win->top->tith = ht;

    }
    ULK(d);
}

int wlshim_dump(Display* d, Window w, const char* fn)
{
    wlwin*    win;
    uint32_t* buf;
    FILE*     f;
    int       x, y;
    uint32_t  p;

    LK(d);
    win = (wlwin*)w;
    buf = calloc((size_t)win->w*win->h, 4);
    blittree(win, buf, win->w, win->h, 0, 0, 0, 0, win->w, win->h);
    ULK(d);
    f = fopen(fn, "w");
    if (!f) { free(buf); return (-1); }
    fprintf(f, "P6 %d %d 255\n", win->w, win->h);
    for (y = 0; y < win->h; y++)
        for (x = 0; x < win->w; x++) {

            p = buf[(size_t)y*win->w+x];
            fputc(p>>16&0xff, f); fputc(p>>8&0xff, f); fputc(p&0xff, f);

        }
    fclose(f);
    free(buf);
    return (0);
}
