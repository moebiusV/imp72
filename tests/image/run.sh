#!/bin/sh
# U and V of the tape's driver: save the compiler with the syntax and declarations it has compiled, and start from the image (-I).  A file of syntax
# is compiled in an interactive session (ext/C), the compiler saved (NAME_/U), and a program of the extended language compiled from the image must give
# the IR that the same program gives when the file of syntax is named on the command line.  Run from the top of the tree (make check does).
root=$(cd "$(dirname "$0")/../.." && pwd)
cd "$root" || exit 1
d=${IMAGE_IMPD:-build/llvm-ir2/impd}
[ -x "$d" ] || { echo "image test skipped: no $d"; exit 0; }
syn="$root/src/llvm-ir/syntax.imc $root/lib/ffi.imc"
out=build/image
rm -rf $out; mkdir -p $out
echo "== image"
cat > $out/ext.imc <<'E'
<ATOM> ::= ZED ::= "7"%%
E
cat > $out/prog.imp <<'E'
SUBR ADD2(A) IS A+ZED
%%
E
fail=0
( cd $out && $root/$d -c base.ir $syn ext.imc prog.imp >base.log 2>&1 ) || { echo "FAILED: image: the command line" >&2; fail=1; }
( cd $out && printf 'ext/C\nmy_/U\n' | $root/$d -i $syn >save.log 2>&1 )
[ -s $out/my.img ] || { echo "FAILED: image: my.img not written" >&2; cat $out/save.log >&2; fail=1; }
( cd $out && $root/$d -I my.img -c img.ir prog.imp >load.log 2>&1 ) || { echo "FAILED: image: -I my.img" >&2; cat $out/load.log >&2; fail=1; }
cmp -s $out/base.ir $out/img.ir || { echo "FAILED: image: the IR from the image differs" >&2; fail=1; }
# the language of the image is the one it was saved with: ZED is not in the base
( cd $out && $root/$d -c plain.ir $syn prog.imp >plain.log 2>&1 )
cmp -s $out/base.ir $out/plain.ir && { echo "FAILED: image: the program compiles without the extension" >&2; fail=1; }
# the image of the base language alone gives the base language's IR
( cd $out && printf 'plain_/V\n' | $root/$d -i $syn >/dev/null 2>&1; printf 'SUBR F(A) IS A+1\n%%%%\n' > f.imp; $root/$d -c f1.ir $syn f.imp >/dev/null 2>&1; $root/$d -I plain.img -c f2.ir f.imp >/dev/null 2>&1 )
cmp -s $out/f1.ir $out/f2.ir || { echo "FAILED: image: the base language from an image differs" >&2; fail=1; }
# an image of another build is refused
printf 'junk' > $out/junk.img
( cd $out && $root/$d -I junk.img -c j.ir prog.imp >junk.log 2>&1 ) && { echo "FAILED: image: a damaged image was accepted" >&2; fail=1; }
# a compiler whose syntax was read twice has bigger tables, and arrays that have moved: its image works too
( cd $out && printf 'ext/C\ntwice_/U\n' | $root/$d -i $syn $syn >twice.log 2>&1; $root/$d -I twice.img -c twice.ir prog.imp >twice2.log 2>&1 ) || { echo "FAILED: image: the image of a compiler with its syntax read twice" >&2; fail=1; }
cmp -s $out/base.ir $out/twice.ir || { echo "FAILED: image: the IR from that image differs" >&2; fail=1; }
# an image restores with the arena at any address: saved at one (IMPCOREBASE), loaded at others, the IR is the unrelocated run's.  The arena of the load has to be
# at another address than the image's (the compiler says so with IMPCOREDEBUG); the census (IMPIMGCENSUS) of the saving says which variables of the
# image hold an address of the arena: the ones the image moves, and no word of the arena itself
if [ "$(sh "$root/tools/longbits.sh")" = 64 ]; then bsave=0x2a1234560000; bload="0x100000000000 0x3b2468ac0000 0x5c13579b0000"
else bsave=0x30123000; bload="0x50246000 0x68135000"; fi
( cd $out && printf 'ext/C\nrel_/U\n' | IMPCOREBASE=$bsave IMPIMGCENSUS=1 $root/$d -i $syn >relsave.log 2>&1 )
[ -s $out/rel.img ] || { echo "FAILED: image: rel.img not written" >&2; cat $out/relsave.log >&2; fail=1; }
grep -q '^census: 0 words of the arena hold an address of the arena' $out/relsave.log || { echo "FAILED: image: a word of the arena holds an address of the arena" >&2; grep '^census: IN-ARENA' $out/relsave.log >&2; fail=1; }
grep '^census: ARENA ' $out/relsave.log | grep -v -e '(moved)' -e '(not saved)' -e ':_25' >$out/relunk.log
[ -s $out/relunk.log ] && { echo "FAILED: image: a variable that is saved holds an address of the arena and is not moved" >&2; cat $out/relunk.log >&2; fail=1; }
grep -q '(moved)' $out/relsave.log || { echo "FAILED: image: the census found no variable to move" >&2; fail=1; }
for b in $bload; do
    ( cd $out && IMPCOREBASE=$b IMPCOREDEBUG=1 $root/$d -I rel.img -c rel.ir prog.imp >relload.log 2>&1 ) || { echo "FAILED: image: -I at $b" >&2; cat $out/relload.log >&2; fail=1; }
    cmp -s $out/base.ir $out/rel.ir || { echo "FAILED: image: the IR from the image moved to $b differs" >&2; fail=1; }
    grep -q 'the image has the arena at' $out/relload.log || echo "image: the system did not give the arena at $b (the image was not moved): not tested" >&2
done
# the same through the imp driver: -i to save, -I to start from the image
w=${IMAGE_IMP72:-build/llvm-ir2/impstage-llvm-ir}
if [ -x "$w" ]; then
    ( cd $out && printf 'ext/C\nwmy_/U\n' | $root/$w -i >wsave.log 2>&1
      $root/$w -I wmy.img -c -o prog.o prog.imp >wload.log 2>&1 )
    [ -s $out/wmy.img ] && [ -s $out/prog.o ] || { echo "FAILED: image: imp -i then imp -I" >&2; fail=1; }
fi
[ $fail = 0 ] && echo ok
exit $fail
