#!/bin/sh
# The code generator that compiles to pcc IR (src/ir, tools/direct.sh): the native tests, the oracle goldens and the IEEE fuzz
# through imp72d, the compiler with it (a REAL word is the double's own bits here, so the library is libimp72raw.a).
root=$(cd "$(dirname "$0")/../.." && pwd)
cd "$root" || exit 1
[ -x "${CC1:-build/pcc/cc/ccom/cc1}" ] || { echo "direct tests skipped: no cc1 (make)"; exit 0; }
case $(uname -m) in x86_64|amd64) ;; *) echo "direct tests skipped: only tried on x86_64 so far"; exit 0 ;; esac
tools/direct.sh >build/direct.log 2>&1; rc=$?
[ $rc = 77 ] && { echo "direct tests skipped: no objcopy"; exit 0; }
[ $rc = 0 ] || { cat build/direct.log >&2; echo "FAILED: direct build" >&2; exit 1; }
CC1=${CC1:-$root/build/pcc/cc/ccom/cc1}; export CC1
export IMP72SYNTAX=$root/src/ir/syntax64.imc IMP72FFI=$root/lib/ffi.imc IMP72LIB=$root/build/lib/libimp72raw.a
export DRIVER=build/direct/imp72stage-ir
fail=0
# the compiler whose own IMP modules (src/*.imp) the IR path compiled, too: tests of it
DIRECT_OUT=build/direct3 DIRECT_DRIVER=build/direct/imp72stage-ir DIRECT_LIB=build/lib/libimp72raw.a \
    SRC_MODULES="free debug stack dir graph parse ercor entree cotree dosem rsyn lex" tools/direct.sh >build/direct3.log 2>&1 \
    || { cat build/direct3.log >&2; echo "FAILED: direct build of the compiler's own modules" >&2; fail=1; }
NATIVE_OUT=build/native-ir NATIVE_LIB=build/lib/libimp72raw.a NATIVE_CFLAGS=-DIMP_REAL_RAW sh tests/native/run.sh || fail=1
ORACLE_IR=1 ORACLE_OUT=build/oracle-ir sh tests/oracle/run.sh || fail=1
if [ -x build/direct3/imp72stage-ir ]; then
    DRIVER=build/direct3/imp72stage-ir NATIVE_OUT=build/native-ir3 NATIVE_LIB=build/lib/libimp72raw.a NATIVE_CFLAGS=-DIMP_REAL_RAW sh tests/native/run.sh || fail=1
    DRIVER=build/direct3/imp72stage-ir ORACLE_IR=1 ORACLE_OUT=build/oracle-ir3 sh tests/oracle/run.sh || fail=1
fi
if command -v python3 >/dev/null 2>&1; then
    FUZZ_OUT=build/ieee-fuzz-ir tools/ieee-fuzz.sh 1 8 || fail=1
    tools/direct-fuzz.sh 31 10 || fail=1
    GEN=imp-fuzz2.py tools/direct-fuzz.sh 41 10 || fail=1
fi
exit $fail
