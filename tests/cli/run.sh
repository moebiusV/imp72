#!/bin/sh
# The command line of imp as a user of the C compiler would try it: where the output goes (a.out, NAME.o, NAME.s, NAME.ll), -c with several files, -o with several outputs (refused), a program
# in two files, -S for the assembly of the target and -Sir for the LLVM IR, --target for another system (the object of it is in that system's format), --version and --help.  Run from the top of the
# tree (make check does).
root=$(cd "$(dirname "$0")/../.." && pwd)
cd "$root" || exit 1
w=${CLI_IMP:-$root/build/llvm-ir2/impstage-llvm-ir}
[ -x "$w" ] || { echo "cli test skipped: no $w"; exit 0; }
export IMPSYNTAX=$root/src/syntax.imc IMPFFI=$root/lib/ffi.imc IMPLIB=$root/build/lib/libimp.a
d=build/cli
rm -rf $d; mkdir -p $d
cd $d || exit 1
cat > main.imp <<'E'
N ARE COMMON;
N:=TWICE(21);
PRINT STG 0,'ANSWER ',IGR 0,N,/;
FINI(0)
%%
E
cat > lib2.imp <<'E'
SUBR TWICE(A) IS A*2
%%
E
fail=0
bad() { echo "FAILED: cli: $*" >&2; fail=1; }
echo "== cli"
"$w" main.imp lib2.imp >/dev/null 2>&1 && [ -x a.out ] && [ "$(./a.out)" = "ANSWER 42" ] || bad "two files linked into a.out"
rm -f a.out
"$w" -o exe1 main.imp lib2.imp >/dev/null 2>&1 && [ "$(./exe1)" = "ANSWER 42" ] || bad "-o exe1 with two files"
"$w" -c main.imp lib2.imp >/dev/null 2>&1 && [ -s main.o ] && [ -s lib2.o ] || bad "-c with two files makes main.o and lib2.o"
"$w" main.o lib2.o -o exe2 >/dev/null 2>&1 && [ "$(./exe2)" = "ANSWER 42" ] || bad "linking the objects"
"$w" -c -o x.o main.imp lib2.imp >/dev/null 2>&1 && bad "-o with -c and two sources was accepted"
"$w" -c -o one.o lib2.imp >/dev/null 2>&1 && [ -s one.o ] || bad "-c -o one.o"
"$w" -S lib2.imp >/dev/null 2>&1 && grep -q -e '\.text' -e '__TEXT,__text' lib2.s || bad "-S makes assembly (lib2.s)"
"$w" -Sir lib2.imp >/dev/null 2>&1 && grep -q '^define' lib2.ll || bad "-Sir makes LLVM IR (lib2.ll)"
"$w" -S -o named.s lib2.imp >/dev/null 2>&1 && grep -q -e '\.text' -e '__TEXT,__text' named.s || bad "-S -o named.s"
"$w" nosuch.imp >/dev/null 2>&1 && bad "a missing file was accepted"
"$w" -z lib2.imp >/dev/null 2>&1 && bad "an unknown option was accepted"
"$w" -o >/dev/null 2>&1 && bad "-o with no argument was accepted"
ver=$(cat "$root/VERSION"); verre=$(printf %s "$ver" | sed 's/\./\\./g')
[ "$("$w" --version 2>&1)" = "imp $ver" ] || bad "--version"
"$w" --help 2>&1 | grep -q '^usage: imp' || bad "--help"
case $(sh "$root/tools/endian.sh" 2>/dev/null) in *-be.imc) be=1 ;; *) be=0 ;; esac
t64=--target=x86_64-linux-gnu                      # (the target's byte order is taken from LLVM's data layout of it, so a big endian host asks for the little endian targets too)
mach() { od -An -tu1 -v -N20 "$1" | tr -s ' \n' '  ' | awk '{print $19 + 256 * $20}'; }       # (bytes 18 and 19 of the header; no -j, which the BSDs' od takes otherwise)
have() { "$w" -c --target=$1 lib2.imp -o probe.o > probe.log 2>&1; ! grep -q 'No available targets' probe.log; }       # does the LLVM of this compiler have the target (a system's own LLVM may be built with its architecture alone: OpenBSD's has X86 and AMDGPU)
if :; then
# another system: the object is in its format (ELF, e_machine at byte 18: x86-64 62, aarch64 183, riscv 243) and 32 bit pointers for a 32 bit one
if have aarch64-linux-gnu; then
"$w" -c --target=aarch64-linux-gnu lib2.imp -o a64.o >/dev/null 2>&1 && [ "$(mach a64.o)" = 183 ] || bad "--target=aarch64-linux-gnu"
"$w" -S --target=aarch64-linux-gnu lib2.imp -o a64.s >/dev/null 2>&1 && grep -q 'ret' a64.s && ! grep -q 'rdi' a64.s || bad "--target=aarch64: assembly for it"
fi
# a target of the other byte order is taken (the order is LLVM's, from the data layout): ELF class (byte 4) and data (byte 5) of s390x are 2 and 2, of powerpc 1 and 2
bo() { c=$(od -An -tu1 -v -j4 -N1 "$1" | tr -d ' \t\n'); d=$(od -An -tu1 -v -j5 -N1 "$1" | tr -d ' \t\n'); echo $((c * 10 + d)); }
if have s390x-linux-gnu; then
"$w" -c --target=s390x-linux-gnu lib2.imp -o s390.o >s390.log 2>&1; rc=$?; [ $rc = 0 ] && [ "$(bo s390.o)" = 22 ] || bad "--target=s390x-linux-gnu (big endian, 64 bits): status $rc, bo '$(bo s390.o)', header bytes '$(od -An -tu1 -v -N8 s390.o 2>&1 | tr -s ' ' ' ')', output '$(head -c 200 s390.log)'"
fi
if have powerpc-linux-gnu; then
"$w" -c --target=powerpc-linux-gnu lib2.imp -o ppc.o >ppc.log 2>&1; rc=$?; [ $rc = 0 ] && [ "$(bo ppc.o)" = 12 ] || bad "--target=powerpc-linux-gnu (big endian, 32 bits): status $rc, bo '$(bo ppc.o)', header bytes '$(od -An -tu1 -v -N8 ppc.o 2>&1 | tr -s ' ' ' ')', output '$(head -c 200 ppc.log)'"
fi
if have riscv64-linux-gnu; then
"$w" -c --target=riscv64-linux-gnu lib2.imp -o rv.o >/dev/null 2>&1 && [ "$(mach rv.o)" = 243 ] || bad "--target=riscv64-linux-gnu"
fi
if have i686-linux-gnu; then
"$w" -Sir --target=i686-linux-gnu lib2.imp -o i.ll >/dev/null 2>&1 && grep -q 'i32 %a0' i.ll || bad "--target=i686: 32 bit pointers in the IR"
fi
# a target that is not one is refused with a message and writes nothing; the usual ones are taken (the vendor is free text)
for t in garbage x86_64-nonsense-os x86_64-pc-foo-bar i686- 'x86_64-linux-gnu;rm' x86_64-linux-bogus x86_64-linux-bogusenv riscv64-unknown-nonsense aarch64--none-nonsense x86_64-linux-gnu-bogus; do
    rm -f tgt.o
    "$w" --target="$t" -c -o tgt.o lib2.imp > tgt.out 2>&1 && bad "--target=$t was accepted"
    [ -e tgt.o ] && bad "--target=$t left an object file"
    grep -q '^imp: unrecognized target' tgt.out || bad "--target=$t: no message"
