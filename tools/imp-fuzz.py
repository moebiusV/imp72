#!/usr/bin/env python3
"""imp-fuzz.py SEED COUNT DIR: write COUNT IMP programs of random integer expressions to DIR.

Each program sets five variables to small values and prints, with IGR 12, the value of 40
expressions built from + - * / AND OR XOR comparisons, LS, RS of a masked value and
conditionals, some fully parenthesised and some as unparenthesised chains (IMP evaluates
right to left, without precedence).  Operand magnitudes are kept far below 2**31 and shifts
are of non-negative values, so the 36 bit original and the 64 bit port must agree."""
import random, sys, os

VARS = "ABCDE"
REL = ["=", "NE", "<", ">", "LE", "GE"]


def atom(r):
    if r.random() < 0.55:
        return r.choice(VARS)
    return str(r.randint(0, 40))


def expr(r, depth):
    if depth == 0 or r.random() < 0.2:
        return atom(r)
    k = r.random()
    a, b = expr(r, depth - 1), expr(r, depth - 1)
    if k < 0.20:
        return "(%s+%s)" % (a, b)
    if k < 0.34:
        return "(%s-%s)" % (a, b)
    if k < 0.44:
        return "((%s AND 15)*(%s AND 15))" % (a, b)
    if k < 0.52:
        return "(%s/((%s AND 7)+1))" % (a, b)
    if k < 0.60:
        return "(%s AND %s)" % (a, b)
    if k < 0.66:
        return "(%s OR %s)" % (a, b)
    if k < 0.72:
        return "(%s XOR %s)" % (a, b)
    if k < 0.80:
        return "(%s %s %s)" % (a, r.choice(REL), b)
    if k < 0.85:
        return "((%s AND 255) LS %d)" % (a, r.randint(0, 8))
    if k < 0.90:
        return "((%s AND 1023) RS %d)" % (a, r.randint(0, 9))
    if k < 0.96:
        return "(%s %s %s=>%s ELSE %s)" % (a, r.choice(REL), b, expr(r, depth - 1), expr(r, depth - 1))
    return "(0-%s)" % a


def chain(r):
    n = r.randint(2, 4)
    ops = ["+", "-", "AND", "OR", "XOR", "*"]
    out = atom(r)
    for _ in range(n):
        out += " %s %s" % (r.choice(ops), atom(r))
    return out


def program(r):
    lines = []
    for v in VARS:
        lines.append("%s_%d;" % (v, r.randint(0, 20)))
    for i in range(40):
        e = chain(r) if r.random() < 0.3 else expr(r, 3)
        lines.append("PRINT IGR 12,%s,/;" % e)
    lines.append("FINI(0)")
    lines.append("%%")
    return "\n".join(lines) + "\n"


if __name__ == "__main__":
    seed, count, d = int(sys.argv[1]), int(sys.argv[2]), sys.argv[3]
    os.makedirs(d, exist_ok=True)
    for i in range(count):
        r = random.Random(seed * 1000 + i)
        open(os.path.join(d, "fz%02d.imp" % i), "w").write(program(r))
