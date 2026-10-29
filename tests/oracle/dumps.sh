#!/bin/sh
# The dumps and the /A listings of the original compiler (IMP 1.5 on TOPS-10, captured with the oracle: tests/golden/oracle-dumps, tests/golden/oracle-lst) against ours.
#   dumps:    the semantics array (S), the code tree and its bit matrix (T), the syntax graph (X) and the directory (I) of the seed with the original's syntax, tools/dump-compare.py
#   listings: the instructions of the programs of tests/oracle/LIST, tools/lst-compare.py
# Each difference that is left has a reason in those tools; anything else fails.  Run from the top of the tree (make check does).
root=$(cd "$(dirname "$0")/../.." && pwd)
cd "$root" || exit 1
py=
for c in ${PYTHON:-} python3 python3.13 python3.12 python3.11 python3.10 python; do
    command -v "$c" >/dev/null 2>&1 && { py=$c; break; }     # (NetBSD's python312 has no python3)
done
[ -n "$py" ] || { echo "oracle dumps skipped: no python"; exit 0; }
seed=stage0/imp72s0
[ -x $seed ] || { echo "oracle dumps skipped: no $seed"; exit 0; }
out=build/oracle-dumps
rm -rf $out; mkdir -p $out
g=tests/golden/oracle-dumps
fail=0
echo "== oracle dumps"
for k in S T X I; do
    $seed -s $k -o $out/ours$k.txt decus/syntax.imc $g/dr.imp > /dev/null 2>&1
    $py tools/dump-compare.py $k $g/dr.$k.lst $out/ours$k.txt $g/dr.imp > $out/$k.log 2>&1 || { echo "FAILED: oracle dump $k" >&2; cat $out/$k.log >&2; fail=1; }
done
echo "== oracle listings"
for n in $(cat tests/oracle/LIST); do
    f=tests/oracle/$n.imp; [ -f $f ] || f=tests/native/$n.imp
    [ -f $f ] && [ -f tests/golden/oracle-lst/$n.lst ] || continue
    $seed -s A -o $out/$n.txt decus/syntax.imc $f > /dev/null 2>&1
    $py tools/lst-compare.py tests/golden/oracle-lst/$n.lst $out/$n.txt > $out/$n.log 2>&1 || { echo "FAILED: oracle listing $n" >&2; cat $out/$n.log >&2; fail=1; }
done
[ $fail = 0 ] && echo ok
exit $fail
