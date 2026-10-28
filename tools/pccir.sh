#!/bin/sh
# pccir.sh: build the compiler whose code generator is src/pccir (docs/porting/pccir-path.md): stage 1 is made by the seed (stage0/, the
# C code generator), compiling src/pccir and the compiler's own IMP modules; stage 2 by stage 1; stage 2 is the one that is installed
# (tests/direct/stage2.sh checks that stage 1 and 2 write the same pcc IR).  Exit 77 where it cannot be built (no ELF objcopy).
set -e
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root"
cc1=${CC1:-$root/build/pcc/cc/ccom/cc1}
# the back end is the driver's to choose (LLVM where there is a clang, else the C one; pcc only when asked for): IMP72BACKEND=llvm, c or pcc forces one
case ${IMP72BACKEND:-} in
"")   [ -x "$root/build/bin/ir2c" ] || { echo "pccir: no ir2c (make): skipped"; exit 77; } ;;
c)    [ -x "$root/build/bin/ir2c" ] || { echo "pccir: no ir2c (make): skipped"; exit 77; } ;;
llvm) [ -x "$root/build/bin/ir2ll" ] || { echo "pccir: no ir2ll (make): skipped"; exit 77; } ;;
pcc)  [ -x "$cc1" ] || { echo "pccir: no pass 2 of pcc: skipped"; exit 77; } ;;
*)    echo "pccir: no back end $IMP72BACKEND (llvm, c, pcc)" >&2; exit 2 ;;
esac
export CC1=$cc1
mods="free debug stack dir graph parse ercor entree cotree dosem rsyn lex impm"
DIRECT_OUT=build/pccir1 DIRECT_LIB=build/lib/libimp72.a SRC_MODULES="$mods" tools/direct.sh >build/pccir1.log 2>&1 \
    || { r=$?; [ $r = 77 ] && { echo "pccir: skipped"; exit 77; }; cat build/pccir1.log >&2; exit $r; }
DIRECT_OUT=build/pccir2 DIRECT_DRIVER=build/pccir1/imp72stage-pccir DIRECT_LIB=build/lib/libimp72.a SRC_MODULES="$mods" \
    tools/direct.sh >build/pccir2.log 2>&1 || { cat build/pccir2.log >&2; exit 1; }
echo "built build/pccir2/imp72d: the compiler with the pcc IR code generator, compiled by itself"
