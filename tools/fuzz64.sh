#!/bin/sh
# fuzz64.sh DRIVER SEED COUNT: programs of tools/imp-fuzz64.py, each compiled with DRIVER (the seed, stage0/imp72stage0, or imp72), run, and its output compared with the one
# the generator worked out by itself in 64 bit arithmetic.  Prints the programs that differ; exit status 1 if there are any.  The programs are in tmp/fuzz64-SEED.
driver=$1 seed=$2 count=$3
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root" || exit 1
d=tmp/fuzz64-$seed
py=
for c in ${PYTHON:-} python3 python3.13 python3.12 python3.11 python3.10 python; do
    command -v "$c" >/dev/null 2>&1 && { py=$c; break; }     # (NetBSD's python312 has no python3)
done
[ -n "$py" ] || { echo "fuzz64: no python: skipped"; exit 0; }
rm -rf $d; $py -I tools/imp-fuzz64.py $seed $count $d || exit 2
bad=0
i=0
while [ $i -lt $count ]; do
    if ! $driver -o $d/p$i $d/p$i.imp >$d/p$i.log 2>&1 || ! $d/p$i > $d/p$i.got 2>&1; then
        echo "p$i: does not compile or run"; bad=$((bad+1))
    elif ! cmp -s $d/p$i.got $d/p$i.exp; then
        echo "p$i: differs: $(diff $d/p$i.exp $d/p$i.got | head -3 | tr '\n' '|')"; bad=$((bad+1))
    fi
    i=$((i+1))
done
echo "fuzz64 $seed: $bad of $count differ"
[ $bad = 0 ]
