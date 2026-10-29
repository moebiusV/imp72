#!/usr/bin/env python3
"""imp-fuzz64.py SEED COUNT DIR: write COUNT programs of random integer expressions to DIR/pN.imp, each with the output it must print in DIR/pN.exp.

The expected output is computed here, by exact 64 bit two's complement arithmetic (the word of this IMP is 64 bits), not by any compiler: nothing in it is a compiler's answer.  The values
and constants are taken from around every width a word has had or has (the 18 bit halfword, 31, 32, 35, 36, 53 and 62 bits), with both signs, so that what a generator got wrong
in the 36 bit PDP-10 days (an 18 bit address field, a 36 bit constant) shows.  Expressions are fully parenthesised.  What the operators are:

    + - *            wrap around at 64 bits            /          truncates toward zero (the divisor is made 1..8)
    AND OR XOR NOT   on the 64 bits                    = NE < > LE GE    -1 for true, 0 for false
    LS n             shift left, n 0..63               RS n       logical shift right (zero fills)      ARS n   arithmetic shift right
    X[(J-k)] X[(J+k)] X[(J-K)] the array X, with the index J=8 (K=3) and a constant offset, read and written
    V<s,p>           the field of s bits at bit p of the variable V (s+p <= 64), read; and V<s,p>_E, stored (the other bits are kept)
"""
import random
import sys
import os

M = 1 << 64


def w(x):
    x &= M - 1
    return x - M if x >> 63 else x


def u(x):
    return x & (M - 1)


EDGES = [0, 1, 2, 3, 5, 7, 8, 9, 15, 16, 31, 100, 255, 256, 1000, 65535, 65536,
         131071, 131072, 262143, 262144, 262145, 524287, 524288, (1 << 20) + 7, (1 << 24) - 1,
         (1 << 31) - 1, 1 << 31, (1 << 31) + 1, (1 << 32) - 1, 1 << 32, (1 << 32) + 5,
         (1 << 35) - 1, 1 << 35, (1 << 35) + 1, (1 << 36) - 1, 1 << 36, (1 << 36) + 3,
         (1 << 40) + 9, (1 << 53) + 1, (1 << 62) + 5, (1 << 62) - 1]


def value(r):
    v = r.choice(EDGES) if r.random() < 0.8 else r.randint(0, 1 << r.randint(1, 62))
    return -v if r.random() < 0.4 else v


def lit(v):
    """the text of the constant v: IMP has no negative literal, so (0-N)"""
    return str(v) if v >= 0 else "(0-%d)" % (-v)


VARS = "ABCDE"
REL = [("=", lambda a, b: a == b), ("NE", lambda a, b: a != b), ("<", lambda a, b: a < b),
       (">", lambda a, b: a > b), ("LE", lambda a, b: a <= b), ("GE", lambda a, b: a >= b)]


def tdiv(a, b):
    q = abs(a) // b
    return -q if a < 0 else q


def field(r):
    """a field size and position: sizes and positions around the old halves of 18 bits, and the widths of the bytes"""
    sz = r.choice([1, 2, 3, 7, 8, 9, 12, 16, 17, 18, 19, 24, 31, 32, 33, 35, 36, 37, 48, 63])
    pos = r.choice([0, 1, 7, 8, 9, 17, 18, 19, 24, 27, 31, 32, 36, 40, 45, 55])
    while sz + pos > 64:
        pos = r.choice([0, 1, 7, 8, 18, 27])
    return sz, pos


class Env:
    def __init__(self, r):
        self.v = {n: value(r) for n in VARS}
        self.x = [value(r) for _ in range(16)]
        self.j, self.k = 8, 3


def atom(r, env):
    k = r.random()
    if k < 0.40:
        n = r.choice(VARS)
        return n, env.v[n]
    if k < 0.65:
        v = value(r)
        return lit(v), v
    if k < 0.80:
        d = r.randint(0, 7)
        form = r.random()
        if form < 0.4:
            return "X[(J-%d)]" % d, env.x[env.j - d]
        if form < 0.7:
            return "X[(J+%d)]" % d, env.x[env.j + d]
        d = r.randint(0, 4)
        return "X[((J-K)+%d)]" % d, env.x[env.j - env.k + d]
    if k < 0.90:
        d = r.randint(1, 7)
        return "X[(J-%d)]" % d, env.x[env.j - d]
    if k < 0.96:
        n = r.choice(VARS)
        sz, pos = field(r)
        return "%s<%d,%d>" % (n, sz, pos), (u(env.v[n]) >> pos) & ((1 << sz) - 1)
    v = value(r)
    return lit(v), v


