#!/bin/sh
# ieee-fuzz.sh SEED COUNT: random programs of real arithmetic (tools/imp-fuzz-ieee.py) compiled by imp72stage0 and run;
# the output must be what the generator computed for IEEE 754 binary64 itself.  Prints the programs that differ.
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root" || exit 1
seed=${1:-1}; count=${2:-10}
driver=${DRIVER:-stage0/imp72stage0}               # DRIVER=build/direct/imp72stage-ir for the code generator of src/ir
d=${FUZZ_OUT:-build/ieee-fuzz}/$seed
rm -rf $d; mkdir -p $d
python3 tools/imp-fuzz-ieee.py $seed $count $d || exit 1
bad=0
for p in $d/rz*.imp; do
    n=$(basename $p .imp)
    $driver ${IMP72_FLAGS} -o $d/$n $p >$d/$n.log 2>&1 || { echo "$n: does not compile"; head -3 $d/$n.log; bad=1; continue; }
    $d/$n | sed 's/[ 	]*$//' > $d/$n.got
    cmp -s $d/$n.got $d/$n.expected || { echo "$n: DIFFERENT"; diff $d/$n.got $d/$n.expected | head -6; bad=1; }
done
[ $bad = 0 ] && echo "ieee-fuzz $seed: $count programs agree"
exit $bad
