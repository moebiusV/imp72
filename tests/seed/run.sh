#!/bin/sh
# The seed (stage0/impstage0, C) and the stage 2 compiler (IMP) are one code generator in two languages: they must write the same LLVM IR, byte for byte, for the same program
# (docs/BOOTSTRAP.md).  Compared: the native tests, the oracle programs, the runtime library (as tools/mklib.sh compiles it) and the modules of the code generator itself.
# Run from the top of the tree.
root=$(cd "$(dirname "$0")/../.." && pwd)
cd "$root" || exit 1
w=${SEED_IMP:-build/llvm-ir2/impstage-llvm-ir}
s=stage0/impstage0
[ -x "$w" ] && [ -x "$s" ] || { echo "seed test skipped: no $w or $s"; exit 0; }
out=build/seed
rm -rf $out; mkdir -p $out
echo "== seed"
fail=0; n=0
endf=$(sh tools/endian.sh)
# one input: name, environment, files
one() {
    nm=$1; envs=$2; shift 2
    n=$((n + 1))
    env $envs IMPSYNTAX=$root/src/syntax.imc IMPFFI=$root/lib/ffi.imc "$w" -Sir -o $out/$nm.stage.ll "$@" > $out/$nm.stage.log 2>&1
    env -u IMPSYNTAX -u IMPFFI -u IMPLIB $envs "$s" -S -o $out/$nm.seed.ll "$@" > $out/$nm.seed.log 2>&1
    if [ -s $out/$nm.seed.ll ] && cmp -s $out/$nm.stage.ll $out/$nm.seed.ll; then rm -f $out/$nm.*; else
        echo "FAILED: seed: $nm: the seed and the stage compiler write different IR ($out/$nm.seed.ll, $out/$nm.stage.ll)" >&2; fail=1; fi
}
for f in tests/native/*.imp tests/oracle/*.imp; do
    one $(echo "$f" | tr / _) "X=1" "$f"
done
{ sed '/^%%$/d' lib/imprt.imp; sed '1d' lib/impio.imp; } > $out/imprt_all.imp
one rt "IMPCHARPTR=1" -C $endf $out/imprt_all.imp
for m in impsem code code2 code3 demit ix; do
    one gen_$m "IMPMODULE=$m" -C $endf -C src/impsyn.imc -C src/irsyn.imc src/$m.imc
done
[ $fail = 0 ] && echo "ok ($n programs)"
exit $fail
