#!/bin/sh
# oracle-fuzz.sh SEED COUNT: random integer-expression programs (tools/imp-fuzz.py) on the
# original compiler and on imp72stage0; prints the programs whose output differs.
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root" || exit 1
seed=${1:-1}; count=${2:-5}
d=build/oracle/fuzz$seed
mkdir -p $d
python3 tools/${GEN:-imp-fuzz.py} $seed $count $d
python3 tools/oracle-run.py -d $d $d/*z*.imp || exit 1
bad=0
for p in $d/*z[0-9][0-9].imp; do
    n=$(basename $p .imp)
    stage0/imp72stage0 -o $d/$n $p >/dev/null 2>&1 || { echo "$n: ours does not compile"; bad=1; continue; }
    $d/$n | sed 's/[ 	]*$//' > $d/$n.got
    cmp -s $d/$n.got $d/$n.oracle && echo "$n: same" || { echo "$n: DIFFERENT"; diff $d/$n.got $d/$n.oracle | head -6; bad=1; }
done
exit $bad
