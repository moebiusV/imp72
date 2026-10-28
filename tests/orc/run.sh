#!/bin/sh
# LLVM in the process (stage0/llvmorc.c): --run compiles a program with ORC and runs it without a file, and the object of a program is made by the
# target machine of the host without a clang.  Skipped where the compiler was built without the LLVM libraries.  Run from the top of the tree.
root=$(cd "$(dirname "$0")/../.." && pwd)
cd "$root" || exit 1
w=${ORC_IMP72:-build/pccir2/imp72stage-pccir}
[ -x "$w" ] || { echo "orc test skipped: no $w"; exit 0; }
ldd "$w" 2>/dev/null | grep -qi llvm || { echo "orc test skipped: built without the LLVM libraries"; exit 0; }
export IMP72SYNTAX=$root/src/pccir/syntax64.imc IMP72FFI=$root/lib/ffi.imc IMP72LIB=$root/build/lib/libimp72.a
out=build/orc
rm -rf $out; mkdir -p $out
fail=0
echo "== orc"
for n in hello algo prog2; do
    [ -f tests/native/$n.imp ] && [ ! -f tests/native/${n}_main.c ] || continue
    $w --run tests/native/$n.imp > $out/$n.run 2>&1
    cmp -s $out/$n.run tests/native/$n.out || { echo "FAILED: orc: --run $n" >&2; fail=1; }
    $w -o $out/$n tests/native/$n.imp > $out/$n.log 2>&1 && $out/$n > $out/$n.got 2>&1
    cmp -s $out/$n.got tests/native/$n.out || { echo "FAILED: orc: object of $n" >&2; fail=1; }
done
# the in-process object is the one the spawned clang makes, as far as running it shows
IMP72LLVMSPAWN=1 $w -o $out/hello_spawn tests/native/hello.imp > /dev/null 2>&1 && $out/hello_spawn > $out/hello_spawn.got 2>&1
cmp -s $out/hello_spawn.got tests/native/hello.out || { echo "FAILED: orc: spawned clang" >&2; fail=1; }
[ $fail = 0 ] && echo ok
exit $fail