done
for t in x86_64-linux-gnu x86_64-pc-cygwin i686-pc-cygwin x86_64-pc-windows-cygnus x86_64-w64-mingw32 x86_64-pc-linux-gnu i686-linux-gnu aarch64-linux-gnu armv7-linux-gnueabihf riscv64-unknown-linux-gnu aarch64-apple-darwin x86_64-unknown-freebsd15.0 x86_64-unknown-openbsd8.0 x86_64-unknown-netbsd11.0 x86_64-linux-musl i586-alpine-linux-musl x86_64-unknown-hurd-gnu x86_64-pc-linux-gnux32 x86_64-unknown-linux-android wasm32-unknown-emscripten thumbv7em-none-eabi x86_64-apple-ios17.0-simulator x86_64-unknown-none-elf; do      # (the last seven are no fixed list's: LLVM's own triple parser decides)
    have $t && { rm -f tgt.o; "$w" --target="$t" -c -o tgt.o lib2.imp > tgt.out 2>&1 && [ -s tgt.o ] || bad "--target=$t was refused: $(head -c 100 tgt.out)"; }
done
# the other system for the link tests is not this one (an arm64 Linux host links aarch64-linux-gnu natively)
oth=
for t in aarch64-linux-gnu riscv64-linux-gnu x86_64-linux-gnu; do
    case $(uname -m)-$(uname -s):$t in x86_64-Linux:x86_64-*|amd64-Linux:x86_64-*|aarch64-Linux:aarch64-*|arm64-Linux:aarch64-*) continue ;; esac     # (that one is the host's: it links)
    have $t && { oth=$t; break; }
