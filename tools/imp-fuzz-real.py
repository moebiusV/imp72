#!/usr/bin/env python3
"""imp-fuzz-real.py SEED COUNT DIR: random programs of real arithmetic for the KI10 oracle.

Values are decimal constants and + - * / results, in 27 bit mantissa arithmetic on both sides
(the original's constants are converted by its own inexact FCON); each is printed with FLT in
two random formats and as FIX of 16 times, 128 times and the value itself."""
import os, random, sys

VARS = "PQRS"
FMTS = ["8.3", "0.2", "10.1", "-12.4", "-8.2", "12.5", "5.2", "3.1", "7.3", "0.3", "9.0", "0.0", "-6.0", "14.7"]


def lit(r):
    k = r.random()
    if k < 0.4:
        return "%.4f" % (r.randint(0, 64) / 16.0)
    if k < 0.8:
        return "%d.%d" % (r.randint(0, 300), r.randint(0, 9999))
    return "0.%d" % r.randint(1, 99999)


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
        return "((%s)*%s)" % (a, r.choice(["0.5", "2.0", "0.25", "1.5", "1.0", "3.0", "0.1", "1.1", "7.3"]))
    if k < 0.9:
        return "((%s)/%s)" % (a, r.choice(["2.0", "4.0", "8.0", "3.0", "7.0", "10.0"]))
    return "(0.0-%s)" % a


def program(r):
    lines = ["P IS REAL; Q IS REAL; R IS REAL; S IS REAL;"]
    for v in VARS:
        lines.append("%s_%s;" % (v, lit(r)))
    for i in range(24):
        v = r.choice(VARS)
        lines.append("%s_%s;" % (v, expr(r, 3)))
        lines.append("PRINT IGR 8,FIX(%s*16.0),IGR 8,FIX(%s*128.0),IGR 8,FIX(%s),/;" % (v, v, v))
        lines.append("PRINT FLT %s,%s,FLT %s,%s,/;" % (r.choice(FMTS), v, r.choice(FMTS), v))
    lines.append("FINI(0)")
    lines.append("%%")
    return "\n".join(lines) + "\n"


if __name__ == "__main__":
    seed, count, d = int(sys.argv[1]), int(sys.argv[2]), sys.argv[3]
    os.makedirs(d, exist_ok=True)
    for i in range(count):
        open(os.path.join(d, "rz%02d.imp" % i), "w").write(program(random.Random(seed * 1000 + i)))
