#!/usr/bin/env python3
"""lst-compare.py ORACLE.lst OURS.txt: compare the instructions the original compiler assembled
(its /A listing) with the code chain imp72s0 -s A prints.

Both are reduced to the sequence of mnemonics (and the labels).  The original's listing
is after register allocation and assembly (AC numbers, absolute addresses, constants at the
end); ours is the chain before (R66 and the like), so only the instruction sequence is
compared.  Prints the differences and exits 1 if there are any."""
import re, sys

PSEUDO = {"SF0", "SF1", "SF2", "SF3", "SF4", "SF5", "CONST", "END", "000"}


def oracle(path):
    out = []
    for l in open(path):
        m = re.match(r"^\s+[0-7]{6}\s+[0-7]{6} [0-7]{6}\s+(\S+)", l)
        if m:
            mn = m.group(1)
            if mn != "000":
                out.append(mn)
            continue
        m = re.match(r"^\s{10,}(\S+:)\s*$", l)
        if m:
            out.append(m.group(1))
    return out


def ours(path):
    out = []
    for l in open(path):
        m = re.match(r"^\s*\d+\s+SF0\s+(\S+:)", l)
        if m:
            out.append(m.group(1))
            continue
        m = re.match(r"^\s*\d+\s+([A-Z][A-Z0-9.]*)\b", l)
        if m and m.group(1) not in PSEUDO:
            out.append(m.group(1))
    return out


def main(a, b):
    x, y = oracle(a), ours(b)
    if x == y:
        print("same (%d instructions and labels)" % len(x))
        return 0
    import difflib
    for l in difflib.unified_diff(x, y, "oracle", "ours", lineterm="", n=2):
        print(l)
    return 1


if __name__ == "__main__":
    sys.exit(main(*sys.argv[1:3]))