done
if [ -n "$oth" ]; then
# linking for another system needs that system's files: without them the message says which and how to give them (--sysroot, --runtime)
"$w" --target=$oth main.imp lib2.imp > xl.log 2>&1 && bad "linking for another system with no sysroot succeeded"
grep -q -e '--sysroot=DIR' xl.log && grep -q -e '--runtime=FILE' xl.log && grep -q 'crt1.o' xl.log || bad "the message for a link for another system does not say what is needed and how to say where"
"$w" --target=$oth --sysroot=$root/build/cli/nosuchroot main.imp lib2.imp > xl2.log 2>&1 && bad "a sysroot with nothing in it linked"
grep -q 'no crt1.o under' xl2.log || bad "the message does not say where crt1.o was looked for"
fi
if have arm64-apple-darwin; then
# Mach-O from any system: the object (magic cf fa ed fe), the message for the SDK, and the Mach-O driver of lld in this compiler (a fake SDK and an empty runtime: it must get as far as lld's complaint)
"$w" -c --target=arm64-apple-darwin lib2.imp -o m.o >/dev/null 2>&1 && [ "$(od -An -tx1 -N4 m.o | tr -d ' \n')" = cffaedfe ] || bad "--target=arm64-apple-darwin: a Mach-O object"
"$w" --target=arm64-apple-darwin main.imp lib2.imp > xm.log 2>&1 && bad "linking for macOS with no SDK succeeded"
grep -q 'libSystem.tbd' xm.log && grep -q -e '--sysroot=DIR' xm.log || bad "the message for macOS does not say what is needed"
mkdir -p fakesdk/usr/lib && printf -- '--- !tapi-tbd\ntbd-version: 4\ntargets: [ arm64-macos ]\ninstall-name: "/usr/lib/libSystem.B.dylib"\nexports:\n  - targets: [ arm64-macos ]\n    symbols: [ dyld_stub_binder ]\n...\n' > fakesdk/usr/lib/libSystem.tbd
ar rcs empty.a 2>/dev/null
"$w" --target=arm64-apple-darwin --sysroot=$root/build/cli/fakesdk --runtime=$root/build/cli/empty.a main.imp lib2.imp > xm2.log 2>&1 && bad "the Mach-O link with no runtime succeeded"
[ "$(uname -s)" = Darwin ] || grep -q 'ld64.lld: error: undefined symbol' xm2.log || { [ ! -x "$(command -v ar)" ] || bad "the Mach-O driver of lld did not run in this compiler"; }       # (on a Mac that target is the host's: no cross link to refuse)
fi
"$w" -c --target=nonsense lib2.imp >/dev/null 2>&1 && bad "a nonsense target was accepted"
fi
if [ "$(uname -m)" = x86_64 ] && [ "$(getconf LONG_BIT)" = 64 ]; then       # the host's own architecture and system, whatever the vendor and the C library: a target that links
    "$w" --target=x86_64-linux-gnu main.imp lib2.imp -o hosted >/dev/null 2>&1 && [ "$(./hosted)" = "ANSWER 42" ] || { [ "$(uname -s)" != Linux ] || bad "the host's own triple is a target that links"; }
