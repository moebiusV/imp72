#!/bin/sh
# The runtime library built by imp72 itself (tools/mkrt.sh; no C compiler): linked into the programs of tests/native that need no C main, they must print what they print with the library the
# seed made (build/lib/libimp72.a).  Then the same library for other targets (aarch64, riscv64, x86_64 Mach-O): the objects are in the format of the system, and the library has what a link needs
# (main, impmain, the runtime's entry points).  Run from the top of the tree.
root=$(cd "$(dirname "$0")/../.." && pwd)
cd "$root" || exit 1
w=${RUNTIME_IMP72:-build/llvm-ir2/imp72stage-llvm-ir}
[ -x "$w" ] || { echo "runtime test skipped: no $w"; exit 0; }
out=build/runtime
rm -rf $out; mkdir -p $out
echo "== runtime"
fail=0
bad() { echo "FAILED: runtime: $*" >&2; fail=1; }
lib=$(sh tools/mkrt.sh "" $out/host 2>$out/mkrt.log) || { bad "tools/mkrt.sh: $(head -c 400 $out/mkrt.log)"; exit 1; }
export IMP72SYNTAX=$root/src/llvm-ir/syntax64.imc IMP72FFI=$root/lib/ffi.imc
for p in tests/native/*.imp; do
    n=$(basename "$p" .imp)
    [ -f tests/native/${n}_main.c ] && continue                      # these need a C main
    [ "$n" = ffi2 ] && [ "$(getconf LONG_BIT)" != 64 ] && continue
    IMP72LIB=$root/$lib "$w" -o $out/$n "$(sh tools/nativesrc.sh "$p" $out)" > $out/$n.log 2>&1 || { bad "$n: the program did not link with the library imp72 made"; continue; }
    ( cd $out && ./$n < /dev/null > $n.got 2>&1 )
    cmp -s $out/$n.got tests/native/$n.out || bad "$n: the output differs with the library imp72 made"
done
rm -f IOTEST.TMP
# the library for other systems: ELF e_machine (byte 18) of its members: x86-64 62, aarch64 183, riscv 243; Mach-O cputype x86_64 0x01000007
mach() { od -An -tu1 -v -N20 "$1" | tr -s ' \n' '  ' | awk '{print $19 + 256 * $20}'; }
have() { "$w" -c --target=$1 lib/imp72main.imp -o $out/probe.o > $out/probe.log 2>&1; ! grep -q 'No available targets' $out/probe.log; }
case $(sh tools/endian.sh 2>/dev/null) in *-be.imc) be=1 ;; *) be=0 ;; esac
if [ $be = 0 ]; then
    for t in aarch64-linux-gnu:183 riscv64-linux-gnu:243; do
        tr_=${t%:*}; em=${t#*:}
        have $tr_ || continue
        l=$(sh tools/mkrt.sh $tr_ $out/$tr_ 2>$out/mkrt-$tr_.log) || { bad "tools/mkrt.sh $tr_: $(head -c 400 $out/mkrt-$tr_.log)"; continue; }
        [ "$(mach $out/$tr_/imp72main.o)" = $em ] && [ "$(mach $out/$tr_/imp72rt_imp.o)" = $em ] || bad "$tr_: the members are not for it"
        ar t $l | grep -q imp72main.o || bad "$tr_: no main in the library"
    done
    if have x86_64-apple-darwin; then
        l=$(sh tools/mkrt.sh x86_64-apple-darwin $out/macho 2>$out/mkrt-macho.log) || bad "tools/mkrt.sh x86_64-apple-darwin: $(head -c 400 $out/mkrt-macho.log)"
        [ -n "$l" ] && [ "$(od -An -tx1 -v -N4 $out/macho/imp72main.o | tr -d ' ')" = cffaedfe ] || bad "x86_64-apple-darwin: the members are not Mach-O"
    fi
fi
[ $fail = 0 ] && echo ok
exit $fail
