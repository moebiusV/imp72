#!/bin/sh
# The link in the process (src/driver.imp: LNK, stage0/impld.cpp): a program is compiled to an executable with -o while no cc is on the PATH (only rm, which the
# driver runs to clean up), so lld in the process must have made the executable, and it must run and print what the native test prints.  Skipped where the
# compiler has no lld, or the architecture is not one of those whose C library is looked for.  Run from the top of the tree.
root=$(cd "$(dirname "$0")/../.." && pwd)
cd "$root" || exit 1
w=${LINK_IMP72:-build/pccir2/imp72stage-pccir}
[ -x "$w" ] || { echo "link test skipped: no $w"; exit 0; }
sh tools/llvm.sh cflags | grep -q HAVE_LLD || { echo "link test skipped: no lld library"; exit 0; }
case $(uname -m) in
x86_64|i386|i686|aarch64|arm64|armv7l|armv8l|ppc64le|s390x|riscv64|loongarch64) ;;
*) echo "link test skipped: $(uname -m) links with cc"; exit 0 ;;
esac
out=build/link
rm -rf $out; mkdir -p $out/bin
ln -s "$(command -v rm)" $out/bin/rm
echo "== link"
fail=0
PATH=$root/$out/bin $root/$w -o $out/hello tests/native/hello.imp >$out/compile.log 2>&1 || { echo "FAILED: link: the compiler" >&2; cat $out/compile.log >&2; fail=1; }
if [ $fail = 0 ]; then
    $out/hello >$out/hello.got 2>&1 || { echo "FAILED: link: the program does not run" >&2; fail=1; }
    cmp -s $out/hello.got tests/native/hello.out || { echo "FAILED: link: the output differs" >&2; fail=1; }
fi
[ $fail = 0 ] && echo ok
exit $fail
