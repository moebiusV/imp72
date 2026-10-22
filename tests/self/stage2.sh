#!/bin/sh
# Stage 2 test, the fixed point: tools/stage2.sh compiles the compiler's modules with stage 1 and they must be the same
# objects stage 0 made; imp72s2, linked from them, must write the same IR as stage 0 for the native tests.
root=$(cd "$(dirname "$0")/../.." && pwd)
cd "$root" || exit 1
export CC1=${CC1:-$root/build/pcc/cc/ccom/cc1}
tools/stage1.sh >/dev/null 2>&1; rc=$?
[ $rc = 77 ] && { echo "stage2 test skipped: no objcopy"; exit 0; }
[ $rc = 0 ] || { echo "FAILED: stage1 build" >&2; exit 1; }
tools/stage2.sh >build/stage2.log 2>&1 || { cat build/stage2.log >&2; echo "FAILED: stage 2" >&2; exit 1; }
out=build/stage2/ir
mkdir -p $out
fail=0
for f in tests/native/*.imp; do
    n=$(basename $f .imp)
    x=; [ $n = ffi ] && x=lib/ffi.imc
    stage0/imp72s0 -c $out/$n.0 decus/syntax.imc $x $f >/dev/null 2>&1
    build/stage2/imp72s2 -c $out/$n.2 decus/syntax.imc $x $f >/dev/null 2>&1
    cmp -s $out/$n.0 $out/$n.2 && [ -s $out/$n.0 ] || { echo "DIFFERENT: $n (stage 2)" >&2; fail=1; }
done
[ $fail = 0 ] && echo ok
exit $fail
