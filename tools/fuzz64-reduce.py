#!/usr/bin/env python3
"""fuzz64-reduce.py DIR N DRIVER: for the program DIR/pN.imp of tools/imp-fuzz64.py that DRIVER compiles wrongly, find the smallest sub-expressions that it gets wrong.

Each expression the program prints is parsed, every sub-expression is worked out here in 64 bit arithmetic (the same model as the generator's), and one small program that prints them all
(after the same assignments) is compiled with DRIVER and run; a sub-expression is reported when it is wrong and none of its own parts is."""
import os
import re
import subprocess
import sys

M = 1 << 64


def w(x):
    x &= M - 1
    return x - M if x >> 63 else x


def u(x):
    return x & (M - 1)


def tdiv(a, b):
    q = abs(a) // b
    return -q if a < 0 else q


TOK = re.compile(r"\s*(\(|\)|\[|\]|,|[A-Za-z_][A-Za-z0-9_]*|\d+|<=|>=|=|<|>|\+|-|\*|/)")


def tokens(s):
    out, i = [], 0
    while i < len(s):
        m = TOK.match(s, i)
        if not m:
            raise ValueError("bad text at %r" % s[i:i + 20])
        out.append(m.group(1))
        i = m.end()
    return out


class P:
    """parser of the generator's fully parenthesised expressions; every node is (text, value, children)"""

    def __init__(self, toks, env):
        self.t, self.i, self.env = toks, 0, env

    def peek(self):
        return self.t[self.i] if self.i < len(self.t) else None

    def next(self):
        self.i += 1
        return self.t[self.i - 1]

    def item(self):
        t = self.peek()
        if t == "(":
            return self.group()
        if t == "NOT":
            self.next()
            a = self.item()
            return ("(NOT %s)" % a[0], w(~a[1]), [a])
        if t == "X":
            self.next()
            self.next()                                     # [
            idx = self.item()
            self.next()                                     # ]
            return ("X[%s]" % idx[0], self.env["X"][idx[1]], [idx])
        self.next()
        if t.isdigit():
            return (t, int(t), [])
        if self.peek() == "<" and self.i + 4 < len(self.t) and self.t[self.i + 1].isdigit() and self.t[self.i + 2] == ",":
            self.next()                                     # <
            sz = int(self.next())
            self.next()                                     # ,
            pos = int(self.next())
            self.next()                                     # >
            return ("%s<%d,%d>" % (t, sz, pos), (u(self.env[t]) >> pos) & ((1 << sz) - 1), [])
        return (t, self.env[t], [])

    def group(self):
        self.next()                                         # (
        a = self.item()
        if self.peek() == ")":
            self.next()
            return a
        op = self.next()
        b = self.item()
        self.next()                                         # )
        txt = "(%s %s %s)" % (a[0], op, b[0]) if op.isalpha() or op in ("=", "<", ">") else "(%s%s%s)" % (a[0], op, b[0])
        x, y = a[1], b[1]
        if op == "+":
            v = w(x + y)
        elif op == "-":
            v = w(x - y)
        elif op == "*":
            v = w(x * y)
        elif op == "/":
            v = tdiv(x, y)
        elif op == "AND":
            v = w(x & y)
        elif op == "OR":
            v = w(x | y)
        elif op == "XOR":
            v = w(x ^ y)
        elif op == "LS":
            v = w(x << y)
        elif op == "RS":
            v = w(u(x) >> y)
        elif op == "ARS":
            v = w(x >> y)
        else:
            rel = {"=": x == y, "NE": x != y, "<": x < y, ">": x > y, "LE": x <= y, "GE": x >= y}[op]
            v = -1 if rel else 0
        return (txt, v, [a, b])


def main():
    d, n, driver = sys.argv[1], int(sys.argv[2]), sys.argv[3]
    src = open(os.path.join(d, "p%d.imp" % n)).read().split("\n")
    exp = open(os.path.join(d, "p%d.exp" % n)).read().split("\n")
    got = open(os.path.join(d, "p%d.got" % n)).read().split("\n")
    first = next((i for i, (e, g) in enumerate(zip(exp, got)) if e != g), None)
    if first is None:
        print("no difference")
        return
    env = {"X": [0] * 16}
    setup = []
    target = None
    for line in src:
        m = re.match(r"^PRINT STG 0,'(\d+) ',IGR 0,(.*),/;$", line)
        if m:
            if int(m.group(1)) == first:
                target = m.group(2)
                break
            continue
        setup.append(line)
        if line == "J:=8; K:=3;":
            env["J"], env["K"] = 8, 3
            continue
        m = re.match(r"^([A-E]):=(.*);$", line)
        if m:
            env[m.group(1)] = P(tokens(m.group(2)), env).item()[1]
        m = re.match(r"^([A-E])<(\d+),(\d+)>_(.*);$", line)
        if m:
            sz, pos = int(m.group(2)), int(m.group(3))
            mask = ((1 << sz) - 1) << pos
            ev = P(tokens(m.group(4)), env).item()[1]
            env[m.group(1)] = w((u(env[m.group(1)]) & ~mask) | ((u(ev) << pos) & mask))
            continue
        m = re.match(r"^X\[(\d+)\]:=(.*);$", line)
        if m:
            env["X"][int(m.group(1))] = P(tokens(m.group(2)), env).item()[1]
        m = re.match(r"^X\[\(J-(\d+)\)\]:=(.*);$", line)
        if m:
            env["X"][8 - int(m.group(1))] = P(tokens(m.group(2)), env).item()[1]
    if target is None:
        print("expression not found")
        return
    tree = P(tokens(target), env).item()
    subs = []

    def walk(node):
        for c in node[2]:
            walk(c)
        if node[2]:
            subs.append(node)

    walk(tree)
    seen, uniq = set(), []
    for s in subs:
        if s[0] not in seen:
            seen.add(s[0])
            uniq.append(s)
    prog = setup[:] + [l for l in []]
    prog = [l for l in prog if l.strip() and l.strip() != "%%"]
    body = prog[:]
    for k, s in enumerate(uniq):
        body.append("PRINT STG 0,'%d ',IGR 0,%s,/;" % (k, s[0]))
    body.append("FINI(0)")
    body.append("%%")
    path = os.path.join(d, "red%d.imp" % n)
    open(path, "w").write("\n".join(body) + "\n")
    exe = os.path.join(d, "red%d" % n)
    r = subprocess.run([driver, "-o", exe, path], capture_output=True, text=True)
    if r.returncode != 0:
        print("does not compile:", r.stdout[-300:], r.stderr[-300:])
        return
    out = subprocess.run([exe], capture_output=True, text=True).stdout.split("\n")
    bad = {}
    for k, s in enumerate(uniq):
        line = next((l for l in out if l.startswith("%d " % k)), None)
        gotv = int(line.split()[1]) if line else None
        if gotv != s[1]:
            bad[s[0]] = (s[1], gotv, s)
    for text, (ev, gv, node) in bad.items():
        if any(c[0] in bad for c in node[2]):
            continue
        print("WRONG: %s\n   expected %d, got %s" % (text, ev, gv))
        for c in node[2]:
            print("     operand %s = %d" % (c[0], c[1]))


if __name__ == "__main__":
    main()
