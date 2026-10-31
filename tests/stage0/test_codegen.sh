#!/bin/sh
# The PDP-10 code the ported code generator makes for the programs in
# golden/ is as in golden/*.lst (listed by hand and found right; not yet
# compared with the original compiler's assembly listing).  Run from stage0/.
fail() { echo "FAILED: $*" >&2; rm -f test_codegen.out; exit 1; }
for p in ../tests/stage0/golden/*.imp; do
    b=${p%.imp}
    ./imp72s0 -s A ../tape/syntax.imc "$p" >test_codegen.out 2>/dev/null \
        || fail "$p: exit status"
    cmp -s test_codegen.out "$b.lst" || { diff test_codegen.out "$b.lst" | head; fail "$p: listing differs"; }
done
rm -f test_codegen.out
echo ok
