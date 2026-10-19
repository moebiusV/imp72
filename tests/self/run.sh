#!/bin/sh
# Self-hosting tests: modules of the compiler's own sources, ported to the 64
# bit model (src/), compiled by imp72stage0 and run against C stand-ins for the
# modules not yet ported.  Run from the top of the tree (make check does).
root=$(cd "$(dirname "$0")/../.." && pwd)
cd "$root" || exit 1
export CC1=${CC1:-$root/build/pcc/cc/ccom/cc1}
mkdir -p build/self
fail=0
echo "== self stack"
stage0/imp72stage0 -c -o build/self/stack.o src/impsyn64.imc src/stack.imp >/dev/null 2>&1 \
    || { echo "FAILED: stack: imp72stage0" >&2; exit 1; }
stage0/imp72stage0 -c -o build/self/bit_test.o src/impsyn64.imc tests/self/bit_test.imp >/dev/null 2>&1 \
    || { echo "FAILED: bit_test: imp72stage0" >&2; exit 1; }
nopie=-no-pie; [ "$(uname)" = Darwin ] && nopie=
cc $nopie -o build/self/stack tests/self/stack_main.c build/self/stack.o build/self/bit_test.o build/lib/libimp72.a \
    || { echo "FAILED: stack: link" >&2; exit 1; }
build/self/stack >build/self/stack.got
cmp -s build/self/stack.got tests/self/stack.out \
    || { diff build/self/stack.got tests/self/stack.out; echo "FAILED: stack" >&2; exit 1; }
echo ok
exit $fail
