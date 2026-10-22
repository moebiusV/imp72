#!/usr/bin/env python3
"""imp-fuzz-ieee.py SEED COUNT DIR: random programs of real arithmetic with their expected output.

REAL is IEEE 754 binary64, so the original compiler is no longer the reference for real arithmetic
(its 27 bit floating point differs).  This generator keeps the expression tree of every statement and evaluates it
itself, in Python floats (binary64, the same operations in the same order), and writes what the program must
print next to it: DIR/rzNN.imp and DIR/rzNN.expected.  tools/ieee-fuzz.sh compiles, runs and compares.

The program prints each value as FIX of 16 times, 128 times and the value (FIX truncates toward zero), and with
PRINT FLT in two random formats.  FLT is written here a second time, independently of lib/imp72io.c, from the
rules in docs/internals/pmod.md and LANGUAGE-DIFFERENCES.md: no 0 before the point; fixed notation with n digits
after the point unless the digits and the point do not fit the field (m wide: fewer than m-1), then .ddd"e with
m-7 digits (two, unpadded, when that is under 2); a negative m is scientific always, in -m columns, n+1 digits;
the digits are those of the exact value, ties away from zero; zero is a bare 0."""
import os
import random
import sys
from decimal import Decimal, ROUND_HALF_UP, getcontext

getcontext().prec = 1200
VARS = "PQRS"
FMTS = ["8.3", "0.2", "10.1", "-12.4", "-8.2", "12.5", "5.2", "3.1", "7.3", "0.3", "9.0", "0.0", "-6.0", "14.7",
        "20.12", "6.4", "-14.9", "11.0"]


def lit(r):
    k = r.random()
    if k < 0.4:
        return "%.4f" % (r.randint(0, 64) / 16.0)
    if k < 0.7:
        return "%d.%d" % (r.randint(0, 300), r.randint(0, 9999))
    if k < 0.9:
        return "0.%d" % r.randint(1, 99999)
    return "%d.%d\"%d" % (r.randint(1, 9), r.randint(0, 99), r.randint(-9, 9))


def value(s):
    return float(s.replace('"', "e"))


def atom(r):
    if r.random() < 0.5:
        return ("v", r.choice(VARS))
    s = lit(r)
    return ("c", s)


def expr(r, d):
    if d == 0 or r.random() < 0.2:
        return atom(r)
    a, b = expr(r, d - 1), expr(r, d - 1)
    k = r.random()
    if k < 0.3:
        return ("+", a, b)
    if k < 0.55:
        return ("-", a, b)
    if k < 0.75:
        return ("*", a, ("c", r.choice(["0.5", "2.0", "0.25", "1.5", "1.0", "3.0", "0.1", "1.1", "7.3", "100.0"])))
    if k < 0.9:
        return ("/", a, ("c", r.choice(["2.0", "4.0", "8.0", "3.0", "7.0", "10.0", "0.3"])))
    return ("neg", a)


def render(e):
    if e[0] == "v":
        return e[1]
    if e[0] == "c":
        return e[1]
    if e[0] == "neg":
        return "(0.0-%s)" % render(e[1])
    if e[0] == "*":
        return "((%s)*%s)" % (render(e[1]), render(e[2]))
    if e[0] == "/":
        return "((%s)/%s)" % (render(e[1]), render(e[2]))
    return "(%s%s%s)" % (render(e[1]), e[0], render(e[2]))


def evaluate(e, env):
    if e[0] == "v":
        return env[e[1]]
    if e[0] == "c":
        return value(e[1])
    if e[0] == "neg":
        return 0.0 - evaluate(e[1], env)
    a, b = evaluate(e[1], env), evaluate(e[2], env)
    return {"+": a + b, "-": a - b, "*": a * b, "/": a / b if b else float("inf")}[e[0]]


def fix(x):
    return int(x)                       # truncation toward zero


def flt(x, m, n):
    """PRINT FLT m.n of x, independently of lib/imp72io.c."""
    neg = x < 0
    ax = abs(Decimal(x))
    d = n
    g = m - 1 if m > 0 else (-m - 1 if m < 0 else -1)
    fixed = m >= 0
    if m < 0:
        d = n + 1
    if x == 0:
        body = "0"
    else:
        body = None
        if fixed:
            q = ax.quantize(Decimal(1).scaleb(-d), rounding=ROUND_HALF_UP)
            s = format(q, "f")
            if s.startswith("0."):
                s = s[1:]
            j = len(s.split(".")[0]) if not s.startswith(".") else 0
            if g >= 0 and (d + 1 if d else 0) + j >= g:
                fixed = False
                d = g - 6
                if d <= 1:
                    d = 2
                    g = -1
            else:
                body = ("-" if neg else "") + s
        if body is None:
            d = max(d, 1)
            e = ax.adjusted()
            q = ax.quantize(Decimal(1).scaleb(e - d + 1), rounding=ROUND_HALF_UP)
            if q.adjusted() != e:                           # 9.99 rounded up to 10.0
                e = q.adjusted()
                q = ax.quantize(Decimal(1).scaleb(e - d + 1), rounding=ROUND_HALF_UP)
            digits = str(q.scaleb(-(e - d + 1)).to_integral_value()).rjust(d, "0")[:d]
            body = ("-" if neg else "") + "." + digits + '"' + str(e + 1)
    if g >= 0 and g + 1 > len(body):
        body = " " * (g + 1 - len(body)) + body
    return body


def program(r):
    lines = ["P IS REAL; Q IS REAL; R IS REAL; S IS REAL;"]
    out = []
    env = {}
    for v in VARS:
        s = lit(r)
        lines.append("%s_%s;" % (v, s))
        env[v] = value(s)
    for _ in range(24):
        v = r.choice(VARS)
        e = expr(r, 3)
        lines.append("%s_%s;" % (v, render(e)))
        env[v] = evaluate(e, env)
        x = env[v]
        lines.append("PRINT IGR 8,FIX(%s*16.0),IGR 8,FIX(%s*128.0),IGR 8,FIX(%s),/;" % (v, v, v))
        out.append("%8d%8d%8d" % (fix(x * 16.0), fix(x * 128.0), fix(x)))
        f1, f2 = r.choice(FMTS), r.choice(FMTS)
        lines.append("PRINT FLT %s,%s,FLT %s,%s,/;" % (f1, v, f2, v))
        a, b = f1.split("."), f2.split(".")
        out.append(flt(x, int(a[0]), int(a[1])) + flt(x, int(b[0]), int(b[1])))
    lines.append("FINI(0)")
    lines.append("%%")
    return "\n".join(lines) + "\n", "\n".join(s.rstrip() for s in out) + "\n"


if __name__ == "__main__":
    seed, count, d = int(sys.argv[1]), int(sys.argv[2]), sys.argv[3]
    os.makedirs(d, exist_ok=True)
    for i in range(count):
        prog, exp = program(random.Random(seed * 1000 + i))
        open(os.path.join(d, "rz%02d.imp" % i), "w").write(prog)
        open(os.path.join(d, "rz%02d.expected" % i), "w").write(exp)
