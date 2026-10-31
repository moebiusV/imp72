#!/bin/sh
# guard.sh [-g GB | -m MB] CMD [ARG...]: run CMD, and kill it (its whole process group) if build/ and tmp/ together grow past GB gigabytes (10 by default), so that a runaway job chokes itself and
# not the disk the other jobs, and the user's files, share.  It polls the size every second (a fast writer does 2 GB a second: the file size limit of tools/limits.sh stops a single file at 2 GB), says which directory grew, and exits with CMD's status (137 if it was killed here).
# Run from the top of the tree.  GUARD_DIRS names other directories to watch (default: build tmp); GUARD_OFF=1 turns the watch off (the limits of tools/limits.sh stay).
gb=10; mb=
[ "$1" = -g ] && { gb=$2; shift 2; }
[ "$1" = -m ] && { mb=$2; shift 2; }
[ $# -gt 0 ] || { echo "usage: guard.sh [-g GB] CMD [ARG...]" >&2; exit 2; }
[ -n "$GUARD_OFF" ] && exec "$@"
root=$(cd "$(dirname "$0")/.." && pwd)
dirs=
for d in ${GUARD_DIRS:-build tmp}; do [ -d "$root/$d" ] && dirs="$dirs $root/$d"; done
limit=$((gb * 1024 * 1024))                                  # in KB
[ -n "$mb" ] && limit=$((mb * 1024))
limtxt="$gb GB"; [ -n "$mb" ] && limtxt="$mb MB"
# CMD in a process group of its own (setsid), so that one kill reaches the compilers and the shells under it
if command -v setsid >/dev/null 2>&1; then setsid "$@" & else "$@" & fi
pid=$!
# an interrupt of the caller (^C, a kill of make) reaches the job, which is in a session of its own
trap 'pkill -KILL -g $pid 2>/dev/null; kill -KILL $pid 2>/dev/null; kill $watch 2>/dev/null; exit 130' INT TERM HUP
(
    while kill -0 $pid 2>/dev/null; do
        sleep 1
        [ -n "$dirs" ] || continue
        used=$(du -sk $dirs 2>/dev/null | awk '{ s += $1 } END { print s + 0 }')
        if [ "$used" -gt "$limit" ]; then
            echo "guard: build/ and tmp/ are $((used / 1024)) MB, over the limit of $limtxt: killing $*" >&2
            du -sk $dirs 2>/dev/null | sort -rn | head -3 | sed 's/^/guard:   /' >&2
            # the group of CMD (it leads one under setsid), else CMD alone
            pkill -KILL -g $pid 2>/dev/null || /bin/kill -KILL -- -$pid 2>/dev/null
            kill -KILL $pid 2>/dev/null
            exit 0
        fi
    done
) &
watch=$!
wait $pid
rc=$?
kill $watch 2>/dev/null
wait $watch 2>/dev/null
exit $rc
