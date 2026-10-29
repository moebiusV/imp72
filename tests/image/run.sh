#!/bin/sh
# U and V of the tape's driver: save the compiler with the syntax and declarations it has compiled, and start from the image (-I).  A file of syntax
# is compiled in an interactive session (ext/C), the compiler saved (NAME_/U), and a program of the extended language compiled from the image must give
# the IR that the same program gives when the file of syntax is named on the command line.  Run from the top of the tree (make check does).
root=$(cd "$(dirname "$0")/../.." && pwd)
cd "$root" || exit 1
d=${IMAGE_IMPD:-build/pccir2/imp72d}
[ -x "$d" ] || { echo "image test skipped: no $d"; exit 0; }
syn="$root/src/pccir/syntax64.imc $root/lib/ffi.imc"
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
# the same through the imp72 driver: -i to save, -I to start from the image
w=${IMAGE_IMP72:-build/pccir2/imp72stage-pccir}
if [ -x "$w" ]; then
    ( cd $out && printf 'ext/C\nwmy_/U\n' | $root/$w -i >wsave.log 2>&1
      $root/$w -I wmy.img -c -o prog.o prog.imp >wload.log 2>&1 )
    [ -s $out/wmy.img ] && [ -s $out/prog.o ] || { echo "FAILED: image: imp72 -i then imp72 -I" >&2; fail=1; }
fi
[ $fail = 0 ] && echo ok
exit $fail