fi
# the commands of the original (-i): a compile writes the object, the REL file of TOPS-10 (a .o), and not the IR; a file of syntax with /C writes none; the banner has today's date; the compiler
# saved by /U shows (A) and the date of the save, and the next one saved from it (B); OBJ,LIST_SOURCE names the object and the listing
printf 'SUBR F(A) IS A+1\n%%%%\n' > okp.imp
printf '<ATOM> ::= ZED ::= "7"%%%%\n' > ext.imc
printf 'SUBR ADD2(A) IS A+ZED\n%%%%\n' > zprog.imp
# -C names a file of syntax or declarations (the original's /C), read before the sources, each of them when there are several; a file given without it is a source
printf 'SUBR G(A) IS A+ZED\n%%%%\n' > cz1.imp
printf 'SUBR H(A) IS A+ZED\n%%%%\n' > cz2.imp
"$w" -c -C ext.imc cz1.imp >/dev/null 2>&1 && [ -s cz1.o ] || bad "-C: the syntax of ext.imc is read before cz1.imp"
rm -f cz1.o
"$w" -c -C ext.imc cz1.imp cz2.imp >/dev/null 2>&1 && [ -s cz1.o ] && [ -s cz2.o ] || bad "-C: the syntax is read before each of two sources"
rm -f cz1.o cz2.o
"$w" -Sir -o cz-with.ll -C ext.imc cz1.imp >/dev/null 2>&1; "$w" -Sir -o cz-without.ll cz1.imp >/dev/null 2>&1
[ -s cz-with.ll ] && [ -s cz-without.ll ] && ! cmp -s cz-with.ll cz-without.ll || bad "-C: the extension changes the program (ZED is 7 with it, a variable without)"
"$w" -C >/dev/null 2>&1; [ $? = 2 ] || bad "-C: a missing argument is refused with 2"
# a file name at the prompt is a Unix name (up to the system's), not the tape's six characters and not 15
ln=$(printf '%0100d' 0 | tr 0 n)
cp okp.imp $ln.imp
printf '%s\n' "$ln" | "$w" -i > il.log 2>&1
[ -s $ln.o ] || bad "-i: a file name of 100 characters ($(head -c 200 il.log))"
rm -f $ln.o $ln.imp
printf 'okp\n' | "$w" -i > i1.log 2>&1
[ -s okp.o ] && [ ! -e okp.ir ] && [ ! -e okp.ir.ll ] || bad "-i: okp makes okp.o and no IR"
head -1 i1.log | grep -q " $(date +%e-%b-%y | tr a-z A-Z | sed 's/^ *//')" || bad "-i: the banner has the date of today ($(head -1 i1.log))"
rm -f okp.o
printf 'ext/C\nzprog\n' | "$w" -i > i2.log 2>&1
[ -s zprog.o ] && [ ! -e ext.o ] || bad "-i: ext/C then zprog makes zprog.o and no ext.o"
printf 'mine,mine_okp/L\n' | "$w" -i > i3.log 2>&1
[ -s mine.o ] && [ -s mine.lst ] || bad "-i: mine,mine_okp/L makes mine.o and mine.lst"
printf 'ext/C\nmy_/U\n' | "$w" -i > i4.log 2>&1
[ -s my.img ] && grep -q 'COMPILER SAVED ON my.img' i4.log || bad "-i: ext/C then my_/U saves my.img"
printf '\n' | "$w" -i -I my.img 2>&1 | head -1 | grep -q "$verre(A)  " || bad "-i -I: the saved compiler shows (A)"
printf 'ext/C\nmy2_/V\n' | "$w" -i -I my.img > i5.log 2>&1
printf '\n' | "$w" -i -I my2.img 2>&1 | head -1 | grep -q "$verre(B)  " || bad "-i -I: the one saved from it shows (B)"
rm -f zprog.o
printf 'zprog\n' | "$w" -i -I my.img > i6.log 2>&1
[ -s zprog.o ] || bad "-i -I: the saved compiler compiles zprog"
if [ "$be" = 0 ] && have aarch64-linux-gnu; then
rm -f okp.o
printf 'okp\n' | "$w" -i --target=aarch64-linux-gnu > i7.log 2>&1
[ -s okp.o ] && [ "$(mach okp.o)" = 183 ] || bad "-i --target=aarch64-linux-gnu makes an aarch64 object"
fi
# declarations of C functions (EXTERNAL INT puts(PTR);): the count of the arguments, the types and the declarations are checked at compile time
cat > cdecl_bad.imp <<'E'
EXTERNAL INT puts(PTR);
EXTERNAL INT printf(PTR, ...);
EXTERNAL INT f(FOO);
EXTERNAL INT ldexp(INT, INT);
EXTERNAL DOUBLE ldexp(DOUBLE, INT);
CCALL puts();
CCALL puts('a','b');
CCALL printf();
CCALL strlen('x');
EXTERNAL INT strlen(PTR);
EXTERNAL INT g(VOID, INT)
%%
E
"$w" -c cdecl_bad.imp > cdecl_bad.log 2>&1 && bad "cdecl: a program with wrong declarations and calls was accepted"
for m in 'FOO IS NOT A C TYPE' 'ldexp IS DECLARED TWICE WITH DIFFERENT TYPES' 'CCALL puts TAKES 1 ARGUMENTS, NOT 0' 'CCALL puts TAKES 1 ARGUMENTS, NOT 2' \
    'CCALL printf TAKES AT LEAST 1 ARGUMENTS, NOT 0' 'strlen IS DECLARED AFTER A CALL OF IT THAT HAD NO DECLARATION' 'VOID IS ONLY THE WHOLE LIST OF PARAMETERS'; do
    grep -q "$m" cdecl_bad.log || bad "cdecl: no error '$m' ($(head -c 300 cdecl_bad.log))"
