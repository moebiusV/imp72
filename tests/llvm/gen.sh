#!/bin/sh
# The module that tools/llvm.sh imp writes for the LLVM and the lld of the build (src/llvm.imc, llvmgen) in each of its shapes: lld as a library or not.  Only the shape of the build at hand is built by make; a mistake in another (a semicolon between two routines, say) shows only on the system that has it (OpenBSD has no lld library).
# Each is compiled by the seed.  Run from the top of the tree (make check does).
root=$(cd "$(dirname "$0")/../.." && pwd)
cd "$root" || exit 1
sh tools/llvm.sh imp > /dev/null 2>&1 && [ -n "$(sh tools/llvm.sh cflags)" ] || { echo "llvm gen test skipped: no LLVM"; exit 0; }
seed=stage0/impstage0
[ -x $seed ] || { echo "llvm gen test skipped: no $seed"; exit 0; }
out=build/llvmgen
rm -rf $out; mkdir -p $out
endf=$(sh tools/endian.sh)
fail=0
echo "== llvm gen"
for lld in yes no; do
    f=$out/gen-$lld.imc
    WITH_LLD=$lld sh tools/llvm.sh imp > $f
    IMPMODULE=llvmgen env -u IMPSYNTAX -u IMPFFI -u IMPLIB $seed -c -o $out/gen-$lld.o -C $endf -C src/impsyn.imc $f > $out/gen-$lld.log 2>&1 \
        || { echo "FAILED: llvm gen: lld $lld" >&2; cat $out/gen-$lld.log >&2; fail=1; }
done
[ $fail = 0 ] && echo ok
exit $fail
