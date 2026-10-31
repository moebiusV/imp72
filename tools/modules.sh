#!/bin/sh
# modules.sh: compile each module of the tape to an object with imp72stage0
# (build/mods/NAME.o) and say which ones fail.  Run from the top of the tree.
mkdir -p build/mods
ok=0 bad=0
for f in tape/*.imc; do
    n=$(basename "$f" .imc)
    case $n in syntax|impsyn|rsyn) continue ;; esac
    if stage0/imp72stage0 -c -o build/mods/$n.o tape/impsyn.imc "$f" >build/mods/$n.log 2>&1; then
        ok=$((ok + 1)); echo "ok     $n"
    else
        bad=$((bad + 1)); printf 'FAILED %s: %s\n' "$n" "$(grep -v 'ADVIS\|BOOTS' build/mods/$n.log | head -1)"
    fi
done
echo "$ok compiled, $bad failed"
