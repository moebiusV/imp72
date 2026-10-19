#!/bin/sh
# oracle-update.sh: make tests/golden/oracle again from the original compiler.  Start the
# oracle first (tools/oracle-boot.sh; the decus binaries must be on its disk: tools/oracle-put.py).
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root" || exit 1
progs=
for n in $(cat tests/oracle/LIST); do
    p=tests/oracle/$n.imp; [ -f "$p" ] || p=tests/native/$n.imp
    progs="$progs $p"
done
mkdir -p build/oracle/out
python3 tools/oracle-run.py -d build/oracle/out $progs || exit 1
for n in $(cat tests/oracle/LIST); do cp build/oracle/out/$n.oracle tests/golden/oracle/$n.out; done