done
# callbacks (CALLBACK INT CMP(PTR, PTR);) and calls through a pointer (FNPTR INT F(PTR);): the declarations are checked against the subr and the calls
cat > callback_bad.imp <<'E'
CALLBACK INT CMP(PTR, PTR);
CALLBACK INT CMP(PTR);
CALLBACK INT F(FOO);
FNPTR INT PC(PTR);
X:=CALLBACK(NOPE);
X:=CALLBACK(CMP);
CCALL CMP(1,2);
CCALL PC(1,2,3);
CCALL PC();
FINI(0);
SUBR CMP(A) IS 0
%%
E
"$w" -c callback_bad.imp > callback_bad.log 2>&1 && bad "callback: a program with wrong declarations and calls was accepted"
for m in 'CMP IS DECLARED TWICE WITH DIFFERENT TYPES' 'FOO IS NOT A C TYPE' 'NOPE IS NOT DECLARED A CALLBACK' 'CMP IS A CALLBACK, A SUBR' 'CCALL PC TAKES 2 ARGUMENTS, NOT 3' \
    'CCALL PC TAKES 2 ARGUMENTS, NOT 0' 'CALLBACK CMP HAS 2 PARAMETERS IN ITS DECLARATION AND THE SUBR HAS 1'; do
    grep -q "$m" callback_bad.log || bad "callback: no error '$m' ($(head -c 300 callback_bad.log))"
