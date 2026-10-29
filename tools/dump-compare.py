#!/usr/bin/env python3
"""dump-compare.py KIND ORACLE.lst OURS.txt [SOURCE.imp]: compare a debugging dump of the original compiler with ours, as streams of words (so that tabs and the wrapping of the bit
matrices do not count).  KIND: S (the semantics array, switch /S), T (the code tree and its bit matrix, /T), X (the syntax graph, /X) or I (the directory of names, /I).

What it takes for equal, and counts so that nothing is hidden:

  - the trailer: the original's listing follows its dump (the source, the statistics); SOURCE.imp, if given, ends the dump at the first line of the source.  Our dump comes twice (once
    when the syntax is read, once at the end); the first is compared.
  - addresses: a word of the semantics array or the tree that names a routine or a constant by its address has the address of imp.shr in the original and a slot of a table (or a place in
    our free storage) here.  Such a pair differs in the low 18 bits only; they are equal if, over the whole dump, the original's address always meets the same one of ours and the other
    way round (a one to one map).  A pair that breaks the map is a difference.
  - the matrix: the tape's loop prints one column past the matrix (the word after it in free storage: not ours to know), the last cell of the last chunk of each row, which is masked.
  - the TERMINAL lines that end the graph dump are compared as a set: they come in the order of the directory (below).
  - the directory (I): the names in the order of its slots.  Compared: the set of names (the program's own, DR, IMP, IFIX... are the original's alone; anything of ours alone is a
    difference), the hash code of each (it depends only on the name; the "#" names of 6 characters or more, which ADDCHAR makes by shifting, have a stray bit in the original's
    word that ours, with a byte to a character, does not have, and so another code: counted, not a difference), and the order of the names that remain (it depends on the order they
    were entered when two collide: counted).

Prints the counts and the first differences with some words before them; exit status 1 if there is any difference that is not one of these."""
import difflib
import os
import re
import sys

MARK = {"S": "*** SEMANTICS ***", "T": "** CODE GENERATION TREE", "X": "HERE IS THE GRAPH"}


def section(path, marker, first_only=True):
    t = open(path, errors="replace").read().replace("\r", "")
    i = t.find(marker)
    if i < 0:
        return ""
    j = t.find(marker, i + len(marker))
    return t[i:(j if first_only and j > 0 else None)]


def tokens(text, kind, source):
    if source:
        first = next((l.strip() for l in open(source, errors="replace") if l.strip()), "")
        if first:
            k = text.find(first)
            if k > 0:
                text = text[:text.rfind("\n", 0, k) + 1]
    out = []
    masked = 0
    inmatrix = False
    lines = text.split("\n")
    if kind == "X":                                  # the TERMINAL lines as a set
        k = next((n for n, l in enumerate(lines) if l.startswith("TERMINAL ")), len(lines))
        tail = sorted(l for l in lines[k:] if l.startswith("TERMINAL "))
        lines = lines[:k] + tail
    for line in lines:
        if "CODE TREE BIT MATRIX" in line:
            inmatrix = True
        w = line.split()
        if kind == "T" and inmatrix and len(w) > 2 and re.fullmatch(r"\d+", w[0]) and all(re.fullmatch(r"[01]{5}", g) for g in w[1:-1]) and re.fullmatch(r"[01]", w[-1]):
            w = w[:-1]                                   # the cell one past the matrix
            masked += 1
        out += w
    return out, masked


# names that only ours has, added by the port on purpose and documented where they are made
KNOWN_OURS = {"CVAL": "a call of a C function by value (stage0/impsem.c: not in the original)"}

# names that only the original has, and why (each looked up in the tape)
KNOWN_ORIGINAL = {
    "\\": "the backslash of the TOPS-10 command line: file specs DEV:FILE.EXT[PPN]\\PROGRAM, entered by the scanner at decus/lex.imc:96; the host's argv replaces it",
    "IMP": "the default file extension of the TOPS-10 command line (decus/impm.imc:268); how it gets into the directory was not traced, it goes with the file name handling the host's argv replaces",
    "IFIX": "entered at the start by the code generator (decus/code2.imc:22) for the library call that makes an integer of a real; imp72 uses an operation of the IR (OFIX), the seed enters it when a FIX needs it",
    "%TMP2": "a temporary RMOD, the register allocator, makes when it spills a register (decus/rmod.imc:246); LLVM allocates here",
    "U": "NOT EXPLAINED: entered before any program is read, in no production of syntax.imc or impsyn.imc and by no DIR call of the tape; the graph, tree and semantics dumps are identical, so nothing uses it",
}

ADDR = re.compile(r"^[0-7]{6}00[0-7]{4}$")          # a word whose low half is an address of imp.shr (below 0o10000), not a constant that ends in 7777


