#!/bin/sh
# tape.sh [check]: tape/ is decus/ (the DECUS 10-198 tape as received) with the text of IMP72 as it is now: assignment is := and not _ (tools/imp-assign.py).  The 22 .imc files are made from
# decus/'s; the rest of tape/ is not (README.md).  With `check` nothing is written: it fails if tape/ is not what the translation of decus/ is.  Run from the top of the tree.
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root" || exit 1
py=
for c in ${PYTHON:-} python3 python3.13 python3.12 python3.11 python3.10 python; do command -v "$c" >/dev/null 2>&1 && { py=$c; break; }; done
[ -n "$py" ] || { echo "tape: no python: skipped"; exit 0; }
d=tape
[ "$1" = check ] && { d=build/tape-check; rm -rf $d; }
mkdir -p $d
for f in decus/*.imc; do cp "$f" $d/; done
$py tools/imp-assign.py $d/*.imc
if [ "$1" = check ]; then
    for f in decus/*.imc; do cmp -s $d/$(basename $f) tape/$(basename $f) || { echo "FAILED: tape: tape/$(basename $f) is not the translation of $f" >&2; exit 1; }; done
    echo ok
fi
