#!/bin/sh
# Oracle goldens: the programs named in tests/oracle/LIST (in tests/oracle/ or tests/native/)
# were compiled and run by the original compiler (IMP 1.5 on TOPS-10, tools/oracle-run.py);
# their output is in tests/golden/oracle/NAME.out.  Here imp72stage0 compiles the same programs
# and the output must be the same.  tools/oracle-update.sh makes the goldens again.
root=$(cd "$(dirname "$0")/../.." && pwd)
cd "$root" || exit 1
mkdir -p build/oracle-check
fail=0
for n in $(cat tests/oracle/LIST); do
    p=tests/oracle/$n.imp; [ -f "$p" ] || p=tests/native/$n.imp
    echo "== oracle $n"
    CC1=${CC1:-$root/build/pcc/cc/ccom/cc1}; export CC1
    stage0/imp72stage0 ${IMP72_FLAGS} -o build/oracle-check/$n "$p" \
        || { echo "FAILED: $n: imp72stage0" >&2; fail=1; continue; }
    (cd build/oracle-check && ./$n > $n.got) || { echo "FAILED: $n: run" >&2; fail=1; continue; }
    # REAL is IEEE 754 here, the original's 27 bit floating point there: where the two give different numbers
    # (tests/golden/ieee) the reviewed IEEE output is the golden; everything else is what the original printed
    g=tests/golden/oracle/$n.out; [ -f tests/golden/ieee/$n.out ] && g=tests/golden/ieee/$n.out
    sed 's/[ 	]*$//' build/oracle-check/$n.got | cmp -s - $g \
        || { diff build/oracle-check/$n.got $g; echo "FAILED: $n: differs from the oracle" >&2; fail=1; continue; }
    echo ok
done
exit $fail
