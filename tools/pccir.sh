#!/bin/sh
# pccir.sh: build the compiler whose code generator is src/pccir (docs/porting/pccir-path.md): stage 1 is made by the seed (stage0/, the
# C code generator), compiling src/pccir and the compiler's own IMP modules; stage 2 by stage 1; stage 2 is the one that is installed
# (tests/direct/stage2.sh checks that stage 1 and 2 write the same IR).  Exit 77 where it cannot be built (no LLVM libraries).
set -e
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root"
# the back end is LLVM, in the process: the seed must have been built with the LLVM libraries
sh "$root/tools/llvm.sh" cflags | grep -q HAVE_LLVM || { echo "pccir: the seed was built without the LLVM libraries: skipped"; exit 77; }
mods="free debug stack dir graph parse ercor entree cotree dosem rsyn lex image impm driver llvm llvmgen"
DIRECT_OUT=build/pccir1 DIRECT_LIB=build/lib/libimp72.a SRC_MODULES="$mods" tools/direct.sh >build/pccir1.log 2>&1 \
    || { r=$?; [ $r = 77 ] && { echo "pccir: skipped"; exit 77; }; cat build/pccir1.log >&2; exit $r; }
DIRECT_OUT=build/pccir2 DIRECT_DRIVER=build/pccir1/imp72stage-pccir DIRECT_LIB=build/lib/libimp72.a SRC_MODULES="$mods" \
    tools/direct.sh >build/pccir2.log 2>&1 || { cat build/pccir2.log >&2; exit 1; }
echo "built build/pccir2/imp72d: the compiler with the IR code generator, compiled by itself"
