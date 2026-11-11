#!/bin/sh
# imptool.sh NAME [ARG...]: run the tool tools/NAME.imp (and tools/NAME-*.imp, if there are any), an IMP program, built first (build/tools/NAME) by the compiler of the tree if it is not there or is older than its source or than the
# compiler.  The tools of this tree are IMP, not Python or shell: the text they handle is IMP's to transform.  The caller's directory and standard files are kept; the build log is
# build/tools/NAME.log (shown when the build fails).  IMP=path names another compiler.  Run from anywhere.
root=$(cd "$(dirname "$0")/.." && pwd)
name=$1; shift
[ -n "$name" ] && [ -f "$root/tools/$name.imp" ] || { echo "imptool: no tools/$name.imp" >&2; exit 2; }
b=$root/build/tools/$name
c=${IMP:-$root/build/llvm-ir2/impstage-llvm-ir}
[ -x "$c" ] || c=$root/stage0/impstage0                      # (the seed reads the same language)
[ -x "$c" ] || { echo "imptool: no compiler built (make first)" >&2; exit 1; }
srcs=$root/tools/$name.imp
for f in "$root/tools/$name"-*.imp; do [ -f "$f" ] && srcs="$srcs $f"; done      # (a tool too long for one file -- the compiler names 2047 registers in a file -- is NAME.imp and NAME-*.imp, compiled together)
if [ ! -x "$b" ] || [ -n "$(find $srcs "$c" -newer "$b" 2>/dev/null)" ]; then
    mkdir -p "$root/build/tools"
    env -u IMPSYNTAX -u IMPFFI -u IMPLIB "$c" -o "$b.new" $srcs >"$b.log" 2>&1 || { cat "$b.log" >&2; echo "imptool: cannot build tools/$name.imp" >&2; exit 1; }
    mv "$b.new" "$b"
fi
exec "$b" "$@"
