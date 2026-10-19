#!/bin/sh
# oracle-build.sh: set up the oracle from nothing (about 400 MB; the work is idempotent and
# resumable).  Downloads go to oracle/dl/<name>/ (untrusted, hashed, never run from there);
# the simulator is built from a copy in build/oracle/.  See oracle/PROVENANCE.md and docs/ORACLE.md.
set -e
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root"
SIMH_COMMIT=87eb7d5e
KIT_SHA=70fbf55544bb50dd6b9216cbad0bd037cb41c1fd9570ea6e94dd13735f61599d
TAPE_SHA=13180d5c1e39b3c78181f6660250562282edbbf8c9379d3e837e4b734012ac9a
mkdir -p oracle/dl/simh-src oracle/dl/kit oracle/dl/decus-tape build/oracle oracle/disks oracle/tapes oracle/units

# 1. the simulator (open-simh, KA10), with the tape patch
if [ ! -x build/oracle/simh/BIN/pdp10-ka ]; then
    [ -d oracle/dl/simh-src/open-simh ] || git clone -q https://github.com/open-simh/simh.git oracle/dl/simh-src/open-simh
    git -C oracle/dl/simh-src/open-simh checkout -q $SIMH_COMMIT 2>/dev/null || true
    rm -rf build/oracle/simh; cp -r oracle/dl/simh-src/open-simh build/oracle/simh
    python3 tools/patch-sim-tape.py build/oracle/simh/sim_tape.c
    (cd build/oracle/simh && make pdp10-ka -j4)
fi

# 2. the TOPS-10 6.03 disks (PiDP-10 project)
if [ ! -f oracle/disks/ka_dskb0.rp3 ]; then
    [ -f oracle/dl/kit/tops603ka.zip ] || curl -sSL -o oracle/dl/kit/tops603ka.zip https://obsolescence.dev/pidp10-sw/tops603ka.zip
    echo "$KIT_SHA  oracle/dl/kit/tops603ka.zip" | sha256sum -c -
    python3 -I -m zipfile -e oracle/dl/kit/tops603ka.zip oracle/disks
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
