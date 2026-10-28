#!/bin/sh
# direct-fuzz.sh SEED COUNT: random programs (tools/imp-fuzz.py, GEN=imp-fuzz2.py for the structured ones) compiled by imp72stage0 (the old
# code generator) and by imp72stage-pccir (src/pccir, tools/direct.sh); the two outputs must be the same.  Prints the programs that differ.
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root" || exit 1
seed=${1:-1}; count=${2:-10}
d=build/direct-fuzz/$seed
rm -rf $d; mkdir -p $d
python3 tools/${GEN:-imp-fuzz.py} $seed $count $d >/dev/null || exit 1
export CC1=${CC1:-$root/build/pcc/cc/ccom/cc1}
bad=0
for p in $d/[a-z]z[0-9][0-9].imp; do
    n=$(basename $p .imp)
    stage0/imp72stage0 -o $d/$n.old $p >/dev/null 2>&1 || { echo "$n: the old path does not compile it"; continue; }
    IMP72SYNTAX=$root/src/pccir/syntax64.imc IMP72FFI=$root/lib/ffi.imc IMP72LIB=$root/build/lib/libimp72.a \
        build/direct/imp72stage-pccir -o $d/$n.new $p >$d/$n.log 2>&1 || { echo "$n: the pcc IR path does not compile it"; head -3 $d/$n.log; bad=1; continue; }
    $d/$n.old > $d/$n.old.out 2>&1; $d/$n.new > $d/$n.new.out 2>&1
    cmp -s $d/$n.old.out $d/$n.new.out || { echo "$n: DIFFERENT"; diff $d/$n.old.out $d/$n.new.out | head -6; bad=1; }
done
[ $bad = 0 ] && echo "direct-fuzz $seed: $count programs agree"
exit $bad
