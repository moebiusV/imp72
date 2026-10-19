#!/usr/bin/env python3
"""dump-compare.py ORACLE.lst OURS.txt: compare the compiler's debugging dumps (switches /T /S /X:
code tree, semantics, syntax graph), original against imp72s0, as streams of words, so that
differences of layout (tabs, wrapping of the bit matrices) do not count.  Prints the first
differences with some of the text before them."""
import difflib, re, sys


import re

# Semantic routines are named by address in the original and by a slot of a table here, and
# constants of the code tree by their address in free storage: the low half of such words
# is masked (they begin 03... in the semantics array and 3000010... in the tree).
ADDRESS = re.compile(r"^(03[0-7]{4}|300001)[0-7]{6}$")


def words(path, start_marker):
    t = open(path, errors="replace").read().replace("\r", "")
    i = t.find(start_marker)
    ws = t[i:].split() if i >= 0 else []
    return [w[:6] + "......" if ADDRESS.match(w) else w for w in ws]


def main(a, b, marker="** CODE GENERATION TREE"):
    import subprocess, tempfile
    x, y = words(a, marker), words(b, marker)
    print("%d words (oracle), %d words (ours)" % (len(x), len(y)))
    with tempfile.NamedTemporaryFile("w", suffix=".o") as fx, tempfile.NamedTemporaryFile("w", suffix=".u") as fy:
        fx.write("\n".join(x) + "\n")
        fy.write("\n".join(y) + "\n")
        fx.flush()
        fy.flush()
        r = subprocess.run(["diff", "-U3", fx.name, fy.name], capture_output=True, text=True)
    lines = r.stdout.split("\n")
    hunks = [l for l in lines if l.startswith("@@")]
    print("%d differing runs" % len(hunks))
    print("\n".join(lines[:80]))
    return 1 if hunks else 0


if __name__ == "__main__":
    sys.exit(main(*sys.argv[1:4]))
