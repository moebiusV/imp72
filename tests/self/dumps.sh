#!/bin/sh
# The diagnostic print-outs (src/debug.imp in imp72s1, debug.c in stage 0) must write the same dumps: the directory (I), the parse
# trace (G), the semantics (S), the code tree (T), the syntax graph (X) and its matrices (N), for three programs.  (The P and M
# traces print absolute free storage indices, which differ between the stages: the IMP modules allocate their first arrays in
# another order.  That is not an error.)
root=$(cd "$(dirname "$0")/../.." && pwd)
cd "$root" || exit 1
export CC1=${CC1:-$root/build/pcc/cc/ccom/cc1}
tools/stage1.sh >/dev/null 2>&1; rc=$?
[ $rc = 77 ] && { echo "dumps test skipped: no objcopy"; exit 0; }
[ $rc = 0 ] || { echo "FAILED: stage1 build" >&2; exit 1; }
out=build/stage1/dumps
mkdir -p $out
fail=0
for sw in I G S T X N TSXNGI; do
    for prog in hello prog2 algo; do
        stage0/imp72s0 -s $sw -o $out/$sw.$prog.0 decus/syntax.imc tests/native/$prog.imp >/dev/null 2>&1
        build/stage1/imp72s1 -s $sw -o $out/$sw.$prog.1 decus/syntax.imc tests/native/$prog.imp >/dev/null 2>&1
        cmp -s $out/$sw.$prog.0 $out/$sw.$prog.1 && [ -s $out/$sw.$prog.0 ] \
            || { echo "DIFFERENT: -s $sw $prog" >&2; fail=1; }
    done
done
[ $fail = 0 ] && echo ok
exit $fail
