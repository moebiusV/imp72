#!/bin/sh
# tools/imprename (the nm and objcopy of the build of the compilers) on a COFF object (Windows, x86-64 and i386): the global symbols it defines are listed, and a rename (to a name longer than 8 bytes,
# which COFF keeps in its string table) is made in the string table and not otherwise: the object still reads (llvm-nm, where it is here), and its symbols are the new ones.
root=$(cd "$(dirname "$0")/../.." && pwd)
cd "$root" || exit 1
w=${IMPRENAME_IMP:-build/llvm-ir2/impstage-llvm-ir}
r=build/llvm-ir2/imprename
[ -x "$w" ] && [ -x "$r" ] || { echo "imprename test skipped: no $w or $r"; exit 77; }
out=build/imprename
rm -rf $out; mkdir -p $out
export IMPSYNTAX=$root/src/syntax.imc IMPFFI=$root/lib/ffi.imc IMPLIB=$root/build/lib/libimp.a
fail=0
bad() { echo "FAILED: imprename: $*" >&2; fail=1; }
echo "== imprename"
for t in x86_64-w64-mingw32 i686-w64-mingw32; do
    "$w" -c --target=$t -o $out/h.o tests/native/hello.imp >/dev/null 2>&1 || { bad "no object for $t"; continue; }
    [ "$(od -An -tx1 -N2 $out/h.o | tr -d ' \n')" = 6486 ] || [ "$(od -An -tx1 -N2 $out/h.o | tr -d ' \n')" = 4c01 ] || { bad "$t: not a COFF object"; continue; }
    $r -d $out/h.o | grep -qx 'impmain' || bad "$t: the listing does not have impmain"
    $r -d $out/h.o | grep -q '^\.' && bad "$t: the listing has a name of the linker's own"
    printf 'impmain impl_THE_RENAMED_MAIN_ROUTINE\nimpca impl_THE_RENAMED_COMMON_CELL\n' > $out/map
    $r -f $out/map $out/h.o $out/h2.o || { bad "$t: the rename failed"; continue; }
    $r -d $out/h2.o | grep -qx 'impl_THE_RENAMED_MAIN_ROUTINE' || bad "$t: the renamed symbol is not listed"
    $r -d $out/h2.o | grep -qx 'impmain' && bad "$t: the old name is still there"
    if command -v llvm-nm >/dev/null 2>&1 || [ -x "$(sh tools/llvm.sh bindir 2>/dev/null)/llvm-nm" ]; then
        nm=$(command -v llvm-nm || echo "$(sh tools/llvm.sh bindir)/llvm-nm")
        $nm $out/h2.o 2>/dev/null | grep -q 'impl_THE_RENAMED_COMMON_CELL' || bad "$t: llvm-nm does not read the renamed object"
    fi
done
[ $fail = 0 ] && echo ok
exit $fail
