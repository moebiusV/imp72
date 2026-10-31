#!/bin/sh
# The code generator that compiles to IR (src/llvm-ir, tools/direct.sh): the native tests, the oracle goldens and the IEEE fuzz
# through imp72d, the compiler with it (a REAL word is the double's own bits here, so the library is libimp72.a).
root=$(cd "$(dirname "$0")/../.." && pwd)
cd "$root" || exit 1
. tools/limits.sh
tools/direct.sh >build/direct.log 2>&1; rc=$?
[ $rc = 77 ] && { echo "direct tests skipped: the seed has no LLVM"; exit 0; }
[ $rc = 0 ] || { dd bs=100000 count=1 if=build/direct.log 2>/dev/null >&2; echo "FAILED: direct build" >&2; exit 1; }
export IMP72SYNTAX=$root/src/llvm-ir/syntax64.imc IMP72FFI=$root/lib/ffi.imc IMP72LIB=$root/build/lib/libimp72.a
export DRIVER=build/direct/imp72stage-llvm-ir
fail=0
# the compiler whose own IMP modules (src/*.imp) the IR path compiled, too: tests of it
DIRECT_OUT=build/direct3 DIRECT_DRIVER=build/direct/imp72stage-llvm-ir DIRECT_LIB=build/lib/libimp72.a \
    SRC_MODULES="free debug stack dir graph parse ercor entree cotree dosem rsyn lex image impm ir2ll ir2llx driver llvm llvmgen" tools/direct.sh >build/direct3.log 2>&1 \
    || { dd bs=100000 count=1 if=build/direct3.log 2>/dev/null >&2; echo "FAILED: direct build of the compiler's own modules" >&2; fail=1; }
NATIVE_OUT=build/native-ir NATIVE_LIB=build/lib/libimp72.a sh tests/native/run.sh || fail=1
ORACLE_LLVMIR=1 ORACLE_OUT=build/oracle-ir sh tests/oracle/run.sh || fail=1
# the PDP-10 constructs the original has, as this machine has them (XWD, IOWD, CALLI): programs and their output, which is not the PDP-10's (32 bit halves)
for p in tests/direct/progs/*.imp; do
    n=$(basename $p .imp); echo "== llvm-ir $n"
    build/direct/imp72stage-llvm-ir -o build/native-ir/$n $p >build/native-ir/$n.log 2>&1 || { cat build/native-ir/$n.log; echo "FAILED: $n: compile" >&2; fail=1; continue; }
    build/native-ir/$n > build/native-ir/$n.got; cmp -s build/native-ir/$n.got tests/direct/progs/$n.out && echo ok || { echo "FAILED: $n: output" >&2; fail=1; }
done
if [ -x build/direct3/imp72stage-llvm-ir ]; then
    DRIVER=build/direct3/imp72stage-llvm-ir NATIVE_OUT=build/native-ir3 NATIVE_LIB=build/lib/libimp72.a sh tests/native/run.sh || fail=1
    DRIVER=build/direct3/imp72stage-llvm-ir ORACLE_LLVMIR=1 ORACLE_OUT=build/oracle-ir3 sh tests/oracle/run.sh || fail=1
fi
if command -v python3 >/dev/null 2>&1; then
    FUZZ_OUT=build/ieee-fuzz-ir tools/ieee-fuzz.sh 1 8 || fail=1
    tools/direct-fuzz.sh 31 10 || fail=1
    GEN=imp-fuzz2.py tools/direct-fuzz.sh 41 10 || fail=1
fi
exit $fail
