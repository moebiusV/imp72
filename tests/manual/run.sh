#!/bin/sh
# docs/manual/imp72.md and imp72.7 are what tools/runoff2md.py and tools/runoff2mdoc.py make of decus/imp.rno
# (with tools/runoff-fixes.txt): regenerate and compare.  mandoc, if present, must find no errors in the page.
root=$(cd "$(dirname "$0")/../.." && pwd)
cd "$root" || exit 1
command -v python3 >/dev/null 2>&1 || { echo "manual check skipped: no python3"; exit 0; }
mkdir -p build
fail=0
python3 tools/runoff2md.py decus/imp.rno > build/imp72.md.new
python3 tools/runoff2mdoc.py decus/imp.rno > build/imp72.7.new
for f in imp72.md imp72.7; do
    cmp -s build/$f.new docs/manual/$f || { diff build/$f.new docs/manual/$f | head -5
        echo "FAILED: docs/manual/$f is not what its converter makes" >&2; fail=1; }
done
if command -v mandoc >/dev/null 2>&1; then
    mandoc -Tlint -Werror docs/manual/imp72.7 >/dev/null 2>build/mandoc.err || { head build/mandoc.err; echo "FAILED: mandoc" >&2; fail=1; }
fi
[ $fail = 0 ] && echo ok
exit $fail
