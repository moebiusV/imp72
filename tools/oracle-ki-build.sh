#!/bin/sh
# oracle-ki-build.sh: the KI10 oracle (TOPS-10 6.03 on a KI10: it has the floating point
# conversion instructions that programs with REAL need; the KA10 of oracle-build.sh has not).
#
# prebsd (https://github.com/moebiusV/prebsd, PREBSD=/path, else it is cloned
# next to this repository) builds the simulator from Richard Cornwell's tree, fetches his system
# tape and restores it to the disks (about twenty minutes the first time; it keeps the result).
set -e
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root"
mkdir -p build/oracle oracle/units oracle/tapes oracle/disks-ki
PREBSD=${PREBSD:-$root/../prebsd}
[ -d "$PREBSD" ] || git clone -q https://github.com/moebiusV/prebsd.git "$PREBSD"
[ -x "$PREBSD/prebsd" ] || (cd "$PREBSD" && { [ -f Makefile ] || ./configure -q; } && make -s)
[ -x "$PREBSD/simh/bin/pdp10-ki" ] || (cd "$PREBSD" && ./build-simh pdp10-ki)

if [ ! -f oracle/disks-ki/ki_dskb0.rp3 ]; then
    (cd "$PREBSD" && ./prebsd -f tops10-603-ki)       # fetches the tape and makes the disks
    for n in 0 1 2 3; do cp "$PREBSD/images/ki_dskb$n.rp3" oracle/disks-ki/; done
fi
[ -f oracle/tapes/in.tap ] || python3 tools/pdp10tap.py put oracle/tapes/in.tap
echo "KI10 oracle built.  tools/oracle-boot.sh ki; then ORACLE_USER=1,2 ORACLE_PASS=FAILSA (the account of that system)"
echo "and tools/oracle-put.py the compiler as for the KA10 (docs/ORACLE.md)."
