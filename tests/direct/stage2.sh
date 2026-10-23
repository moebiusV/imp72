#!/bin/sh
# The code generator of src/ir compiles itself: imp72d (modules compiled by the old code generator, tools/direct.sh) and build/direct2/imp72d
# (the modules compiled by imp72d) must write the same IR for every program: the native tests, the oracle programs and the sources of the
# modules, the ones of src/ir among them.
root=$(cd "$(dirname "$0")/../.." && pwd)
cd "$root" || exit 1
[ -x "${CC1:-build/pcc/cc/ccom/cc1}" ] || { echo "direct stage 2 skipped: no cc1 (make)"; exit 0; }
case $(uname -m) in x86_64|amd64) ;; *) echo "direct stage 2 skipped: only tried on x86_64 so far"; exit 0 ;; esac
export CC1=${CC1:-$root/build/pcc/cc/ccom/cc1}
tools/direct.sh >build/direct.log 2>&1; rc=$?
[ $rc = 77 ] && { echo "direct stage 2 skipped: no objcopy"; exit 0; }
[ $rc = 0 ] || { cat build/direct.log >&2; echo "FAILED: direct build" >&2; exit 1; }
export IMP72SYNTAX=$root/src/ir/syntax64.imc IMP72FFI=$root/lib/ffi.imc IMP72LIB=$root/build/lib/libimp72raw.a
DIRECT_OUT=build/direct2 DIRECT_DRIVER=build/direct/imp72stage-ir DIRECT_LIB=build/lib/libimp72raw.a tools/direct.sh >build/direct2.log 2>&1 \
    || { cat build/direct2.log >&2; echo "FAILED: direct stage 2 build" >&2; exit 1; }
out=build/direct2/ir
mkdir -p $out
fail=0
run() {         # name files...
    n=$1; shift
    build/direct/imp72d -c $out/$n.1 $IMP72SYNTAX $IMP72FFI "$@" >/dev/null 2>&1; r1=$?
    build/direct2/imp72d -c $out/$n.2 $IMP72SYNTAX $IMP72FFI "$@" >/dev/null 2>&1; r2=$?
    if cmp -s $out/$n.1 $out/$n.2 && [ -s $out/$n.1 ]; then :; else
        echo "DIFFERENT: $n (stage 1 exit $r1, stage 2 exit $r2)" >&2; fail=1
    fi
}
for f in tests/native/*.imp tests/oracle/*.imp; do run $(basename $(dirname $f))_$(basename $f .imp) $f; done
for f in src/*.imp; do run src_$(basename $f .imp) src/impsyn64.imc $f; done
for f in src/ir/*.imp; do run ir_$(basename $f .imp) src/impsyn64.imc src/ir/irsyn.imc $f; done
[ $fail = 0 ] && echo "direct stage 2: the code generator compiled by itself writes the same IR for every program"
exit $fail
