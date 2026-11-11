#!/bin/sh
# The link in the process (src/driver.imc: LNK, stage0/impld.cpp): a program is compiled to an executable with -o while no cc and no rm is on the PATH (it holds at most ld.lld), so
# lld in the process must have made the executable, and it must run and print what the native test prints.  Skipped where the
# compiler has no lld.  Run from the top of the tree.
root=$(cd "$(dirname "$0")/../.." && pwd)
cd "$root" || exit 1
w=${LINK_IMP:-build/llvm-ir2/impstage-llvm-ir}
[ -x "$w" ] || { echo "link test skipped: no $w"; exit 0; }
lld=1
sh tools/llvm.sh cflags | grep -q HAVE_LLD || lld=0                 # without the lld library the compiler runs ld.lld, which has to be found by the PATH
[ $lld = 0 ] && ! command -v ld.lld >/dev/null 2>&1 && [ ! -x "$(sh tools/llvm.sh bindir)/ld.lld" ] && { echo "link test skipped: no lld library and no ld.lld"; exit 0; }
out=build/link
rm -rf $out; mkdir -p $out/bin
[ $lld = 0 ] && command -v ld.lld >/dev/null 2>&1 && ln -s "$(command -v ld.lld)" $out/bin/ld.lld         # else the compiler finds it in LLVM's bin directory
echo "== link"
fail=0
PATH=$root/$out/bin $root/$w -o $out/hello tests/native/hello.imp >$out/compile.log 2>&1 || { echo "FAILED: link: the compiler" >&2; cat $out/compile.log >&2; fail=1; }
if [ $fail = 0 ]; then
    $out/hello >$out/hello.got 2>&1 || { echo "FAILED: link: the program does not run" >&2; fail=1; }
    cmp -s $out/hello.got tests/native/hello.out || { echo "FAILED: link: the output differs" >&2; fail=1; }
fi
# the seed links the same way: the same arguments for ld (IMPLDSHOW=1 prints them; the temporary directory has a name of its own each time)
if [ $fail = 0 ] && [ -x stage0/impstage0 ]; then
    for d in "$w" stage0/impstage0; do
        n=$(basename "$d")
        rm -f $out/hello.same
        IMPLIB=$root/build/lib/libimp.a IMPLDSHOW=1 PATH=$root/$out/bin $root/$d -o $out/hello.same tests/native/hello.imp 2>&1 >/dev/null | sed 's#[^ ]*impstage0\.[A-Za-z0-9]*#T#g; s#/proc/self/fd/[0-9]*#T#g' >$out/ld.$n
    done
    [ -s $out/ld.impstage-llvm-ir ] && cmp -s $out/ld.impstage-llvm-ir $out/ld.impstage0 || { echo "FAILED: link: the seed and imp do not link with the same arguments" >&2; diff $out/ld.impstage-llvm-ir $out/ld.impstage0 >&2; fail=1; }
    $out/hello.same | cmp -s - tests/native/hello.out || { echo "FAILED: link: the program linked by the seed does not run" >&2; fail=1; }
fi
[ $fail = 0 ] && echo ok
exit $fail
