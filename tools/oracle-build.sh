#!/bin/sh
# oracle-build.sh: set up the oracle from nothing (the work is idempotent and resumable).
# prebsd (https://github.com/moebiusV/prebsd, PREBSD=/path, else it is cloned next
# to this repository) builds the simulators from pinned sources, fetches the TOPS-10 disks, and later boots the system (tools/oracle-boot.sh).  What is here is what
# it does not do: the DECUS tape for the exact words of the compiler, and the transfer tapes.
# Downloads go to oracle/dl/<name>/ (untrusted, hashed, never run from there).  See
# oracle/PROVENANCE.md and docs/ORACLE.md.
set -e
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root"
TAPE_SHA=13180d5c1e39b3c78181f6660250562282edbbf8c9379d3e837e4b734012ac9a
mkdir -p oracle/dl/decus-tape build/oracle oracle/disks oracle/tapes oracle/units

# 1. prebsd and its KA10 simulator
PREBSD=${PREBSD:-$root/../prebsd}
[ -d "$PREBSD" ] || git clone -q https://github.com/moebiusV/prebsd.git "$PREBSD"
[ -x "$PREBSD/prebsd" ] || (cd "$PREBSD" && { [ -f Makefile ] || ./configure -q; } && make -s)
[ -x "$PREBSD/simh/bin/pdp10-ka" ] || (cd "$PREBSD" && ./build-simh pdp10-ka)

# 2. the TOPS-10 6.03 disks (PiDP-10 project).  The oracle writes to its disks, so it gets
#    copies; prebsd's stay as fetched.
if [ ! -f oracle/disks/ka_dskb0.rp3 ]; then
    (cd "$PREBSD" && ./prebsd -f tops10-603-ka)
    for n in 0 1 2 3; do cp "$PREBSD/images/ka_dskb$n.rp3" oracle/disks/; done
fi

# 3. the DECUS tape, for the exact words of imp.low, imp.shr and implib.lib
if [ ! -f decus/exact/imp.low.words ]; then
    [ -f oracle/dl/decus-tape/decuslib10-03.tap.bz2 ] || \
        curl -sSL -o oracle/dl/decus-tape/decuslib10-03.tap.bz2 https://pdp-10.trailing-edge.com/tapes/decuslib10-03.tap.bz2
    echo "$TAPE_SHA  oracle/dl/decus-tape/decuslib10-03.tap.bz2" | sha256sum -c -
    bzip2 -dkf oracle/dl/decus-tape/decuslib10-03.tap.bz2
    python3 tools/decus-exact.py oracle/dl/decus-tape/decuslib10-03.tap decus/exact
fi

# 4. an (empty) tape drive file, long enough for anything: the simulator takes the tape's size
#    from the file when it is attached
[ -f oracle/tapes/in.tap ] || python3 tools/pdp10tap.py put oracle/tapes/in.tap
cp oracle/dl/decus-tape/decuslib10-03.tap oracle/tapes/decus3.tap 2>/dev/null || :
echo "oracle built; start it with tools/oracle-boot.sh, then put the compiler on its disk:"
echo "  tools/oracle-put.py decus/exact/imp.low.words=IMP.LOW decus/exact/imp.shr.words=IMP.SHR decus/exact/implib.lib.words=IMPLIB.LIB"