def entries(path, which):
    t = open(path, errors="replace").read().replace("\r", "")
    idx = [m.start() for m in re.finditer("HASH CODE   TOKEN", t)]
    i = idx[which]
    j = idx[which + 1] if which + 1 < len(idx) else None
    out, h = [], None
    for l in t[i:j].split("\n")[1:]:
        m = re.match(r"^ {4}([0-7]{3}) {4}(.*)$", l)
        if m:
            h = m.group(1)
            out.append((h, m.group(2).rstrip()))
            continue
        m = re.match(r"^ {11}(.*)$", l)
        if m and h is not None:
            out.append((h, m.group(1).rstrip()))
    return out


def directory(oracle, ours):
    a = entries(oracle, 0)
    b = entries(ours, len(re.findall("HASH CODE   TOKEN", open(ours, errors="replace").read())) - 1)      # (ours comes twice: the second is the end of the run, as the original's)
    na, nb = [n for _, n in a], [n for _, n in b]
    sa, sb = set(na), set(nb)
    ha, hb = dict((n, h) for h, n in a), dict((n, h) for h, n in b)
    only_o, only_u = [n for n in na if n not in sb], [n for n in nb if n not in sa]
    prog = os.path.basename(oracle).split(".")[0].upper()
    expl = ["%s: %s" % (n, KNOWN_ORIGINAL[n]) for n in only_o if n in KNOWN_ORIGINAL]
    named = [n for n in only_o if n == prog or n in ("'" + prog, "1000")]
    only_o = [n for n in only_o if n not in KNOWN_ORIGINAL and n not in named]
    added = [n for n in only_u if n in KNOWN_OURS]
    only_u = [n for n in only_u if n not in KNOWN_OURS]
    quirk = sorted(n for n in ha if n in hb and ha[n] != hb[n] and n.startswith("#") and len(n) >= 6)
    badhash = sorted(n for n in ha if n in hb and ha[n] != hb[n] and n not in quirk)
    ca = [n for n in na if n in sb and n not in quirk]
    cb = [n for n in nb if n in sa and n not in quirk]
    sm = difflib.SequenceMatcher(None, ca, cb, autojunk=False)
    hunks = sum(1 for t in sm.get_opcodes() if t[0] != "equal")
    print("I: %d names (oracle), %d (ours)" % (len(na), len(nb)))
    print("   names of the original alone, explained:")
    for e in expl:
        print("      " + e)
    print("   names of the original alone, the program's own: %s" % named)
    print("   names of the original alone, unexplained: %s" % only_o)
    print("   names of ours alone, added on purpose: %s" % ["%s (%s)" % (n, KNOWN_OURS[n]) for n in added])
    print("   names of ours alone, unexplained: %s" % only_u)
    print("   hash codes of the stray-bit '#' names, another code by design: %d %s" % (len(quirk), quirk))
    print("   hash codes that differ otherwise: %s" % badhash)
    print("   order of the %d common names: %d differing runs (collisions entered in another order), ratio %.4f" % (len(ca), hunks, sm.ratio()))
    return 1 if (only_u or only_o or badhash) else 0


def main(kind, oracle, ours, source=None):
    if kind == "I":
        return directory(oracle, ours)
    marker = MARK[kind]
    a, ma = tokens(section(oracle, marker), kind, source)
    b, mb = tokens(section(ours, marker), kind, source)
    print("%s: %d words (oracle), %d words (ours); %d past-the-matrix cells masked (oracle), %d (ours)" % (kind, len(a), len(b), ma, mb))
    sm = difflib.SequenceMatcher(None, a, b, autojunk=False)
    fwd, back, pairs, real = {}, {}, 0, []
    for tag, i1, i2, j1, j2 in sm.get_opcodes():
        if tag == "equal":
            continue
        if tag == "replace" and i2 - i1 == j2 - j1 and all(ADDR.match(x) and ADDR.match(y) and x[:6] == y[:6] for x, y in zip(a[i1:i2], b[j1:j2])):
            ok = True
            for x, y in zip(a[i1:i2], b[j1:j2]):
                if fwd.setdefault(x, y) != y or back.setdefault(y, x) != x:
                    ok = False
            if ok:
                pairs += i2 - i1
                continue
        real.append((tag, i1, i2, j1, j2))
    print("%d words are addresses that pair one to one (%d distinct); %d other differing runs" % (pairs, len(fwd), len(real)))
    for tag, i1, i2, j1, j2 in real[:12]:
        print("  %s oracle[%d:%d] ours[%d:%d]\n     before: %s\n     oracle: %s\n     ours:   %s" % (tag, i1, i2, j1, j2, " ".join(a[max(i1 - 6, 0):i1]), " ".join(a[i1:i2][:14]), " ".join(b[j1:j2][:14])))
    return 1 if real else 0


if __name__ == "__main__":
    sys.exit(main(*sys.argv[1:5]))
