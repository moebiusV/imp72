#!/bin/sh
# tools/exports.imp: the declarations of the routines and COMMON variables of an IMP72 source for C, Zig and Rust (ROADMAP 8).  The three files it writes for tests/exports/sample.imp are
# compared with the ones kept here, and the C header is used by a C program (cc) that calls the compiled routines and reads and writes the COMMON variables, which checks that the
# declarations have the types the routines have.  Run from the top of the tree (make check does).
root=$(cd "$(dirname "$0")/../.." && pwd)
cd "$root" || exit 1
w=${EXPORTS_IMP:-build/llvm-ir2/impstage-llvm-ir}
[ -x "$w" ] || { echo "exports test skipped: no $w"; exit 0; }
out=build/exports
rm -rf $out; mkdir -p $out
fail=0
bad() { echo "FAILED: exports: $*" >&2; fail=1; }
echo "== exports"
sh tools/imptool.sh exports -h >/dev/null 2>&1; [ $? = 2 ] || { bad "tools/exports.imp does not build"; exit 1; }
tool=build/tools/exports
$tool -c -n sample -o $out/sample.h tests/exports/sample.imp && $tool -z -o $out/sample.zig tests/exports/sample.imp && $tool -r -o $out/sample.rs tests/exports/sample.imp || bad "the tool failed"
for f in sample.h sample.zig sample.rs; do cmp -s $out/$f tests/exports/$f || { diff $out/$f tests/exports/$f | head -6; bad "$f is not what is kept"; }; done
$tool -c tests/exports/nothere.imp >/dev/null 2>&1 && bad "a source that is not there was accepted"
$tool -x tests/exports/sample.imp >/dev/null 2>&1 && bad "an unknown option was accepted"
if command -v cc >/dev/null 2>&1; then
    export IMPSYNTAX=$root/src/syntax.imc IMPFFI=$root/lib/ffi.imc IMPLIB=$root/build/lib/libimp.a
    "$w" -c -o $out/sample.o tests/exports/sample.imp > $out/sample.log 2>&1 || bad "the compiler did not compile the sample"
    cc $(sh tools/nopie.sh) -I$out -o $out/main tests/exports/main.c $out/sample.o $root/build/lib/libimp.a > $out/cc.log 2>&1 || { head -5 $out/cc.log >&2; bad "the C program did not compile with the header"; }
    [ "$($out/main 2>&1)" = "5 42 7 115 9" ] || bad "the C program's answers are not 5 42 7 115 9 ($($out/main 2>&1 | head -1))"
else
    echo "exports: no cc, the C program is not run"
fi
[ $fail = 0 ] && echo ok
exit $fail
