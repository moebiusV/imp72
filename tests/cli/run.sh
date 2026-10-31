#!/bin/sh
# The command line of imp72 as a user of the C compiler would try it: where the output goes (a.out, NAME.o, NAME.s, NAME.ll), -c with several files, -o with several outputs (refused), a program
# in two files, -S for the assembly of the target and -Sir for the LLVM IR, --target for another system (the object of it is in that system's format), --version and --help.  Run from the top of the
# tree (make check does).
root=$(cd "$(dirname "$0")/../.." && pwd)
cd "$root" || exit 1
w=${CLI_IMP72:-$root/build/pccir2/imp72stage-pccir}
[ -x "$w" ] || { echo "cli test skipped: no $w"; exit 0; }
export IMP72SYNTAX=$root/src/pccir/syntax64.imc IMP72FFI=$root/lib/ffi.imc IMP72LIB=$root/build/lib/libimp72.a
d=build/cli
rm -rf $d; mkdir -p $d
cd $d || exit 1
cat > main.imp <<'E'
N ARE COMMON;
N_TWICE(21);
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
"$w" -S lib2.imp >/dev/null 2>&1 && grep -q '\.text' lib2.s || bad "-S makes assembly (lib2.s)"
"$w" -Sir lib2.imp >/dev/null 2>&1 && grep -q '^define' lib2.ll || bad "-Sir makes LLVM IR (lib2.ll)"
"$w" -S -o named.s lib2.imp >/dev/null 2>&1 && grep -q '\.text' named.s || bad "-S -o named.s"
"$w" nosuch.imp >/dev/null 2>&1 && bad "a missing file was accepted"
"$w" -z lib2.imp >/dev/null 2>&1 && bad "an unknown option was accepted"
"$w" -o >/dev/null 2>&1 && bad "-o with no argument was accepted"
[ "$("$w" --version 2>&1)" = "imp72 0.9.0" ] || bad "--version"
"$w" --help 2>&1 | grep -q '^usage: imp72' || bad "--help"
# another system: the object is in its format (ELF, e_machine at byte 18: x86-64 62, aarch64 183, riscv 243) and 32 bit pointers for a 32 bit one
mach() { od -An -tu1 -j18 -N2 "$1" | awk '{print $1 + 256 * $2}'; }
"$w" -c --target=aarch64-linux-gnu lib2.imp -o a64.o >/dev/null 2>&1 && [ "$(mach a64.o)" = 183 ] || bad "--target=aarch64-linux-gnu"
"$w" -c --target=riscv64-linux-gnu lib2.imp -o rv.o >/dev/null 2>&1 && [ "$(mach rv.o)" = 243 ] || bad "--target=riscv64-linux-gnu"
"$w" -Sir --target=i686-linux-gnu lib2.imp -o i.ll >/dev/null 2>&1 && grep -q 'i32 %a0' i.ll || bad "--target=i686: 32 bit pointers in the IR"
"$w" -S --target=aarch64-linux-gnu lib2.imp -o a64.s >/dev/null 2>&1 && grep -q 'ret' a64.s && ! grep -q 'rdi' a64.s || bad "--target=aarch64: assembly for it"
"$w" --target=aarch64-linux-gnu main.imp lib2.imp >/dev/null 2>&1 && bad "linking for another system was attempted"
"$w" -c --target=nonsense lib2.imp >/dev/null 2>&1 && bad "a nonsense target was accepted"
if [ "$(uname -m)" = x86_64 ] && [ "$(getconf LONG_BIT)" = 64 ]; then       # the host's own architecture and system, whatever the vendor and the C library: a target that links
    "$w" --target=x86_64-linux-gnu main.imp lib2.imp -o hosted >/dev/null 2>&1 && [ "$(./hosted)" = "ANSWER 42" ] || { [ "$(uname -s)" != Linux ] || bad "the host's own triple is a target that links"; }
fi
# the commands of the original (-i): a compile writes the object, the REL file of TOPS-10 (a .o), and not the IR; a file of syntax with /C writes none; the banner has today's date; the compiler
# saved by /U shows (A) and the date of the save, and the next one saved from it (B); OBJ,LIST_SOURCE names the object and the listing
printf 'SUBR F(A) IS A+1\n%%%%\n' > okp.imp
printf '<ATOM> ::= ZED ::= "7"%%%%\n' > ext.imc
printf 'SUBR ADD2(A) IS A+ZED\n%%%%\n' > zprog.imp
printf 'okp\n' | "$w" -i > i1.log 2>&1
[ -s okp.o ] && [ ! -e okp.ir ] && [ ! -e okp.ir.ll ] || bad "-i: okp makes okp.o and no IR"
head -1 i1.log | grep -q "$(date +%d-%b-%y | tr a-z A-Z)" || bad "-i: the banner has the date of today ($(head -1 i1.log))"
rm -f okp.o
printf 'ext/C\nzprog\n' | "$w" -i > i2.log 2>&1
[ -s zprog.o ] && [ ! -e ext.o ] || bad "-i: ext/C then zprog makes zprog.o and no ext.o"
printf 'mine,mine_okp/L\n' | "$w" -i > i3.log 2>&1
[ -s mine.o ] && [ -s mine.lst ] || bad "-i: mine,mine_okp/L makes mine.o and mine.lst"
printf 'ext/C\nmy_/U\n' | "$w" -i > i4.log 2>&1
[ -s my.img ] && grep -q 'COMPILER SAVED ON my.img' i4.log || bad "-i: ext/C then my_/U saves my.img"
printf '\n' | "$w" -i -I my.img 2>&1 | head -1 | grep -q '0\.9\.0(A)  ' || bad "-i -I: the saved compiler shows (A)"
printf 'ext/C\nmy2_/V\n' | "$w" -i -I my.img > i5.log 2>&1
printf '\n' | "$w" -i -I my2.img 2>&1 | head -1 | grep -q '0\.9\.0(B)  ' || bad "-i -I: the one saved from it shows (B)"
rm -f zprog.o
printf 'zprog\n' | "$w" -i -I my.img > i6.log 2>&1
[ -s zprog.o ] || bad "-i -I: the saved compiler compiles zprog"
rm -f okp.o
printf 'okp\n' | "$w" -i --target=aarch64-linux-gnu > i7.log 2>&1
[ -s okp.o ] && [ "$(mach okp.o)" = 183 ] || bad "-i --target=aarch64-linux-gnu makes an aarch64 object"
[ $fail = 0 ] && echo ok
exit $fail
