#!/bin/sh
# oracle-boot.sh: start TOPS-10 6.03 on the SIMH KA10 in the background (console log in
# build/oracle/console.log, telnet on 10603).  The boot sometimes hangs after
# "%DPA7 IS OFF-LINE" (the quickstart says so too): then it is restarted.
cd "$(dirname "$0")/../oracle" || exit 1
sim=../build/oracle/simh/BIN/pdp10-ka
log=../build/oracle/console.log
for try in 1 2 3 4 5 6; do
    for p in $(pgrep -x pdp10-ka); do kill -9 $p; done; for p in $(pgrep -fx 'tail -f /dev/null'); do kill -9 $p; done; sleep 2
    # the old sockets must be gone, or the new simulator cannot listen
    w=0; while [ $w -lt 60 ] && ss -tan | grep -q ':10603 '; do sleep 2; w=$((w + 1)); done
    : > "$log"
    (setsid sh -c "tail -f /dev/null | $sim boot.ini" > "$log" 2>&1 &)
    i=0
    while [ $i -lt 40 ]; do
        sleep 2; i=$((i + 1))
        grep -q "(CP)" "$log" && grep -q "Listening on port" "$log" && { echo "TOPS-10 is up (try $try)"; exit 0; }
    done
done
echo "TOPS-10 did not come up" >&2
exit 1