done
# the C entry of a callback is an internal function NAME@SIGNATURE with the C types, in the module of the target (32 bit pointers on i686)
cat > callback_ir.imp <<'E'
CALLBACK INT CMP(PTR, PTR);
CALLBACK DOUBLE HALF(DOUBLE);
EXTERNAL VOID qsort(PTR, SIZE, SIZE, PTR);
CCALL qsort(LOC(CMP),2,8,CALLBACK(CMP));
CCALL qsort(LOC(CMP),2,8,CALLBACK(HALF));
FINI(0);
SUBR CMP(A,B) IS [A]-[B];
SUBR HALF(A) IS A
%%
E
"$w" callback_ir.imp -Sir $t64 > /dev/null 2>&1 && grep -q 'define internal signext i32 @"CMP@ipp"(ptr %p0, ptr %p1)' callback_ir.ll && grep -q 'define internal double @"HALF@dd"(double %p0)' callback_ir.ll \
    || bad "callback: the C entries in the IR"
if [ $be = 0 ]; then
"$w" callback_ir.imp -Sir --target=i686-linux-gnu > /dev/null 2>&1 && grep -q 'call i64 (i32, i32) @"CMP"(i32 %a0, i32 %a1)' callback_ir.ll || bad "callback: the C entry on i686"
fi
# structure layouts: the errors, and the offsets for amd64 and i686 (a LONG and a PTR are 4 bytes there, a DOUBLE aligns to 4)
cat > struct_bad.imp <<'E'
STRUCT S1 (INT A, FOO B, VOID C, INT A, CHAR D[0], S9 E);
STRUCT S1 (INT A);
STRUCT S2 (INT A);
STRUCT S3 (S2 N, LONG L[3]);
STRUCT INT (INT A);
X:=SIZEOF(NOPE);
X:=OFFSETOF(S2,Z);
X:=OFFSETOF(NOPE,Z);
X:=FIELD(P,S2.A,1);
X:=FIELD(P,S3.L);
X:=FIELD(P,S3.N);
X:=OFFSETOF(S3,L.Q);
FINI(0)
%%
E
"$w" -c struct_bad.imp > struct_bad.log 2>&1 && bad "struct: a program with wrong declarations and accesses was accepted"
for m in 'FOO IS NOT A TYPE OF C OR A STRUCTURE DECLARED BEFORE' 'VOID IS NOT THE TYPE OF A FIELD' 'THE FIELD A IS DECLARED TWICE' 'THE LENGTH OF THE ARRAY FIELD D' 'S9 IS NOT A TYPE OF C OR A STRUCTURE DECLARED BEFORE' \
    'S1 IS DECLARED TWICE AS A STRUCTURE' 'INT IS A TYPE OF C' 'NOPE IS NOT A TYPE OF C OR A STRUCTURE' 'THE STRUCTURE S2 HAS NO FIELD Z' 'NOPE IS NOT A STRUCTURE' \
    'THE FIELD IS NOT AN ARRAY' 'THE FIELD IS AN ARRAY' 'THE FIELD IS OF A STRUCTURE TYPE' 'Q IS NOT A FIELD OF A STRUCTURE TYPE'; do
    grep -q "$m" struct_bad.log || bad "struct: no error '$m' ($(head -c 400 struct_bad.log))"
done
cat > struct_ir.imp <<'E'
STRUCT S (CHAR A, DOUBLE D, PTR P, SHORT H);
X:=SIZEOF(S);
Y:=OFFSETOF(S,P);
FIELD(X,S.H):=1;
Z:=FIELD(X,S.D);
FINI(0)
%%
E
"$w" struct_ir.imp -Sir $t64 > /dev/null 2>&1 && grep -q 'store i16' struct_ir.ll && grep -q 'add i64 %v[0-9]*, 24$' struct_ir.ll || bad "struct: the typed store on amd64"
if [ $be = 0 ]; then
"$w" struct_ir.imp -Sir --target=i686-linux-gnu > /dev/null 2>&1 && grep -q 'store i16' struct_ir.ll && grep -q 'add i64 %v[0-9]*, 16$' struct_ir.ll || bad "struct: the typed store on i686"
fi
[ $fail = 0 ] && echo ok
exit $fail
