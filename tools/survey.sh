#!/bin/sh
# survey.sh: how far does stage 0 get on each IMP source of the tape?
# For each module, run imp72s0 with syntax.imc and impsyn.imc loaded, and
# print the first error (or "ok").  Run from the top of the repository.
s0=${IMP72S0:-stage0/imp72s0}
for f in decus/*.imc; do
    n=$(basename "$f" .imc)
    case $n in syntax|impsyn|rsyn) continue ;; esac
    first=$(timeout 60 "$s0" -c /dev/null decus/syntax.imc decus/impsyn.imc "$f" 2>&1 >/dev/null \
        | grep -v "^SYNTAX BOOTSTRAPPED\|ADVISORY - PRODUCTION DUPLICATES" | head -1)
    rc=$?
    printf '%-8s %s\n' "$n" "${first:-ok}"
done
