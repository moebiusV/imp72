#!/bin/sh
# Programs compiled and linked for Windows by this compiler (--target=x86_64-w64-mingw32, --sysroot, --runtime) and run under Wine (ROADMAP 11): the runtime library for Windows (tools/mkrt.sh: the arena by VirtualAlloc, lib/impcore-win.iml;
# the standard files of msvcrt, in binary mode, so that a program writes the bytes it means) must make them print what the native test prints.  The mingw-w64 libraries and Wine are the Debian packages, unpacked into tmp/win by tools/wine-setup.sh (nothing is installed; the
# directory stays for the next run, WINRUN_DIR names another).  Skipped, exit 77, where that cannot be done (no apt-get, no network, no user namespaces), where the compiler is not here, or where the target is not one of LLVM's here.  Run from the top of the tree.
root=$(cd "$(dirname "$0")/../.." && pwd)
cd "$root" || exit 1
w=${WINRUN_IMP:-build/llvm-ir2/impstage-llvm-ir}
[ -x "$w" ] || { echo "winrun test skipped: no $w"; exit 77; }
[ "$(uname -s)" = Linux ] && [ "$(uname -m)" = x86_64 ] || { echo "winrun test skipped: Wine is run on x86_64 Linux"; exit 77; }
d=${WINRUN_DIR:-$root/tmp/win}
sh tools/wine-setup.sh "$d" >$root/build/wine-setup.log 2>&1
r=$?
[ $r = 77 ] && { echo "winrun test skipped: no Wine ($(tail -1 build/wine-setup.log))"; exit 77; }
[ $r = 0 ] || { echo "FAILED: winrun: tools/wine-setup.sh" >&2; cat build/wine-setup.log >&2; exit 1; }
triple=x86_64-w64-mingw32
sr=$d/root/usr/x86_64-w64-mingw32
out=$root/build/winrun
seed=$root/build/win-seed/impstage0.exe
if [ "$WINRUN_SEED" = 1 ]; then       # the compiling too is done under Wine: by the seed for Windows (tools/win-seed.sh), which links with lld in its own process; the UCRT, as it has MSYS2's tree
    WINRUN_UCRT=1
    [ -f "$seed" ] || { [ "$IMP_TEST_NET" = 1 ] && sh tools/win-seed.sh >$root/build/win-seed.log 2>&1; }
    [ -f "$seed" ] || { echo "winrun (seed) test skipped: no $seed (tools/win-seed.sh makes it; IMP_TEST_NET=1 lets this test)"; exit 77; }
    out=$root/build/winrun-seed
fi
if [ "$WINRUN_UCRT" = 1 ]; then      # the same programs with the UCRT (MSYS2's mingw-w64, fetched by imp-sysroot) in place of msvcrt
    triple=x86_64-w64-mingw32-ucrt
    sh tools/imp-sysroot --quiet $triple >$root/build/winrun-sysroot.log 2>&1 || { echo "winrun (UCRT) test skipped: no sysroot for $triple ($(tail -1 build/winrun-sysroot.log))"; exit 77; }
    sr=$(sh tools/imp-sysroot --path $triple | head -1)
    [ "$WINRUN_SEED" = 1 ] || out=$root/build/winrun-ucrt
fi
rm -rf $out; mkdir -p $out
export IMPSYNTAX=$root/src/syntax.imc IMPFFI=$root/lib/ffi.imc
unset TMPDIR
fail=0 ran=0
echo "== winrun $triple"
sh tools/mkrt.sh $triple $out/rt >$out/mkrt.log 2>&1 || { echo "FAILED: winrun: libimp.a for $triple" >&2; tail -5 $out/mkrt.log >&2; exit 1; }
# (left out: structs calls strptime, cdecl htonl (ws2_32) and cdecl2 the ctype functions with values the C library of Windows treats otherwise, extcall environ: C functions that Windows lacks or has another way)
progs="hello chars names bytes algo basic case callind gotoconst gotomain printorder features callback cmath datasym fltwide io realffi chan chan2 cstring rconst sixbit counted utf8ops initmain"
list=
for n in $progs; do
    [ -f tests/native/$n.imp ] && [ ! -f tests/native/${n}_main.c ] && list="$list tests/native/$n.imp:tests/native/$n.out"
done
for n in $(cat tests/oracle/LIST); do
    p=tests/oracle/$n.imp; [ -f "$p" ] || p=tests/native/$n.imp
    g=tests/golden/oracle/$n.out; [ -f tests/golden/ieee/$n.out ] && g=tests/golden/ieee/$n.out
    [ -f tests/golden/llvm-ir/$n.out ] && g=tests/golden/llvm-ir/$n.out
    [ -f tests/native/${n}_main.c ] || list="$list $p:$g"
done
for pg in $list; do
    p=${pg%:*} g=${pg#*:}
    n=$(basename $p .imp)
    exe=$out/$n.exe
    if [ "$WINRUN_SEED" = 1 ]; then
        (cd $out && IMPSYNTAX=$root/src/syntax.imc IMPFFI=$root/lib/ffi.imc IMPLIB=$out/rt/libimp.a IMPSYSROOT=$sr WINEDEBUG=-all timeout 300 $d/run.sh $seed -o $exe $root/$p >$out/$n.log 2>&1); [ -f $exe ]
    else
        $w --target=$triple --sysroot=$sr --runtime=$out/rt/libimp.a -o $exe $p >$out/$n.log 2>&1
    fi || { echo "FAILED: winrun: $n does not compile or link" >&2; head -c 400 $out/$n.log >&2; fail=1; continue; }
    (cd $out && rm -f IOTEST.TMP && timeout 120 $d/run.sh $exe >$n.got 2>/dev/null; rm -f IOTEST.TMP)
    ran=$((ran + 1))
    if sed 's/[ 	]*$//' $out/$n.got | cmp -s - $g || cmp -s $out/$n.got $g; then rm -f $exe $out/$n.got $out/$n.log; else
        echo "FAILED: winrun: $n: the output differs ($out/$n.got)" >&2; fail=1; fi
done
[ $ran = 0 ] && [ $fail = 0 ] && { echo "winrun test skipped: nothing ran"; exit 77; }
[ $fail = 0 ] && echo "ok ($ran programs)"
exit $fail
