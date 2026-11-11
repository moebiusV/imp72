#!/bin/sh
# The tables of the link for another system (src/driver.imp: LNKCROSS, SETARCH, LNKBSD): imp --target=TRIPLE --sysroot=DIR with a sysroot of empty files prints (IMPLDSHOW=1) the
# arguments of ld before lld finds them empty, and they must be those of tests/link/cross.out: the loader of each architecture and C library, the start files of each system.  The
# refusals (windows, no sysroot, no crt1.o) must say what is missing.  Needs no real sysroot.  Run from the top of the tree.
root=$(cd "$(dirname "$0")/../.." && pwd)
cd "$root" || exit 1
w=${LINK_IMP:-build/llvm-ir2/impstage-llvm-ir}
[ -x "$w" ] || { echo "link cross test skipped: no $w"; exit 0; }
out=build/linkcross
rm -rf $out; mkdir -p $out
echo "== link cross"
sr=$out/sr
mkdir -p $sr/usr/lib
for f in crt1.o crt0.o crti.o crtn.o crtbegin.o crtend.o libc.so libm.so libc.so.97.1 libm.so.10.1 libimp.a; do : > $sr/usr/lib/$f; done
for f in crt2.o libmingw32.a crt0.o libcygwin.a; do : > $sr/usr/lib/$f; done
mkdir -p $sr/usr/lib/gcc/x86_64-pc-cygwin/15 $sr/usr/lib/gcc/i686-pc-cygwin/15
for d in x86_64-pc-cygwin i686-pc-cygwin; do for f in crtbegin.o crtend.o libgcc.a; do : > $sr/usr/lib/gcc/$d/15/$f; done; done
: > $out/empty.o
got=$out/cross.got
: > $got
for t in i386-linux-gnu armv7-linux-gnueabihf armv7-linux-gnueabi aarch64-linux-gnu aarch64_be-linux-gnu riscv64-linux-gnu riscv32-linux-gnu \
         powerpc-linux-gnu powerpc64-linux-gnu powerpc64le-linux-gnu s390x-linux-gnu loongarch64-linux-gnu mips-linux-gnu mipsel-linux-gnu mips64-linux-gnuabi64 \
         sparc64-linux-gnu i386-linux-musl aarch64-linux-musl armv7-linux-musleabihf powerpc64le-linux-musl s390x-linux-musl \
         x86_64-unknown-freebsd aarch64-unknown-freebsd riscv64-unknown-freebsd x86_64-unknown-netbsd i386-unknown-netbsd aarch64-unknown-openbsd i386-unknown-openbsd \
         powerpc64-unknown-openbsd x86_64-w64-mingw32 i686-w64-mingw32 aarch64-w64-mingw32 x86_64-pc-cygwin i686-pc-cygwin; do
    # (a triple of this machine's own architecture and system is a native link, which takes its files from the running program: not in this table)
    case $t in
    *-linux-*) [ "$(uname -s)" = Linux ] && case "${t%%-*}:$(uname -m)" in x86_64:x86_64|i386:i?86|aarch64:aarch64|s390x:s390x|riscv64:riscv64|loongarch64:loongarch64|powerpc64le:ppc64le|powerpc64:ppc64|armv7:armv*) continue ;; esac ;;
    *-freebsd|*-netbsd|*-openbsd) [ "$(uname -s | tr A-Z a-z)" = "${t##*-}" ] && case "${t%%-*}:$(uname -m)" in x86_64:x86_64|x86_64:amd64|aarch64:aarch64|aarch64:arm64|i386:i?86) continue ;; esac ;;
    esac
    # (a triple whose code generator this LLVM does not have makes no object and prints no ld line: it is left out, as the golden file is cut to the triples that are in the output)
    r=$(IMPLDSHOW=1 $w --target=$t --sysroot=$root/$sr --runtime=$root/$sr/usr/lib/libimp.a -o $out/a.out tests/native/hello.imp 2>&1 >/dev/null \
        | grep -E "^ld(64)?\.lld " | sed "s#$root/$sr#SYSROOT#g; s#[^ ]*impstage[^ ]*#T#g; s#/proc/self/fd/[0-9]*#T#g; s#build/linkcross/a.out#a.out#")
    [ -n "$r" ] || continue
    echo "$t:" >> $got
    echo "$r" >> $got
done
fail=0
awk 'NR==FNR { if ($0 ~ /^[^ ]+:$/) want[$0] = 1; next } /^[^ ]+:$/ { pr = ($0 in want) } pr' $got tests/link/cross.out > $out/cross.want
cmp -s $got $out/cross.want || { echo "FAILED: link cross: the arguments of ld differ from tests/link/cross.out ($got)" >&2; diff $out/cross.want $got | head -20 >&2; fail=1; }
# the refusals say what is wrong
chk() {    # TRIPLE, SYSROOT-OPTION, expected text
    r=$($w --target=$1 $2 -o $out/b.out tests/native/hello.imp 2>&1 | head -n 1)
    case $r in *"No available targets"*) echo "link cross: $1: this LLVM has no code generator for it: not checked" ; return ;; esac
    case $r in *"$3"*) ;; *) echo "FAILED: link cross: $1: '$r' does not say '$3'" >&2; fail=1 ;; esac
}
chk x86_64-pc-windows-msvc --sysroot=$sr "linking for windows is supported with the mingw-w64 C library"
chk x86_64-pc-windows-gnu "" "no sysroot is given"
chk x86_64-pc-windows-gnu --sysroot=$out "no crt2.o and libmingw32.a under"
chk x86_64-pc-cygwin --sysroot=$out "no crt0.o and libcygwin.a under"
chk aarch64-pc-cygwin --sysroot=$sr "linking for windows on aarch64 is not supported"
chk wasm32-unknown-wasi "" "linking for wasi is not supported"
chk wasm32-unknown-unknown --sysroot=$sr "no operating system"
chk mipsel-linux-android --sysroot=$sr "android"
# (the triple of this machine itself is a native link: no table, no sysroot, no refusal)
if [ "$(uname -m)" != s390x ]; then
chk s390x-linux-gnu "" "no sysroot is given"
chk s390x-linux-gnu --sysroot=$out "no crt1.o under"
fi
chk x86_64-unknown-freebsd --sysroot=$out "no crt1.o under"
chk x86_64-apple-darwin --sysroot=$out "no usr/lib/libSystem.tbd"
chk riscv64-apple-darwin "" "macOS on riscv64 is not supported"
[ $fail = 0 ] && echo ok
exit $fail
