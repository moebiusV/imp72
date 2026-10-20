#!/bin/sh
# Stage 1 test: imp72s1 (stage 0 with modules replaced by the compiler's own
# sources compiled by imp72stage0, tools/stage1.sh) must write the same IR as
# stage 0 for every program: the native tests and the modules of the tape.
root=$(cd "$(dirname "$0")/../.." && pwd)
cd "$root" || exit 1
export CC1=${CC1:-$root/build/pcc/cc/ccom/cc1}
tools/stage1.sh >/dev/null 2>&1; rc=$?
[ $rc = 77 ] && { echo "stage1 test skipped: no objcopy"; exit 0; }
[ $rc = 0 ] || { echo "FAILED: stage1 build" >&2; exit 1; }
out=build/stage1/ir
mkdir -p $out
fail=0
run() {         # name files...
    n=$1; shift
    stage0/imp72s0 -c $out/$n.0 "$@" >/dev/null 2>&1
    build/stage1/imp72s1 -c $out/$n.1 "$@" >/dev/null 2>&1
    if cmp -s $out/$n.0 $out/$n.1 && [ -s $out/$n.0 ]; then :; else
        echo "DIFFERENT: $n" >&2; fail=1
    fi
}
for f in tests/native/*.imp; do
    n=$(basename $f .imp)
    x=; [ $n = ffi ] && x=lib/ffi.imc
    run native_$n decus/syntax.imc $x $f
done
for f in decus/*.imc; do
    n=$(basename $f .imc)
    case $n in syntax|impsyn|rsyn|impm|impsem|dosem|free|ercor) continue ;; esac
    run mod_$n decus/syntax.imc decus/impsyn.imc $f
done
# the compiler's own sources, compiled by the compiler that has some of them compiled by itself
for f in src/*.imp; do
    n=$(basename $f .imp)
    run src_$n decus/syntax.imc src/impsyn64.imc $f
done
[ $fail = 0 ] && echo ok
exit $fail
