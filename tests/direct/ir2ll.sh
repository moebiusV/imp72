#!/bin/sh
# The LLVM IR writer in IMP (src/ir2ll.imp, src/ir2llx.imp) against the C it was translated from (tools/ir2ll.c, which the seed links): the IR of the native tests, the oracle
# programs, the modules of the compiler and a fuzzer (tools/ir2ll-fuzz.py: every operator, type and directive) goes through both, for pointers of 64 and of 32 bits, and what they write
# must be the same, byte for byte.  The IMP one is taken as stage 1 (the seed) and stage 2 (stage 1) compiled it, which are the objects tools/direct.sh left in build/llvm-ir1 and build/llvm-ir2.
root=$(cd "$(dirname "$0")/../.." && pwd)
cd "$root" || exit 1
for s in llvm-ir1 llvm-ir2; do
    [ -f build/$s/ir2ll.imp2.o ] && [ -f build/$s/ir2llx.imp2.o ] || { echo "ir2ll: no build/$s/ir2ll.imp2.o: skipped"; exit 0; }
done
[ -x build/llvm-ir2/imp72d ] && [ -f build/lib/libimp72.a ] || { echo "ir2ll: no compiler: skipped"; exit 0; }
out=build/ir2ll
rm -rf $out
mkdir -p $out/ir
cc -DREF -o $out/ref tests/direct/ir2ll_drv.c tools/ir2ll.c || exit 1
for s in llvm-ir1 llvm-ir2; do
    cc -o $out/imp-$s tests/direct/ir2ll_drv.c build/$s/ir2ll.imp2.o build/$s/ir2llx.imp2.o build/lib/libimp72.a || exit 1
done
export IMP72SYNTAX=$root/src/llvm-ir/syntax64.imc IMP72FFI=$root/lib/ffi.imc
k=build/llvm-ir2/imp72d
endf=$(sh tools/endian.sh)
for f in tests/native/*.imp tests/oracle/*.imp tests/direct/progs/*.imp; do
    $k -c $out/ir/$(basename $(dirname $f))_$(basename $f .imp).ir $IMP72SYNTAX $IMP72FFI $f >/dev/null 2>&1
done
for f in src/*.imp; do
    IMP72MODULE=$(basename $f .imp) $k -c $out/ir/src_$(basename $f .imp).ir $IMP72SYNTAX $IMP72FFI $endf src/impsyn64.imc $f >/dev/null 2>&1
done
for f in src/llvm-ir/*.imp; do
    IMP72MODULE=$(basename $f .imp) $k -c $out/ir/ir_$(basename $f .imp).ir $IMP72SYNTAX $IMP72FFI $endf src/impsyn64.imc src/llvm-ir/irsyn.imc $f >/dev/null 2>&1
done
python3 tools/ir2ll-fuzz.py 1 ${IR2LL_COUNT:-60} $out/ir >/dev/null 2>&1 || echo "ir2ll: no python3, or the fuzzer failed: the fuzzed IR is left out"
fail=0
n=0
for f in $out/ir/*.ir; do
    [ -s $f ] || { echo "FAILED: ir2ll: no IR in $f" >&2; fail=1; continue; }
    for p in 0 1; do
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
