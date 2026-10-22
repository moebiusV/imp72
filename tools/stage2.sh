#!/bin/sh
# stage2.sh: the fixed point.  build/stage1/imp72s1 (stage 0 with the modules in src/ compiled by stage 0) is used as
# the compiler to compile the same modules again and link build/stage2/imp72s2.  The objects stage 1 and stage 2 make
# of the modules must be identical: the compiler written in IMP compiles itself to what it was compiled to.
# Run from the top of the tree after `make` and tools/stage1.sh.
set -e
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root"
[ -x build/stage1/imp72s1 ] || { echo "stage2: run tools/stage1.sh first" >&2; exit 77; }
mkdir -p build/stage2
# the driver runs by the name imp72stage*; it finds its parts through the environment
cp build/stage1/imp72s1 build/stage2/imp72stage1
export IMP72SYNTAX=$root/decus/syntax.imc IMP72FFI=$root/lib/ffi.imc CC1=${CC1:-$root/build/pcc/cc/ccom/cc1} \
    IR2C=$root/build/bin/ir2c IMP72LIB=$root/build/lib/libimp72.a
STAGE_OUT=build/stage2 STAGE_DRIVER=build/stage2/imp72stage1 STAGE_NAME=imp72s2 tools/stage1.sh
rc=0
for o in build/stage1/*.imp.o; do
    n=$(basename $o)
    if cmp -s $o build/stage2/$n; then :; else echo "stage2: $n differs from stage 1's" >&2; rc=1; fi
done
[ $rc = 0 ] && echo "stage2: every module is the same object as stage 1's"
exit $rc
