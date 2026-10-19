#!/bin/sh
# arm-fast.sh NAME...: the ARM back end without rebuilding anything in the container.
# The IR of tests/native/NAME.imp goes through a cc1 built on this host for the arm target
# (build/pcc-arm-host, see docs/porting/pcc-targets.md), the assembly is linked and run in
# the imp-arm container, and the output is compared with NAME.out.
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root" || exit 1
CC1=$root/build/pcc-arm-host/cc/ccom/arm-unknown-linux-gnueabihf-cc1
mkdir -p build/arm-fast
for n in "$@"; do
    syn="decus/syntax.imc"; extra=
    [ "$n" = ffi ] && extra=lib/ffi.imc
    stage0/imp72s0 -T arm -c build/arm-fast/$n.ir decus/syntax.imc $extra tests/native/$n.imp >/dev/null 2>&1 \
        || { echo "$n: imp72s0 failed"; continue; }
    $CC1 < build/arm-fast/$n.ir > build/arm-fast/$n.s || { echo "$n: cc1 failed"; continue; }
    docker cp build/arm-fast/$n.s imp-arm:/tmp/t/$n.s >/dev/null
    main=tests/native/${n}_main.c
    [ -f $main ] && docker cp $main imp-arm:/tmp/t/${n}_main.c >/dev/null
    docker exec imp-arm sh -c "cd /tmp/t && cp /work/tests/native/imp72words.h . 2>/dev/null
        if [ -f ${n}_main.c ]; then cc -no-pie -I/work/lib -o $n ${n}_main.c $n.s /work/build/lib/libimp72.a;
        else cc -no-pie -o $n /work/lib/imp72main.c $n.s /work/build/lib/libimp72.a; fi 2>&1 | head -5
        timeout 30 ./$n > $n.got 2>&1; echo rc=\$? >> $n.got" > build/arm-fast/$n.log 2>&1
    docker cp imp-arm:/tmp/t/$n.got build/arm-fast/$n.got >/dev/null
    if sed '$d' build/arm-fast/$n.got | cmp -s - tests/native/$n.out; then echo "$n: ok"; else echo "$n: DIFF"; head -4 build/arm-fast/$n.got; head -3 build/arm-fast/$n.log; fi
done
