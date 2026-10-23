#!/bin/sh
# The code generator that compiles to pcc IR (src/ir, tools/direct.sh): the native tests, the oracle goldens and the IEEE fuzz
# through imp72d, the compiler with it (a REAL word is the double's own bits here, so the library is libimp72raw.a).
root=$(cd "$(dirname "$0")/../.." && pwd)
cd "$root" || exit 1
[ -x "${CC1:-build/pcc/cc/ccom/cc1}" ] || { echo "direct tests skipped: no cc1 (make)"; exit 0; }
tools/direct.sh >build/direct.log 2>&1; rc=$?
[ $rc = 77 ] && { echo "direct tests skipped: no objcopy"; exit 0; }
[ $rc = 0 ] || { cat build/direct.log >&2; echo "FAILED: direct build" >&2; exit 1; }
CC1=${CC1:-$root/build/pcc/cc/ccom/cc1}; export CC1
export IMP72SYNTAX=$root/src/ir/syntax64.imc IMP72FFI=$root/lib/ffi.imc IMP72LIB=$root/build/lib/libimp72raw.a
export DRIVER=build/direct/imp72stage-ir
fail=0
NATIVE_OUT=build/native-ir NATIVE_LIB=build/lib/libimp72raw.a NATIVE_CFLAGS=-DIMP_REAL_RAW sh tests/native/run.sh || fail=1
ORACLE_IR=1 ORACLE_OUT=build/oracle-ir sh tests/oracle/run.sh || fail=1
if command -v python3 >/dev/null 2>&1; then
    FUZZ_OUT=build/ieee-fuzz-ir tools/ieee-fuzz.sh 1 8 || fail=1
fi
exit $fail
