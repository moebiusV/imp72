#!/bin/sh
# The native tests again through the C back end (imp72c -C): no pcc involved
# at run time.  Same programs, same expected output.
root=$(cd "$(dirname "$0")/../.." && pwd)
cd "$root" || exit 1
mkdir -p build/nativec
fail=0
for p in tests/native/*.imp; do
    n=$(basename "$p" .imp)
    echo "== native C $n"
    if [ -f tests/native/${n}_main.c ]; then
        tools/imp72c -C -c -o build/nativec/$n.o "$p" \
            || { echo "FAILED: $n: imp72c -C" >&2; fail=1; continue; }
        cc -o build/nativec/$n tests/native/${n}_main.c build/nativec/$n.o build/lib/libimp72.a \
            || { echo "FAILED: $n: link" >&2; fail=1; continue; }
    else
        tools/imp72c -C -o build/nativec/$n "$p" \
            || { echo "FAILED: $n: imp72c -C" >&2; fail=1; continue; }
    fi
    build/nativec/$n >build/nativec/$n.got || { echo "FAILED: $n: run" >&2; fail=1; continue; }
    rm -f IOTEST.TMP
    cmp -s build/nativec/$n.got tests/native/$n.out \
        || { diff build/nativec/$n.got tests/native/$n.out; echo "FAILED: $n: output" >&2; fail=1; continue; }
    echo ok
done
exit $fail
