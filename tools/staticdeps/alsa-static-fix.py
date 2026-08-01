#!/usr/bin/env python3
################################################################################
#
# Apply the ALSA static fix to src/conf.c.
#
# The one change a static link requires: a config hook whose module cannot
# be loaded is tolerated when the hook sets 'errors false'
# (snd_config_hooks_call). The distribution's 99-pulse.conf hook loads a
# shared module, which a static binary cannot; without this the failure
# poisons the whole ALSA configuration and no device opens.
#
# This is a semantic transform rather than a context diff, so it survives
# the cosmetic drift a diff does not (upstream has already renamed its
# error macro under the same code). It anchors on the parts of
# snd_config_hooks_call that have not changed between alsa-lib 1.2.2 and
# current master: the function name, the "Cannot open shared library"
# failure block, and the call guarded by err >= 0 after the _err label.
# If any anchor is missing or ambiguous, it fails loudly and the
# dependency build stops: that is the signal the fix needs a fresh look
# against the new source, exactly as a rejected patch hunk would be.
#
# Usage: alsa-static-fix.py <path-to-alsa-lib-source>/src/conf.c
#
################################################################################

import re
import sys

if len(sys.argv) != 2:
    print("usage: alsa-static-fix.py <conf.c>", file=sys.stderr)
    sys.exit(1)
fn = sys.argv[1]
src = open(fn).read()

def fail(what):
    print("alsa-static-fix: anchor not found or ambiguous: "+what,
          file=sys.stderr)
    print("alsa-static-fix: the fix needs review against this alsa-lib "
          "version", file=sys.stderr)
    sys.exit(1)

# bound the function: from its definition to the next line-start brace close
m = re.search(r'static int snd_config_hooks_call\(', src)
if not m: fail("snd_config_hooks_call")
fstart = m.start()
m = re.search(r'\n\}\n', src[fstart:])
if not m: fail("snd_config_hooks_call end")
fend = fstart+m.end()
body = src[fstart:fend]

# 1. read the hook's errors flag after the declaration of err
anchor = "\tint err;\n"
if body.count(anchor) != 1: fail("declaration of err")
body = body.replace(anchor, anchor+"""\tint errors = 1;

\t/* A hook whose module cannot be loaded is tolerated when the hook
\t   sets 'errors false': in a static build the module is unavailable
\t   by construction, and the hook must degrade as if absent. */
\terr = snd_config_search(config, "errors", &c);
\tif (err >= 0) {
\t\terrors = snd_config_get_bool(c);
\t\tif (errors < 0)
\t\t\terrors = 1;
\t}
""", 1)

# 2. tolerate the two load failure modes. The error macro name and
#    argument shape are the parts that drift; the structure is the anchor.
pat = re.compile(
    r'if \(!h\) \{\n'
    r'\t\t(\w+\([^;]*Cannot open shared library[^;]*\);)\n'
    r'\t\terr = -ENOENT;\n'
    r'\t\} else if \(!func\) \{\n'
    r'\t\t(\w+\([^;]*\);)\n'
    r'\t\tsnd_dlclose\(h\);\n'
    r'\t\terr = -ENXIO;\n'
    r'\t\}')
if len(pat.findall(body)) != 1: fail("load failure blocks")
body = pat.sub(
    'if (!h) {\n'
    '\t\tif (errors) {\n'
    '\t\t\t\\1\n'
    '\t\t\terr = -ENOENT;\n'
    '\t\t}\n'
    '\t} else if (!func) {\n'
    '\t\tif (errors)\n'
    '\t\t\t\\2\n'
    '\t\tsnd_dlclose(h);\n'
    '\t\terr = errors ? -ENXIO : 0;\n'
    '\t}', body, 1)

# 3. a tolerated failure skips the call: err can now be clean with no
#    function, and the call site must not go through NULL
i = body.find("_err:")
if i < 0: fail("_err label")
anchor = "\tif (err >= 0) {"
if body.count(anchor, i) < 1: fail("call guard after _err")
j = body.index(anchor, i)
body = body[:j]+"\tif (err >= 0 && func) {"+body[j+len(anchor):]

open(fn, 'w').write(src[:fstart]+body+src[fend:])
print("alsa-static-fix: applied to "+fn)
