#!/bin/sh
# Integration test: stage 0 reads the whole of the DECUS syntax file.
# Run from stage0/ (make check does).
fail() { echo "FAILED: $*" >&2; rm -f test_syntax.err test_syntax.out test_syntax.dump; exit 1; }
SYNTAX=../decus/syntax.imc

./imp72s0 -s E "$SYNTAX" >test_syntax.out 2>test_syntax.err || fail "imp72s0 exit status"
grep -q '^SYNTAX BOOTSTRAPPED$' test_syntax.err || fail "no SYNTAX BOOTSTRAPPED"
grep -q 'ERROR' test_syntax.err && fail "errors reported"

# syntax.imc has 177 syntax statements (tools/imp72_to_hbnf.py counts them
# the same way); each enters one production
n=$(grep -c '^PRODUCTION ENTERED IN GRAPH' test_syntax.out)
[ "$n" = 177 ] || fail "$n productions entered, expected 177"
# two of them are known duplicates of earlier ones
d=$(grep -c 'DUPLICATES ONE ALREADY' test_syntax.err)
[ "$d" = 2 ] || fail "$d duplicate advisories, expected 2"

# every dump runs and has its heading
./imp72s0 -s STXN -o test_syntax.dump "$SYNTAX" 2>/dev/null || fail "dump run"
for h in '\*\* CODE GENERATION TREE:' 'CODE TREE BIT MATRIX:' \
    '\*\*\* SEMANTICS \*\*\*' 'HERE IS THE GRAPH FOLLOWING ENTRY OF PRODUCTION NR\.' \
    'CONNECTIVITY MATRICES BITM AND NBITM\.'; do
    grep -q "$h" test_syntax.dump || fail "dump lacks $h"
done

rm -f test_syntax.err test_syntax.out test_syntax.dump
echo ok
