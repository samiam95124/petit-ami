"""Generate doc/remote_catalog.tex from include/graph_remote.h.

The header is the normative definition of the protocol: the message ids and
the payload each carries. This reads the catalog enum and writes the rows
the paper and the manual's protocol chapter print, so the printed catalog
cannot drift from the header without someone noticing.

    python3 doc/mkremotecatalog.py            check against the file on disk
    python3 doc/mkremotecatalog.py --write    write it

Payload text comes from the comment on each entry, either the block comment
above it or the trailing one beside it. An arrow, "->", becomes the LaTeX
arrow; the rest is escaped.
"""
import re, sys, os

HDR = "include/graph_remote.h"
OUT = "doc/remote_catalog.tex"

def esc(s):
    """LaTeX out of C comment text"""
    s = s.replace("\\", r"\textbackslash{}")
    for a, b in (("_", r"\_"), ("&", r"\&"), ("%", r"\%"), ("#", r"\#"),
                 ("{", r"\{"), ("}", r"\}"), ("$", r"\$")):
        s = s.replace(a, b)
    s = s.replace("->", r"$\rightarrow$")
    return s

def catalog(hdr):
    """(name, id, payload) for every message, in the header's own order"""
    src = open(hdr).read()
    blk = re.search(r'typedef enum \{(.*?)\}\s*gr_msg;', src, re.S)
    if not blk:                      # the enum names differ; take the one with the ids
        blk = max(re.finditer(r'typedef enum \{(.*?)\}\s*gr_[a-z]+;', src, re.S),
                  key=lambda m: m.group(1).count("GR_M"))
    body = blk.group(1)
    consts, out, val, pend = {}, [], 0, []
    i = 0
    while i < len(body):
        # a block comment: either documentation for what follows, or a divider
        if body.startswith("/*", i):
            j = body.index("*/", i) + 2
            txt = body[i:j]
            if txt.startswith("/**"):
                pend = [" ".join(txt[3:-2].split())]
            i = j
            continue
        # the comma is optional: the last entry of an enum has none, and
        # requiring it drops that entry without a word
        m = re.compile(r'\b(GR_[A-Z0-9_]+)\s*(?:=\s*([^,/\n]+))?\s*,?').match(body, i)
        if m:
            name, expr = m.group(1), (m.group(2) or "").strip()
            if expr:
                e = expr
                for k, v in consts.items(): e = e.replace(k, str(v))
                try: val = eval(e, {}, {})
                except Exception: pass
            consts[name] = val
            i = m.end()
            # a trailing comment on the same line belongs to this entry
            rest = body[i:body.index("\n", i) if "\n" in body[i:] else len(body)]
            tm = re.match(r'\s*/\*(.*?)\*/', rest, re.S)
            trail = None
            if tm:
                trail = " ".join(tm.group(1).split())
                i += tm.end()
            elif re.match(r'\s*/\*', rest):          # a trailing comment that wraps
                j = body.index("*/", i) + 2
                trail = " ".join(body[body.index("/*", i)+2:j-2].split())
                i = j
            if name.startswith("GR_M"):
                txt = trail if trail else (pend[0] if pend else "")
                # an entry the header leaves silent takes no payload
                out.append((name, val, txt if txt else "(no payload)"))
            pend = []
            val += 1
            continue
        i += 1
    # nothing may be dropped in silence: the enum's own count governs
    named = len(set(re.findall(r'\bGR_M[A-Z0-9_]+\b(?=\s*(?:=|,))', body)))
    if len(out) != named:
        raise SystemExit("catalog: read %d messages, the enum names %d"
                         % (len(out), named))
    # two messages on one id means a value was misread, not that the header
    # says so: this is the check that catches a parse going quietly wrong
    ids = [v for _, v, _ in out]
    if len(set(ids)) != len(ids):
        dup = sorted(set(v for v in ids if ids.count(v) > 1))[:5]
        raise SystemExit("catalog: ids repeat, which the header does not: %s" % dup)
    return out

def rows(cat):
    out = []
    base = None
    for name, val, payload in cat:
        if base is None and val >= 8192:
            base = val
            out.append(r"\hline")
            out.append(r"\multicolumn{3}{@{}l}{\textbf{The sound partition "
                       r"(base 8192)}} \\")
            out.append(r"\hline")
        out.append(r"\texttt{%s} & %d & %s \\" % (esc(name), val, esc(payload)))
    return out

def main():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    os.chdir(root)
    cat = catalog(HDR)
    text = ("% Generated from include/graph_remote.h: the message catalog rows.\n"
            "% Regenerate with doc/mkremotecatalog.py; the header is normative.\n"
            + "\n".join(rows(cat)) + "\n")
    if "--write" in sys.argv:
        open(OUT, "w").write(text)
        print("wrote %s, %d messages" % (OUT, len(cat)))
    else:
        have = open(OUT).read()
        if have == text: print("%s is current, %d messages" % (OUT, len(cat)))
        else:
            print("%s differs from the header" % OUT)
            import difflib
            d = list(difflib.unified_diff(have.split("\n"), text.split("\n"),
                                          "on disk", "from the header", lineterm=""))
            print("\n".join(d[:40]))
            sys.exit(1)

main()
