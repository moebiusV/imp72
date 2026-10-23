#!/bin/sh
# Native tests: each tests/native/NAME.imp is compiled by imp72stage0, linked with
# NAME_main.c, run, and its output compared with NAME.out.  Run from the top
# of the repository (make check does); CC1 names the pass 2 compiler.
root=$(cd "$(dirname "$0")/../.." && pwd)
cd "$root" || exit 1
[ -x "${CC1:-build/pcc/cc/ccom/cc1}" ] || { echo "native tests skipped: no cc1 (make)"; exit 0; }
driver=${DRIVER:-stage0/imp72stage0}              # DRIVER=build/direct/imp72stage-ir with IMP72SYNTAX=src/ir/syntax64.imc: the code generator of src/ir
out=${NATIVE_OUT:-build/native}
lib=${NATIVE_LIB:-build/lib/libimp72.a}          # NATIVE_LIB=build/lib/libimp72raw.a for the code generator of src/ir
mkdir -p $out
fail=0
for p in tests/native/*.imp; do
    n=$(basename "$p" .imp)
    echo "== native $n"
    CC1=${CC1:-$root/build/pcc/cc/ccom/cc1}; export CC1
    if [ -f tests/native/${n}_main.c ]; then
        $driver -c -o $out/$n.o "$p" \
            || { echo "FAILED: $n: imp72stage0" >&2; fail=1; continue; }
        cc -no-pie -Ilib -o $out/$n tests/native/${n}_main.c $out/$n.o \
            $lib || { echo "FAILED: $n: link" >&2; fail=1; continue; }
    else
        $driver -o $out/$n "$p" \
            || { echo "FAILED: $n: imp72stage0" >&2; fail=1; continue; }
    fi
    $out/$n >$out/$n.got || { echo "FAILED: $n: run" >&2; fail=1; continue; }
    rm -f IOTEST.TMP
    cmp -s $out/$n.got tests/native/$n.out \
        || { diff $out/$n.got tests/native/$n.out; echo "FAILED: $n: output" >&2; fail=1; continue; }
    echo ok
done
exit $fail
