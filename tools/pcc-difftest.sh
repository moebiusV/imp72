#!/bin/sh
# pcc-difftest.sh CCOM DIR: compile each DIR/*.c with gcc and with pcc's ccom (CCOMFLAGS=-xuchar for the
# aarch64 Linux ABI) and compare what they print.  tests/pcc/ has the programs; run it where the
# machine is the target (an Alpine container of that architecture, tools/ci-arch.sh).
# usage: difftest.sh CCOM DIR  -- compile each DIR/*.c with gcc and with ccom, compare outputs
CCOM=$1; DIR=$2; CCOMFLAGS=${CCOMFLAGS-}
mkdir -p /dt; cd /dt
ok=0; bad=0
for f in $DIR/*.c; do
    n=$(basename $f .c)
    gcc -w -O0 -fno-builtin -o $n.gcc $f 2>/dev/null || { echo "$n: gcc cannot build"; continue; }
    ./$n.gcc > $n.exp 2>&1
    if ! $CCOM $CCOMFLAGS $f $n.s >$n.err 2>&1; then echo "$n: ccom FAILED: $(head -c 300 $n.err | tr '\n' ' ')"; bad=$((bad+1)); continue; fi
    if ! gcc -no-pie -o $n.pcc $n.s >$n.lerr 2>&1; then echo "$n: assemble/link FAILED: $(head -c 300 $n.lerr | tr '\n' ' ')"; bad=$((bad+1)); continue; fi
    ./$n.pcc > $n.out 2>&1
    if cmp -s $n.exp $n.out; then echo "$n: ok"; ok=$((ok+1)); else echo "$n: DIFFERENT: $(diff $n.exp $n.out | head -4 | tr '\n' '|')"; bad=$((bad+1)); fi
done
echo "$ok ok, $bad bad"
