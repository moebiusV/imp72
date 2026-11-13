#!/bin/sh
# -H FILE: a file of declarations read in front of each source (src/lex.imc LEXHDR, src/driver.imc), so that a declaration is written once.  Two units that call the C
# functions strlen and strcmp, declared in one file that is not part of either (the declaration of strcmp makes its negative int result a negative word: how negative is the C library's own, -1 on x86_64 and -64 on aarch64, so the program says only whether it is); the seed and imp
# must agree, and a file that is not there is refused.
root=$(cd "$(dirname "$0")/../.." && pwd)
cd "$root" || exit 1
w=${DECLS_IMP:-build/llvm-ir2/impstage-llvm-ir}
s=stage0/impstage0
[ -x "$w" ] && [ -x "$s" ] || { echo "decls test skipped: no $w or $s"; exit 0; }
out=build/decls
rm -rf $out; mkdir -p $out
cat > $out/decls.imp <<'E'
EXTERNAL INT strcmp(PTR, PTR);
EXTERNAL SIZE strlen(PTR);
E
cat > $out/a.imp <<'E'
N,NEGV ARE COMMON;
N:=B1(0);
NEGV:=CCALL strcmp(LOC('A'),LOC('B'));
PRINT STG 0,'A ',IGR 0,N,STG 0,' ',IGR 0,(NEGV<0 => 1 ELSE 0),/;
FINI(0)
%%
E
cat > $out/b.imp <<'E'
SUBR B1(Z) IS (CCALL strlen(LOC('HELLO')) AND 4294967295)
%%
E
fail=0
bad() { echo "FAILED: decls: $*" >&2; fail=1; }
echo "== decls"
export IMPSYNTAX=$root/src/syntax.imc IMPFFI=$root/lib/ffi.imc IMPLIB=$root/build/lib/libimp.a
"$w" -H $out/decls.imp -o $out/imp.exe $out/a.imp $out/b.imp > $out/imp.log 2>&1 && [ "$($out/imp.exe)" = "A 5 1" ] || { bad "imp: two units that share a file of declarations"; cat $out/imp.log >&2; }
# (without the file strcmp is a function that returns a word and the upper half of it is the C library's own: how it ends is not for a test to say; what the file changes is the declaration, and a result that is extended from the int it is)
"$w" -H $out/decls.imp -Sir -o $out/with.ll $out/a.imp > /dev/null 2>&1; "$w" -Sir -o $out/without.ll $out/a.imp > /dev/null 2>&1
{ grep -q 'declare i32 @"strcmp"(ptr, ptr)' $out/with.ll && grep -q 'sext i32' $out/with.ll && ! grep -q 'declare i32 @"strcmp"(ptr, ptr)' $out/without.ll; } || bad "imp: the declaration of strcmp does not make its result an extended int (the file is not what changes it)"
"$w" -H $out/decls.imp -c -o $out/a.o $out/a.imp >/dev/null 2>&1 && [ -s $out/a.o ] || bad "imp: -c with -H"
"$w" -H $out/nothere.imp -c -o $out/n.o $out/a.imp > $out/n.log 2>&1 && bad "imp: a file of declarations that is not there was accepted"
unset IMPSYNTAX IMPFFI IMPLIB
"$s" -H $out/decls.imp -c -o $out/sa.o $out/a.imp > $out/sa.log 2>&1 && [ -s $out/sa.o ] || { bad "seed: -H"; cat $out/sa.log >&2; }
# the same IR from both
env -u IMPSYNTAX -u IMPFFI -u IMPLIB "$s" -H $out/decls.imp -Sir -o $out/seed.ll $out/a.imp > $out/seed.log 2>&1
IMPSYNTAX=$root/src/syntax.imc IMPFFI=$root/lib/ffi.imc "$w" -H $out/decls.imp -Sir -o $out/stage.ll $out/a.imp > $out/stage.log 2>&1
[ -s $out/seed.ll ] && cmp -s $out/seed.ll $out/stage.ll || bad "the seed and imp write different IR for a source with -H"
[ $fail = 0 ] && echo ok
exit $fail
