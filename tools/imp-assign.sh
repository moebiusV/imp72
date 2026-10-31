#!/bin/sh
# imp-assign.sh [-t] FILE...: tools/imp-assign.imp, built first (build/imp-assign) by the compiler of the tree if it is not there or is older than its source.  Without -t each file is
# rewritten with := for _ (the tape's text to IMP72's); with -t the tape's text of each file (:= back to _) goes to the standard output.  The caller's directory is kept.
root=$(cd "$(dirname "$0")/.." && pwd)
b=$root/build/imp-assign
if [ ! -x "$b" ] || [ -n "$(find "$root/tools/imp-assign.imp" -newer "$b" 2>/dev/null)" ]; then
    c=${IMP72:-$root/build/llvm-ir2/imp72stage-llvm-ir}
    [ -x "$c" ] || c=$root/stage0/imp72stage0                      # (the seed reads the same language)
    [ -x "$c" ] || { echo "imp-assign: no compiler built (make first)" >&2; exit 1; }
    mkdir -p "$root/build"
    env -u IMP72SYNTAX -u IMP72FFI -u IMP72LIB "$c" -o "$b.new" "$root/tools/imp-assign.imp" >"$root/build/imp-assign.log" 2>&1 || { cat "$root/build/imp-assign.log" >&2; echo "imp-assign: cannot build tools/imp-assign.imp" >&2; exit 1; }
    mv "$b.new" "$b"
fi
exec "$b" "$@"
