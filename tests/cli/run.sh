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
"$w" -o prog main.imp lib2.imp >/dev/null 2>&1 && [ "$(./prog)" = "ANSWER 42" ] || bad "-o prog with two files"
"$w" -c main.imp lib2.imp >/dev/null 2>&1 && [ -s main.o ] && [ -s lib2.o ] || bad "-c with two files makes main.o and lib2.o"
"$w" main.o lib2.o -o prog2 >/dev/null 2>&1 && [ "$(./prog2)" = "ANSWER 42" ] || bad "linking the objects"
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
"$w" --target=x86_64-linux-gnu main.imp lib2.imp -o hosted >/dev/null 2>&1 && [ "$(./hosted)" = "ANSWER 42" ] || { [ "$(uname -m)" != x86_64 ] || bad "the host's own triple is a target that links"; }
[ $fail = 0 ] && echo ok
exit $fail
