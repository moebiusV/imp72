#!/usr/bin/env python3
"""probe-fields.py DRIVER: bit-field reads and stores, V<s,p> and V<s,p>_E, on a grid of sizes and positions and several values, in chunks of 50 (one function may have only so many temporaries),
each compiled with DRIVER and compared with exact 64 bit arithmetic.  Prints the (size, position) pairs that are wrong, with a count."""
import itertools
import os
import subprocess
import sys

M = 1 << 64
SIZES = [1, 2, 3, 7, 8, 9, 12, 16, 17, 18, 19, 24, 31, 32, 33, 35, 36, 37, 40, 48, 60, 62, 63]
POSS = [0, 1, 7, 8, 9, 17, 18, 19, 24, 27, 31, 32, 35, 36, 40, 45, 55]
VALS = [0x7123456789ABCDE1, -0x123456789ABCDE, 0x00000003FFFFFFFF, -1]


def w(x):
    x &= M - 1
    return x - M if x >> 63 else x


def lit(v):
    return str(v) if v >= 0 else "(0-%d)" % (-v)


def main():
    driver = sys.argv[1]
    d = "tmp/fieldprobe"
    os.makedirs(d, exist_ok=True)
    cases = []
    for v in VALS:
        for s, p in itertools.product(SIZES, POSS):
            if s + p <= 64:
                cases.append(("r", v, s, p))
    for s, p in itertools.product(SIZES, POSS):
        if s + p <= 64:
            cases.append(("w", VALS[0], s, p))
    bad = {}
    for c0 in range(0, len(cases), 50):
        chunk = cases[c0:c0 + 50]
        lines, exp = ["A,B ARE COMMON;"], []
        for n, (kind, v, s, p) in enumerate(chunk):
            if kind == "r":
                lines.append("A:=%s; PRINT STG 0,'%d ',IGR 0,A<%d,%d>,/;" % (lit(v), n, s, p))
                exp.append((n, ((v & (M - 1)) >> p) & ((1 << s) - 1)))
            else:
                new = 0x5A5A5A5A5A5A5A5 & ((1 << s) - 1)
                mask = ((1 << s) - 1) << p
                lines.append("A:=%s; A<%d,%d>:=%d; PRINT STG 0,'%d ',IGR 0,A,/;" % (lit(v), s, p, new, n))
                exp.append((n, w(((v & (M - 1)) & ~mask) | ((new << p) & mask))))
        lines += ["FINI(0)", "%%"]
        src = os.path.join(d, "c%d.imp" % c0)
        open(src, "w").write("\n".join(lines) + "\n")
        exe = os.path.join(d, "c%d" % c0)
        r = subprocess.run([driver, "-o", exe, src], capture_output=True, text=True)
        if r.returncode != 0:
            print("chunk %d does not compile: %s" % (c0, (r.stdout + r.stderr)[-200:]))
            continue
        out = subprocess.run([exe], capture_output=True, text=True).stdout.split("\n")
        got = {}
        for ln in out:
            f = ln.split()
            if len(f) == 2:
                got[int(f[0])] = int(f[1])
        for n, ev in exp:
            if got.get(n) != ev:
                kind, v, s, p = chunk[n]
                bad.setdefault((kind, s, p), []).append((v, ev, got.get(n)))
    for (kind, s, p), items in sorted(bad.items()):
        v, ev, gv = items[0]
        print("%s <%d,%d>: wrong for %d of the values, e.g. value %d expected %d got %s" % ("read " if kind == "r" else "store", s, p, len(items), v, ev, gv))
    print("%d cases, %d (kind, size, position) wrong" % (len(cases), len(bad)))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
