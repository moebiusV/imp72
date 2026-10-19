#!/usr/bin/env python3
"""dump-compare.py ORACLE.lst OURS.txt: compare the compiler's debugging dumps (switches /T /S /X:
code tree, semantics, syntax graph), original against imp72s0, as streams of words, so that
differences of layout (tabs, wrapping of the bit matrices) do not count.  Prints the first
differences with some of the text before them."""
import difflib, re, sys


def words(path, start_marker):
    t = open(path, errors="replace").read().replace("\r", "")
    i = t.find(start_marker)
    return t[i:].split() if i >= 0 else []


def main(a, b, marker="** CODE GENERATION TREE"):
    x, y = words(a, marker), words(b, marker)
    print("%d words (oracle), %d words (ours)" % (len(x), len(y)))
    sm = difflib.SequenceMatcher(None, x, y, autojunk=False)
    n = 0
    for tag, i1, i2, j1, j2 in sm.get_opcodes():
        if tag == "equal":
            continue
        n += 1
        if n <= 12:
            print("%s at oracle word %d / ours %d:" % (tag, i1, j1))
            print("   before:", " ".join(x[max(0, i1 - 8):i1]))
            print("   oracle:", " ".join(x[i1:i2][:12]))
            print("   ours  :", " ".join(y[j1:j2][:12]))
    print("%d differing runs" % n)
    return 1 if n else 0


if __name__ == "__main__":
    sys.exit(main(*sys.argv[1:4]))