def expr(r, env, depth):
    if depth == 0 or r.random() < 0.15:
        return atom(r, env)
    k = r.random()
    a, av = expr(r, env, depth - 1)
    if k < 0.06:
        return "(NOT %s)" % a, w(~av)
    if k < 0.10:
        return "(0-%s)" % a, w(-av)
    if k < 0.20:
        n = r.randint(0, 63)
        return "(%s LS %d)" % (a, n), w(av << n)
    if k < 0.28:
        n = r.randint(0, 63)
        return "(%s RS %d)" % (a, n), w(u(av) >> n)
    if k < 0.34:
        n = r.randint(0, 63)
        return "(%s ARS %d)" % (a, n), w(av >> n)
    b, bv = expr(r, env, depth - 1)
    if k < 0.46:
        return "(%s+%s)" % (a, b), w(av + bv)
    if k < 0.56:
        return "(%s-%s)" % (a, b), w(av - bv)
    if k < 0.64:
        return "(%s*%s)" % (a, b), w(av * bv)
    if k < 0.70:
        return "(%s/((%s AND 7)+1))" % (a, b), tdiv(av, (bv & 7) + 1)
    if k < 0.77:
        return "(%s AND %s)" % (a, b), w(av & bv)
    if k < 0.83:
        return "(%s OR %s)" % (a, b), w(av | bv)
    if k < 0.89:
        return "(%s XOR %s)" % (a, b), w(av ^ bv)
    name, fn = r.choice(REL)
    return "(%s %s %s)" % (a, name, b), -1 if fn(av, bv) else 0


def program(r, n):
    env = Env(r)
    lines = ["A,B,C,D,E,J,K ARE COMMON;", "X IS 16 LONG;"]
    for name in VARS:
        lines.append("%s_%s;" % (name, lit(env.v[name])))
    lines.append("J_8; K_3;")
    for i, v in enumerate(env.x):
        lines.append("X[%d]_%s;" % (i, lit(v)))
    exp = []
    for i in range(n):
        if r.random() < 0.12:                      # a store through a computed index, then the load
            d = r.randint(1, 7)
            e, ev = expr(r, env, 2)
            lines.append("X[(J-%d)]_%s;" % (d, e))
            env.x[env.j - d] = ev
            lines.append("PRINT STG 0,'%d ',IGR 0,X[(J-%d)],/;" % (i, d))
            exp.append("%d %d" % (i, ev))
            continue
        if r.random() < 0.10:                      # a store into a field, then the whole variable
            n = r.choice(VARS)
            sz, pos = field(r)
            e, ev = expr(r, env, 2)
            mask = ((1 << sz) - 1) << pos
            env.v[n] = w((u(env.v[n]) & ~mask) | ((u(ev) << pos) & mask))
            lines.append("%s<%d,%d>_%s;" % (n, sz, pos, e))
            lines.append("PRINT STG 0,'%d ',IGR 0,%s,/;" % (i, n))
            exp.append("%d %d" % (i, env.v[n]))
            continue
        e, ev = expr(r, env, r.randint(1, 4))
        lines.append("PRINT STG 0,'%d ',IGR 0,%s,/;" % (i, e))
        exp.append("%d %d" % (i, ev))
    lines.append("FINI(0)")
    lines.append("%%")
    return "\n".join(lines) + "\n", "\n".join(exp) + "\n"


def main():
    seed, count, out = int(sys.argv[1]), int(sys.argv[2]), sys.argv[3]
    os.makedirs(out, exist_ok=True)
    for i in range(count):
        r = random.Random(seed * 100003 + i)
        src, exp = program(r, 40)
        with open(os.path.join(out, "p%d.imp" % i), "w") as f:
            f.write(src)
        with open(os.path.join(out, "p%d.exp" % i), "w") as f:
            f.write(exp)


if __name__ == "__main__":
    main()
