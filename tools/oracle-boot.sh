#!/bin/sh
# oracle-boot.sh: start TOPS-10 6.03 on the SIMH KA10 or KI10 in the background, through prebsd
# (which has the simulator, drives the console through the boot and starts it again when it
# hangs after "%DPA7 IS OFF-LINE").  The operator's console is telnet 10604 (KA) or 10605 (KI),
# held open by prebsd; the user terminals are the DC lines, telnet 10603, which the other tools
# use.  The simulator's log is $PREBSD/simh/run/imp72-oracle-KIND.log.
# usage: oracle-boot.sh [ka|ki]      oracle-boot.sh stop
root=$(cd "$(dirname "$0")/.." && pwd)
PREBSD=${PREBSD:-$root/../prebsd}
kind=${1:-ka}
case $kind in
ka) sim=pdp10-ka; con=10604; common=common.ini ;;
ki) sim=pdp10-ki; con=10605; common=common-ki.ini ;;
stop) for k in ka ki; do (cd "$PREBSD" && ./prebsd -S imp72-oracle-$k >/dev/null 2>&1); done; exit 0 ;;
*) echo "usage: oracle-boot.sh [ka|ki|stop]" >&2; exit 2 ;;
esac
[ -x "$PREBSD/prebsd" ] || { echo "no $PREBSD/prebsd: run tools/oracle-build.sh" >&2; exit 1; }
name=imp72-oracle-$kind
mkdir -p "$root/build/oracle"
"$0" stop                               # the DC port is the same for both
cat > "$root/oracle/up-$kind.ini" <<EOF
; made by tools/oracle-boot.sh: the oracle's devices, and the operator's console on telnet
do $common
set console telnet=$con
boot dpa
EOF
cat > "$root/build/oracle/$name.json" <<EOF
{ "name": "$name", "ini": "$root/oracle/up-$kind.ini", "simh": "$sim", "os": "tops10",
  "ready": "(CP)", "retries": 6,
  "boot": "SYSTEM>WHY RELOAD:|SCHED>DATE:|21-APR-79>TIME:|1000>STARTUP OPTION:|QUICK>(CP)" }
EOF
cd "$PREBSD" || exit 1
if ./prebsd -k "$root/build/oracle/$name.json" > "$root/build/oracle/boot-$kind.log" 2>&1; then
    echo "TOPS-10 is up (build/oracle/boot-$kind.log)"
else
    tail -20 "$root/build/oracle/boot-$kind.log" >&2
    echo "TOPS-10 did not come up" >&2
    exit 1
fi
