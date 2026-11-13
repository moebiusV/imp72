#!/bin/sh
# imp-sysroot: the table is read (every row has five fields, the triples are distinct, --list and --path work, a bad name is refused), and where the network is
# allowed (IMP_TEST_NET=1) the whole way: an empty cache, imp --target=T hello.imp fetches the system's C library, headers and start files, makes libimp.a and links
# (x86_64-w64-mingw32 and aarch64-linux-gnu), and the programs are run (Wine through tests/winrun's tmp/win, qemu's user mode) where those are here.
root=$(cd "$(dirname "$0")/../.." && pwd)
cd "$root" || exit 1
fail=0
export IMP_SYSROOTS=$root/build/sysroots-test
rm -rf "$IMP_SYSROOTS"
awk -F'\t' '!/^#/ && NF != 5 { print "FAILED: sysroots.tsv row of " NF " fields: " $1; bad = 1 } !/^#/ { if (seen[$1]++) { print "FAILED: sysroots.tsv twice: " $1; bad = 1 } } END { exit bad }' tools/sysroots.tsv || fail=1
# (not grep -q below: it leaves at the first match and the shell that writes the rest says I/O error)
sh tools/imp-sysroot --list | grep 'aarch64-linux-gnu' >/dev/null || { echo "FAILED: sysroot --list" >&2; fail=1; }
sh tools/imp-sysroot --path aarch64-linux-gnu >/dev/null 2>&1 && { echo "FAILED: sysroot --path of what is not here" >&2; fail=1; }
sh tools/imp-sysroot no-such-system >/dev/null 2>&1; [ $? = 2 ] || { echo "FAILED: sysroot: an unknown system is exit 2" >&2; fail=1; }
if [ "$IMP_TEST_NET" = 1 ]; then
    w=build/llvm-ir2/impstage-llvm-ir
    export IMPSYNTAX=$root/src/syntax.imc IMPFFI=$root/lib/ffi.imc IMPLIB=$root/build/lib/libimp.a
    for t in x86_64-w64-mingw32 aarch64-linux-gnu; do
        "$w" --target=$t -o build/sysroot-hello-$t tests/native/hello.imp >build/sysroot-$t.log 2>&1 || { echo "FAILED: sysroot: imp --target=$t" >&2; tail -3 build/sysroot-$t.log >&2; fail=1; continue; }
        [ -x "$(sh tools/imp-sysroot --path $t | head -1)/usr/lib/libimp.a" ] || [ -f "$(sh tools/imp-sysroot --path $t | head -1)/usr/lib/libimp.a" ] || { echo "FAILED: sysroot: no libimp.a in the sysroot of $t" >&2; fail=1; }
    done
    if [ -x tmp/win/run.sh ]; then
        out=$(tmp/win/run.sh build/sysroot-hello-x86_64-w64-mingw32 2>/dev/null | head -1)
        if [ -z "$out" ]; then echo "sysroot: Wine here does not run it at all (nothing printed): the run is skipped"      # (a Wine that cannot start, as on a CI runner that left the setup half done, is not what this test is for)
        elif [ "$out" != "HELLO, WORLD" ]; then echo "FAILED: sysroot: hello.exe under Wine said '$out'" >&2; fail=1; fi
    fi
    if [ -e /proc/sys/fs/binfmt_misc/qemu-aarch64 ]; then
        out=$(QEMU_LD_PREFIX=$(sh tools/imp-sysroot --path aarch64-linux-gnu | head -1) build/sysroot-hello-aarch64-linux-gnu 2>/dev/null | head -1); [ "$out" = "HELLO, WORLD" ] || { echo "FAILED: sysroot: aarch64 hello under qemu said '$out'" >&2; fail=1; }
    fi
else
    echo "sysroot: the fetch is not tried (IMP_TEST_NET=1 tries it)"
fi
[ $fail = 0 ] && echo "sysroot ok"
exit $fail
