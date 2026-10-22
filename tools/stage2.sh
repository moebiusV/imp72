#!/bin/sh
# stage2.sh: the fixed point.  build/stage1/imp72s1 (stage 0 with the modules in src/ compiled by stage 0) is used as
# the compiler to compile the same modules again, and build/stage2/imp72s2 is linked from the result.  The compiler
# written in IMP must compile its own sources to what stage 0 compiled them to: the C text each of stage 0, stage 1 and
# stage 2 writes for every module must be the same.  (Objects are not compared: some systems, OpenBSD's RETGUARD for
# one, put random numbers in them.)  Run from the top of the tree after `make`.
set -e
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root"
tools/stage1.sh >/dev/null
mkdir -p build/stage2
# the driver runs by the name imp72stage*; it finds its parts through the environment
cp build/stage1/imp72s1 build/stage2/imp72stage1
export IMP72SYNTAX=$root/decus/syntax.imc IMP72FFI=$root/lib/ffi.imc CC1=${CC1:-$root/build/pcc/cc/ccom/cc1} \
    IR2C=$root/build/bin/ir2c IMP72LIB=$root/build/lib/libimp72.a
STAGE_OUT=build/stage2 STAGE_DRIVER=build/stage2/imp72stage1 STAGE_NAME=imp72s2 tools/stage1.sh
cp build/stage2/imp72s2 build/stage2/imp72stage2
rc=0
for f in src/*.imp; do
    n=$(basename $f .imp)
    stage0/imp72stage0 -C -S -o build/stage2/$n.0.c src/impsyn64.imc $f >/dev/null 2>&1
    build/stage2/imp72stage1 -C -S -o build/stage2/$n.1.c src/impsyn64.imc $f >/dev/null 2>&1
    build/stage2/imp72stage2 -C -S -o build/stage2/$n.2.c src/impsyn64.imc $f >/dev/null 2>&1
    if [ -s build/stage2/$n.0.c ] && cmp -s build/stage2/$n.0.c build/stage2/$n.1.c && cmp -s build/stage2/$n.1.c build/stage2/$n.2.c; then :; else
        echo "stage2: $n: stage 0, 1 and 2 do not write the same C" >&2; rc=1
    fi
done
[ $rc = 0 ] && echo "stage2: stage 0, stage 1 and stage 2 write the same C for every module of src/"
exit $rc
