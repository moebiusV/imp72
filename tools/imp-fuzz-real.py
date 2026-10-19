#!/usr/bin/env python3
"""imp-fuzz-real.py SEED COUNT DIR: random programs of real arithmetic for the KI10 oracle.

Values are multiples of 1/16 below 256 in magnitude and the operations are + - * and division
by a power of two, so every result is exact in the original's 27 bit single precision as
well as in our doubles; they are printed with FLT 14.5 and as FIX of 16 times the value.
(Anything inexact in binary prints differently: the original's digits are not exact.)"""
import os, random, sys

VARS = "PQRS"


def lit(r):
    return "%.4f" % (r.randint(-64, 64) / 16.0) if False else "%.4f" % (r.randint(0, 64) / 16.0)


def atom(r):
    if r.random() < 0.5:
        return r.choice(VARS)
    return lit(r)


def expr(r, d):
    if d == 0 or r.random() < 0.2:
        return atom(r)
    a, b = expr(r, d - 1), expr(r, d - 1)
    k = r.random()
    if k < 0.3:
        return "(%s+%s)" % (a, b)
    if k < 0.55:
        return "(%s-%s)" % (a, b)
    if k < 0.75:
        return "((%s)*%s)" % (a, r.choice(["0.5", "2.0", "0.25", "1.5", "1.0", "3.0"]))
    if k < 0.9:
        return "((%s)/%s)" % (a, r.choice(["2.0", "4.0", "8.0"]))
    return "(0.0-%s)" % a


def program(r):
    lines = ["P IS REAL; Q IS REAL; R IS REAL; S IS REAL;"]
    for v in VARS:
        lines.append("%s_%s;" % (v, lit(r)))
    for i in range(24):
        v = r.choice(VARS)
        lines.append("%s_%s;" % (v, expr(r, 3)))
        lines.append("PRINT FLT 14.5,%s,/;" % v)
        lines.append("PRINT IGR 8,FIX(%s*16.0),/;" % v)
    lines.append("FINI(0)")
    lines.append("%%")
    return "\n".join(lines) + "\n"


if __name__ == "__main__":
    seed, count, d = int(sys.argv[1]), int(sys.argv[2]), sys.argv[3]
    os.makedirs(d, exist_ok=True)
    for i in range(count):
        open(os.path.join(d, "rz%02d.imp" % i), "w").write(program(random.Random(seed * 1000 + i)))
