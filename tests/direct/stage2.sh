#!/bin/sh
# The code generator of src/llvm-ir compiles itself: impd (modules compiled by the old code generator, tools/direct.sh) and build/direct2/impd
# (the modules compiled by impd) must write the same IR for every program: the native tests, the oracle programs and the sources of the
# modules, the ones of the code generator among them.
root=$(cd "$(dirname "$0")/../.." && pwd)
cd "$root" || exit 1
. tools/limits.sh
tools/direct.sh >build/direct.log 2>&1; rc=$?
[ $rc = 77 ] && { echo "direct stage 2 skipped: the seed has no LLVM"; exit 0; }
[ $rc = 0 ] || { dd bs=100000 count=1 if=build/direct.log 2>/dev/null >&2; echo "FAILED: direct build" >&2; exit 1; }
export IMPSYNTAX=$root/src/syntax.imc IMPFFI=$root/lib/ffi.imc IMPLIB=$root/build/lib/libimp.a
DIRECT_OUT=build/direct2 DIRECT_DRIVER=build/direct/impstage-llvm-ir DIRECT_LIB=build/lib/libimp.a tools/direct.sh >build/direct2.log 2>&1 \
    || { dd bs=100000 count=1 if=build/direct2.log 2>/dev/null >&2; echo "FAILED: direct stage 2 build" >&2; exit 1; }
out=build/direct2/ir
mkdir -p $out
fail=0
run() {         # name files...
    n=$1; shift
    build/direct/impd -c $out/$n.1 $IMPSYNTAX $IMPFFI $root/lib/utf8.imc "$@" >/dev/null 2>&1; r1=$?
    build/direct2/impd -c $out/$n.2 $IMPSYNTAX $IMPFFI $root/lib/utf8.imc "$@" >/dev/null 2>&1; r2=$?
    if cmp -s $out/$n.1 $out/$n.2 && [ -s $out/$n.1 ]; then :; else
        echo "DIFFERENT: $n (stage 1 exit $r1, stage 2 exit $r2)" >&2; fail=1
    fi
}
for f in tests/native/*.imp tests/oracle/*.imp; do run $(basename $(dirname $f))_$(basename $f .imp) $f; done
gen="code code2 code3 demit impsem ix"       # the code generator; the other modules of src are the kernel's and the compiler's own (impsyn, impend-*, irsyn and syntax are syntax)
for f in $(ls src/*.imc | grep -v -E 'impsyn|impend-|irsyn|syntax'); do
    case " $gen " in *" $(basename $f .imc) "*) continue ;; esac
    run src_$(basename $f .imc) $(sh tools/endian.sh) src/impsyn.imc $f
done
for m in $gen; do run ir_$m $(sh tools/endian.sh) src/impsyn.imc src/irsyn.imc src/$m.imc; done
[ $fail = 0 ] && echo "direct stage 2: the code generator compiled by itself writes the same IR for every program"
exit $fail
