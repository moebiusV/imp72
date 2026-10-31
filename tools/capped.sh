#!/bin/sh
# capped.sh FILE CMD [ARG...]: run CMD with its standard output and error in FILE, but at most 200 MB of them (CAP_MB changes that).  When FILE is full `head` ends and the writer gets SIGPIPE, so
# a program that prints an error for ever (a compiler in a loop) stops by itself (awk ends at the cap, as head -c would; the BSDs' head has no -c) and leaves the beginning of the log, which is what says why.  The status is CMD's, not head's.
f=$1; shift
mb=${CAP_MB:-200}
# the status of CMD through a pipe: written to a file descriptor of its own
{ "$@" 2>&1; echo $? >&3; } 3>"$f.rc" | awk -v max="${mb}000000" '{ n += length($0) + 1; if (n > max) exit; print }' > "$f"
rc=$(cat "$f.rc" 2>/dev/null); rm -f "$f.rc"
[ -n "$rc" ] || rc=141
exit "$rc"
