#!/bin/sh
# The LLVM IR writer in IMP (src/ir2ll.imc, src/ir2llx.imc) against the C it was translated from (tools/ir2ll.c, which the seed links): the IR of the native tests, the oracle
# programs, the modules of the compiler and a fuzzer (tools/ir2ll-fuzz.py: every operator, type and directive) goes through both, for pointers of 64 and of 32 bits, in the byte order of the host and in the other, and what they write
# must be the same, byte for byte.  The IMP one is taken as stage 1 (the seed) and stage 2 (stage 1) compiled it, which are the objects tools/direct.sh left in build/llvm-ir1 and build/llvm-ir2.
root=$(cd "$(dirname "$0")/../.." && pwd)
cd "$root" || exit 1
for s in llvm-ir1 llvm-ir2; do
    [ -f build/$s/ir2ll.imp2.o ] && [ -f build/$s/ir2llx.imp2.o ] || { echo "ir2ll: no build/$s/ir2ll.imp2.o: skipped"; exit 0; }
done
[ -x build/llvm-ir2/impd ] && [ -f build/lib/libimp.a ] || { echo "ir2ll: no compiler: skipped"; exit 0; }
out=build/ir2ll
rm -rf $out
mkdir -p $out/ir
cc -DREF -o $out/ref tests/direct/ir2ll_drv.c tools/ir2ll.c || exit 1
for s in llvm-ir1 llvm-ir2; do
    cc -o $out/imp-$s tests/direct/ir2ll_drv.c build/$s/ir2ll.imp2.o build/$s/ir2llx.imp2.o build/lib/libimp.a || exit 1
done
export IMPSYNTAX=$root/src/syntax.imc IMPFFI=$root/lib/ffi.imc
k=build/llvm-ir2/impd
endf=$(sh tools/endian.sh)
for f in tests/native/*.imp tests/oracle/*.imp tests/direct/progs/*.imp; do
    $k -c $out/ir/$(basename $(dirname $f))_$(basename $f .imp).ir $IMPSYNTAX $IMPFFI $f >/dev/null 2>&1
done
gen="code code2 code3 demit impsem ix"       # the code generator; the other modules of src are the kernel's and the compiler's own (impsyn, impend-*, irsyn and syntax are syntax)
for f in $(ls src/*.imc | grep -v 'impsyn\|impend-\|irsyn\|syntax'); do
    case " $gen " in *" $(basename $f .imc) "*) continue ;; esac
    IMPMODULE=$(basename $f .imc) $k -c $out/ir/src_$(basename $f .imc).ir $IMPSYNTAX $IMPFFI $endf src/impsyn.imc $f >/dev/null 2>&1
done
for m in $gen; do
    IMPMODULE=$m $k -c $out/ir/ir_$m.ir $IMPSYNTAX $IMPFFI $endf src/impsyn.imc src/irsyn.imc src/$m.imc >/dev/null 2>&1
done
python3 tools/ir2ll-fuzz.py 1 ${IR2LL_COUNT:-60} $out/ir >/dev/null 2>&1 || echo "ir2ll: no python3, or the fuzzer failed: the fuzzed IR is left out"
fail=0
n=0
for f in $out/ir/*.ir; do
    [ -s $f ] || { echo "FAILED: ir2ll: no IR in $f" >&2; fail=1; continue; }
    for p in 0 1 2 3; do
        n=$((n + 1))
        $out/ref $f $out/ref.ll $p 2>$out/ref.err; r=$?
        for s in llvm-ir1 llvm-ir2; do
            $out/imp-$s $f $out/imp.ll $p 2>$out/imp.err; ri=$?
            if [ $r != $ri ] || { [ $r = 0 ] && ! cmp -s $out/ref.ll $out/imp.ll; } || { [ $r != 0 ] && ! cmp -s $out/ref.err $out/imp.err; }; then
                echo "FAILED: ir2ll: $f (pointers of $((64 - 32 * p)) bits) differs in the IMP writer of $s" >&2; fail=1
            fi
        done
    done
done
[ $fail = 0 ] && echo "ir2ll: the IMP writer writes what the C writes, for $n runs of $(ls $out/ir | wc -l) IR files"
exit $fail
