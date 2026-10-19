#!/bin/sh
# oracle-ki-build.sh: the KI10 oracle (TOPS-10 6.03 on a KI10: it has the floating point
# conversion instructions that programs with REAL need; the KA10 of oracle-build.sh has not).
#
# The simulator is built from Richard Cornwell's tree (open-simh's pdp10-ki runs his restore
# script out of step) and the disks are made by prebsd (https://github.com/moebiusV/prebsd,
# branch tops10): PREBSD=/path/to/prebsd, else it is cloned next to this repository.
set -e
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root"
SIMS_COMMIT=be8cd4e933589faf14e23eb9d31363f2ad99b4da
mkdir -p oracle/dl/rcsims build/oracle oracle/units oracle/tapes

if [ ! -x build/oracle/rcsims/BIN/pdp10-ki ]; then
    [ -d oracle/dl/rcsims/src ] || git clone -q https://github.com/rcornwell/sims.git oracle/dl/rcsims/src
    git -C oracle/dl/rcsims/src checkout -q $SIMS_COMMIT
    rm -rf build/oracle/rcsims; cp -r oracle/dl/rcsims/src build/oracle/rcsims
    python3 tools/patch-sim-tape.py build/oracle/rcsims/sim_tape.c
    (cd build/oracle/rcsims && make pdp10-ki -j4)
fi

if [ ! -f oracle/disks-ki/ki_dskb0.rp3 ]; then
    PREBSD=${PREBSD:-$root/../prebsd}
    [ -d "$PREBSD" ] || git clone -q -b tops10 https://github.com/moebiusV/prebsd.git "$PREBSD"
    (cd "$PREBSD" && ./fetch tops10-603-ki && SIMH_KI="$root/build/oracle/rcsims/BIN/pdp10-ki" ./pdp10/install-ki.sh)
    mkdir -p oracle/disks-ki
    for n in 0 1 2 3; do cp "$PREBSD/images/ki_dskb$n.rp3" oracle/disks-ki/; done
fi
[ -f oracle/tapes/in.tap ] || python3 tools/pdp10tap.py put oracle/tapes/in.tap
echo "KI10 oracle built.  tools/oracle-boot.sh ki; then ORACLE_USER=1,2 ORACLE_PASS=FAILSA (the account of that system)"
echo "and tools/oracle-put.py the compiler as for the KA10 (docs/ORACLE.md)."
