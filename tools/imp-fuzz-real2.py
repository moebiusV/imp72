#!/usr/bin/env python3
"""imp-fuzz-real2.py SEED COUNT DIR: write COUNT programs of REAL arithmetic over the whole range of the double to DIR/pN.imp, each with the output it must print in DIR/pN.exp.

tools/imp-fuzz-ieee.py checks the printed text of modest values and small constant factors.  This one checks the bits.  A REAL is an IEEE 754 binary64 held in a word, so the program prints the word
itself ([LOC(V)], the 64 bit integer that is the bits of the real) and the expected output is the bits Python's float (the same binary64, the same operations, round to nearest even) has:

    + - * /       of any two expressions, from the denormals (to 1e-320) to past overflow (1e308 * 10 is the infinity, whose bits are the same everywhere)
    0.0-X         negation (-0.0 and the zeros of both signs are kept apart by the bits)
    X < > = NE LE GE   a mask of the six, so that 0.0 = -0.0 and the order of values that differ by one bit show
    FLT(J)        an integer to a real, round to nearest even, from around 2**53 (where integers stop being exact) to 2**63-1
    FIX(X)        a real to an integer, toward zero, for |X| < 2**63 (past that the machines differ: x86 gives the least integer, aarch64 saturates)

Not generated: anything whose result is a NaN (0/0, inf-inf, inf*0: the sign and payload of the default NaN are the machine's own), and division by zero of a nonzero value is the infinity of
the right sign.  A literal is written with the exponent mark of IMP, the double quote: 1.5"300.
"""
import math
import os
import random
import struct
import sys

VARS = "PQRS"


def bits(x):
    return struct.unpack("<q", struct.pack("<d", x))[0]


def lit(r):
    k = r.random()
    if k < 0.25:
        return "%d.%d" % (r.randint(0, 9), r.randint(0, 99))
    if k < 0.5:
        return "%d.%d\"%d" % (r.randint(1, 9), r.randint(0, 9999), r.randint(-30, 30))
    if k < 0.75:
        return "%d.%d\"%d" % (r.randint(1, 9), r.randint(0, 99999), r.randint(-300, 300))
    if k < 0.9:
        return "%d.%d\"%d" % (r.randint(1, 9), r.randint(0, 99), r.choice([-320, -315, -310, -308, 306, 307, 308]))
    return r.choice(["0.0", "1.0", "2.0", "0.5", "4503599627370496.0", "9007199254740992.0", "9007199254740993.0"])


def value(s):
    return float(s.replace('"', "e"))


def atom(r, env):
    if r.random() < 0.55:
        v = r.choice(VARS)
        return v, env[v]
    s = lit(r)
    return s, value(s)


def div(a, b):
    if b == 0.0:
        if a == 0.0 or math.isnan(a):
            return float("nan")
        neg = (math.copysign(1.0, a) < 0) != (math.copysign(1.0, b) < 0)
        return -math.inf if neg else math.inf
    return a / b


def expr(r, env, depth):
    if depth == 0 or r.random() < 0.2:
        return atom(r, env)
    a, av = expr(r, env, depth - 1)
    if r.random() < 0.1:
        return "(0.0-%s)" % a, 0.0 - av
    b, bv = expr(r, env, depth - 1)
    k = r.random()
    if k < 0.3:
        return "(%s+%s)" % (a, b), av + bv
    if k < 0.55:
        return "(%s-%s)" % (a, b), av - bv
    if k < 0.8:
        return "(%s*%s)" % (a, b), av * bv
    return "(%s/%s)" % (a, b), div(av, bv)


INTS = [0, 1, 2, 7, 1000, 65535, 1 << 31, (1 << 32) + 1, (1 << 52) - 1, 1 << 52, (1 << 52) + 1, (1 << 53) - 1, 1 << 53, (1 << 53) + 1, (1 << 53) + 2, (1 << 53) + 3,
        (1 << 54) + 1, (1 << 54) + 3, (1 << 62) + 1, (1 << 63) - 1, (1 << 63) - 513, (1 << 63) - 1025]


def program(r, n):
    lines = ["P IS REAL; Q IS REAL; R IS REAL; S IS REAL;", "J,T ARE COMMON;"]
    env = {}
    exp = []
    for v in VARS:
        s = lit(r)
        lines.append("%s:=%s;" % (v, s))
        env[v] = value(s)
    for i in range(n):
        k = r.random()
        if k < 0.55:                                          # an expression into a variable; its bits
            e, x = expr(r, env, r.randint(1, 3))
            if math.isnan(x):
                continue
            v = r.choice(VARS)
            lines.append("%s:=%s;" % (v, e))
            env[v] = x
            lines.append("PRINT STG 0,'%d ',IGR 0,[LOC(%s)],/;" % (i, v))
            exp.append("%d %d" % (i, bits(x)))
        elif k < 0.70:                                        # the order of two values
            a, b = r.choice(VARS), r.choice(VARS)
            if r.random() < 0.3:                              # a value with itself
                b = a
            x, y = env[a], env[b]
            m = 0
            for bit, ok in ((1, x < y), (2, x > y), (4, x == y), (8, x != y), (16, x <= y), (32, x >= y)):
                m += bit if ok else 0
            lines.append("T:=0; (%s<%s=>T:=T+1); (%s>%s=>T:=T+2); (%s=%s=>T:=T+4); (%s NE %s=>T:=T+8); (%s LE %s=>T:=T+16); (%s GE %s=>T:=T+32);" % ((a, b) * 6))
            lines.append("PRINT STG 0,'%d ',IGR 0,T,/;" % i)
            exp.append("%d %d" % (i, m))
        elif k < 0.85:                                        # an integer to a real
            j = r.choice(INTS)
            if r.random() < 0.5:
                j = -j
            v = r.choice(VARS)
            lines.append("J:=%s;" % (j if j >= 0 else "(0-%d)" % -j))
            lines.append("%s:=FLT(J);" % v)
            env[v] = float(j)
            lines.append("PRINT STG 0,'%d ',IGR 0,[LOC(%s)],/;" % (i, v))
            exp.append("%d %d" % (i, bits(env[v])))
        else:                                                 # a real to an integer
            v = r.choice(VARS)
            if abs(env[v]) >= 2.0 ** 63 or math.isnan(env[v]):
                continue
            lines.append("PRINT STG 0,'%d ',IGR 0,FIX(%s),/;" % (i, v))
            exp.append("%d %d" % (i, int(env[v])))
    lines.append("FINI(0)")
    lines.append("%%")
    return "\n".join(lines) + "\n", "\n".join(exp) + "\n"


def main():
    seed, count, out = int(sys.argv[1]), int(sys.argv[2]), sys.argv[3]
    os.makedirs(out, exist_ok=True)
    for i in range(count):
        r = random.Random(seed * 100003 + i)
        src, exp = program(r, 60)
        with open(os.path.join(out, "p%d.imp" % i), "w") as f:
            f.write(src)
        with open(os.path.join(out, "p%d.exp" % i), "w") as f:
            f.write(exp)


if __name__ == "__main__":
    main()
