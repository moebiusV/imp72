#!/bin/sh
# Native tests: each tests/native/NAME.imp is compiled by imp72stage0, linked with
# NAME_main.c, run, and its output compared with NAME.out.  Run from the top
# of the repository (make check does); CC1 names the pass 2 compiler.
root=$(cd "$(dirname "$0")/../.." && pwd)
cd "$root" || exit 1
[ -x "${CC1:-build/pcc/cc/ccom/cc1}" ] || { echo "native tests skipped: no cc1 (make)"; exit 0; }
mkdir -p build/native
fail=0
for p in tests/native/*.imp; do
    n=$(basename "$p" .imp)
    echo "== native $n"
    CC1=${CC1:-$root/build/pcc/cc/ccom/cc1}; export CC1
    if [ -f tests/native/${n}_main.c ]; then
        stage0/imp72stage0 -c -o build/native/$n.o "$p" \
            || { echo "FAILED: $n: imp72stage0" >&2; fail=1; continue; }
        cc -no-pie -Ilib -o build/native/$n tests/native/${n}_main.c build/native/$n.o \
            build/lib/libimp72.a || { echo "FAILED: $n: link" >&2; fail=1; continue; }
    else
        stage0/imp72stage0 -o build/native/$n "$p" \
            || { echo "FAILED: $n: imp72stage0" >&2; fail=1; continue; }
    fi
    build/native/$n >build/native/$n.got || { echo "FAILED: $n: run" >&2; fail=1; continue; }
    rm -f IOTEST.TMP
    cmp -s build/native/$n.got tests/native/$n.out \
        || { diff build/native/$n.got tests/native/$n.out; echo "FAILED: $n: output" >&2; fail=1; continue; }
    echo ok
done
exit $fail
