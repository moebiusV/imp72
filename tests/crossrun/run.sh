#!/bin/sh
# Programs compiled and linked for other architectures by this compiler (--target, --sysroot, --runtime), and run under qemu's user mode emulation (no docker, no system
# emulation): the byte order and the pointer size of the target are taken from LLVM's data layout of it, so a big endian target (s390x, 64 bits; powerpc, 32 bits) must
# print what the native test prints.  The sysroots are made on demand from the .deb files of the prep inputs (tests/crossrun/sysroot.sh) in tmp/, and removed at the end
# (CROSSRUN_KEEP=1 keeps them).  Skipped, exit 77, where the inputs, dpkg, the compiler or the emulator's registration are not here.  Run from the top of the tree.
root=$(cd "$(dirname "$0")/../.." && pwd)
cd "$root" || exit 1
w=${CROSSRUN_IMP:-build/llvm-ir2/impstage-llvm-ir}
[ -x "$w" ] || { echo "crossrun test skipped: no $w"; exit 77; }
command -v dpkg >/dev/null 2>&1 || { echo "crossrun test skipped: no dpkg"; exit 77; }
[ -d "${PREP_INPUTS:-$root/../prebsd/mirror/dev/inputs}" ] || { echo "crossrun test skipped: no prep inputs"; exit 77; }
out=tmp/crossrun
mkdir -p $out
export IMPSYNTAX=$root/src/llvm-ir/syntax.imc IMPFFI=$root/lib/ffi.imc
fail=0 ran=0
# the programs: tests/native without a C main, and the ones whose output depends on the size of a long are left out
progs="hello chars names bytes algo basic case callind gotoconst gotomain printorder features structs cdecl cdecl2 callback cmath datasym fltwide io extcall realffi"
# one target: TRIPLE, the Debian architecture of its sysroot, how it runs (qemu NAME, or direct)
one() {
    triple=$1 darch=$2 emu=$3
    echo "== crossrun $triple"
    sr=tmp/crossrun/sysroot-$darch
    sh tests/crossrun/sysroot.sh $darch $sr >$out/$darch.log 2>&1
    r=$?
    [ $r = 77 ] && { echo "skipped: no sysroot for $darch"; return 0; }
    [ $r = 0 ] || { echo "FAILED: crossrun: the sysroot of $darch" >&2; cat $out/$darch.log >&2; fail=1; return 0; }
    case $emu in
    direct) ;;
    *) [ -e /proc/sys/fs/binfmt_misc/qemu-$emu ] || { echo "skipped: qemu-$emu is not registered"; return 0; } ;;
    esac
    rt=$out/rt-$triple
    sh tools/mkrt.sh $triple $rt >$out/mkrt-$triple.log 2>&1 || { echo "FAILED: crossrun: libimp.a for $triple" >&2; tail -5 $out/mkrt-$triple.log >&2; fail=1; return 0; }
    list=
    for n in $progs; do
        [ -f tests/native/$n.imp ] && [ ! -f tests/native/${n}_main.c ] && list="$list tests/native/$n.imp:tests/native/$n.out"
    done
    list="$list tests/crossrun/words.imp:tests/crossrun/words.out"
    for n in $(cat tests/oracle/LIST); do                     # the oracle programs, with the goldens of tests/oracle/run.sh
        p=tests/oracle/$n.imp; [ -f "$p" ] || p=tests/native/$n.imp
        g=tests/golden/oracle/$n.out; [ -f tests/golden/ieee/$n.out ] && g=tests/golden/ieee/$n.out
        [ -f tests/golden/llvm-ir/$n.out ] && g=tests/golden/llvm-ir/$n.out
        [ -f tests/native/${n}_main.c ] || list="$list $p:$g"
    done
    for pg in $list; do
        p=${pg%:*} g=${pg#*:}
        n=$(basename $p .imp)
        case $triple:$n in powerpc-*:extcall) continue ;; esac    # (impcallp reads the low half of the 64 bit word an IMP routine returns, in the first register of a 32 bit little endian machine; powerpc returns it in the pair r3:r4)
        exe=$out/$triple-$n
        $w --target=$triple --sysroot=$sr --runtime=$rt/libimp.a -o $exe $p >$out/$triple-$n.log 2>&1 || { echo "FAILED: crossrun: $triple: $n does not compile or link" >&2; head -c 400 $out/$triple-$n.log >&2; fail=1; continue; }
        rm -f IOTEST.TMP
        case $emu in
        direct) $sr/lib/ld-linux.so.2 --library-path $sr/usr/lib/i386-linux-gnu:$sr/lib/i386-linux-gnu $exe >$exe.got 2>&1 ;;
        *) QEMU_LD_PREFIX=$root/$sr $exe >$exe.got 2>&1 ;;
        esac
        rm -f IOTEST.TMP
        ran=$((ran + 1))
        if cmp -s $exe.got $g || sed 's/[ 	]*$//' $exe.got | cmp -s - $g; then rm -f $exe $exe.got $exe.log; else
            echo "FAILED: crossrun: $triple: $n: the output differs ($exe.got)" >&2; fail=1; fi
    done
}
one s390x-linux-gnu s390x s390x
one powerpc-linux-gnu powerpc ppc
one i386-linux-gnu i386 direct
[ "${CROSSRUN_KEEP:-0}" = 1 ] || { chmod -R u+w tmp/crossrun 2>/dev/null; rm -rf tmp/crossrun; }
[ $ran = 0 ] && [ $fail = 0 ] && { echo "crossrun test skipped: nothing ran"; exit 77; }
[ $fail = 0 ] && echo "ok ($ran programs)"
exit $fail
