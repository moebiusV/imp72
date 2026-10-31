#!/bin/sh
# LLVM in the process (stage0/llvmorc.c): --run compiles a program with ORC and runs it without a file, and the object of a program is made by the
# target machine of the host; there is no clang in it.  Skipped where the compiler was built without the LLVM libraries.  Run from the top of the tree.
root=$(cd "$(dirname "$0")/../.." && pwd)
cd "$root" || exit 1
w=${ORC_IMP72:-build/llvm-ir2/imp72stage-llvm-ir}
[ -x "$w" ] || { echo "orc test skipped: no $w"; exit 0; }
ldd "$w" 2>/dev/null | grep -qi llvm || { echo "orc test skipped: built without the LLVM libraries"; exit 0; }
export IMP72SYNTAX=$root/src/llvm-ir/syntax64.imc IMP72FFI=$root/lib/ffi.imc IMP72LIB=$root/build/lib/libimp72.a
out=build/orc
rm -rf $out; mkdir -p $out
fail=0
echo "== orc"
for n in hello algo prog2; do
    [ -f tests/native/$n.imp ] && [ ! -f tests/native/${n}_main.c ] || continue
    $w --run tests/native/$n.imp > $out/$n.run 2>&1
    cmp -s $out/$n.run tests/native/$n.out || { echo "FAILED: orc: --run $n: $(dd bs=400 count=1 if=$out/$n.run 2>/dev/null | tr '\n' '|')" >&2; fail=1; }
    $w -o $out/$n tests/native/$n.imp > $out/$n.log 2>&1 && $out/$n > $out/$n.got 2>&1
    cmp -s $out/$n.got tests/native/$n.out || { echo "FAILED: orc: object of $n: $(dd bs=400 count=1 if=$out/$n.log 2>/dev/null $out/$n.got 2>/dev/null | tr '\n' '|')" >&2; fail=1; }
done
[ $fail = 0 ] && echo ok
exit $fail
