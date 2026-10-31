#!/bin/sh
# The arithmetic of both code generators against the 64 bit arithmetic of tools/imp-fuzz64.py: programs of random expressions (shifts, comparisons, division, array indexing with
# offsets, values around every width a word has had), each printing what the generator itself worked out.  The seed (stage0/imp72stage0) and imp72 (the IMP generator of
# src/llvm-ir) compile the same programs.  Run from the top of the tree (make check does).
root=$(cd "$(dirname "$0")/../.." && pwd)
cd "$root" || exit 1
echo "== fuzz64"
fail=0
count=${FUZZ64_COUNT:-24}
# the integer arithmetic, the byte pointers and block moves, and the bits of REAL arithmetic over the whole range of the double (each its own generator of tools/)
for drv in stage0/imp72stage0 ${FUZZ64_IMP72:-build/llvm-ir2/imp72stage-llvm-ir}; do
    [ -x "$drv" ] || { echo "fuzz64: no $drv: skipped"; continue; }
    for gen in imp-fuzz64.py imp-fuzz-bytes.py imp-fuzz-real2.py; do
        for seed in 101 102; do
            GEN=$gen sh tools/fuzz64.sh $drv $seed $count > build/fuzz64.log 2>&1 || { echo "FAILED: fuzz64: $gen $drv seed $seed" >&2; cat build/fuzz64.log >&2; fail=1; }
        done
    done
done
[ $fail = 0 ] && echo ok
exit $fail
