#!/bin/sh
# nativesrc.sh SOURCE DIR: the file to compile for the native test SOURCE (tests/native/NAME.imp): SOURCE itself, or on OpenBSD, whose srand
# ignores its seed, a copy in DIR that calls srand_deterministic, the call that makes rand repeat.
p=$1 dir=$2
if [ "$(uname -s)" = OpenBSD ] && grep -q 'srand(' "$p"; then
    n=$(basename "$p")
    sed 's/srand(/srand_deterministic(/g' "$p" > "$dir/$n" && { echo "$dir/$n"; exit 0; }
fi
echo "$p"
