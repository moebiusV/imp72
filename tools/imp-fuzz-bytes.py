#!/usr/bin/env python3
"""imp-fuzz-bytes.py SEED COUNT DIR: write COUNT programs of byte pointers and block moves to DIR/pN.imp, each with the output it must print in DIR/pN.exp.

As tools/imp-fuzz64.py, the expected output is worked out here from the rules of the word of 64 bits, not by a compiler.  The rules (lib/imp72rt.imp, docs/internals/codegen-and-translation.md):

    X_BYTEP P[k]<S,Q>   a pointer to the field of S bits with its low bit at bit Q of the word P[k].  <X> reads the field, <X>_E writes the low S bits of E and leaves the others,
                        <+X> steps the pointer first and then reads (<+X>_E steps and then writes): the next lower field of the same size, or, when there is no room (Q-S < 0), the
                        top field of the next word (the field at bit 64-S, the address + 8).
    X_BYTEP P[k]<7,36>  the one pointer to characters, as the tape has it: a byte address, that of the word P[k] less 1 (the pointer is before the first character: <+X> is
                        the first).  <X> is the byte there, <+X> steps one byte and reads, writes keep the low 8 bits of E.  A byte of a word is where the machine puts it
                        (the byte order of the host: the generator runs on the machine that runs the programs).
    MOVE P[a] THROUGH N TO Q[b]   N+1 words from P[a] up, to Q[b] up, one word at a time in ascending order as BLT does, so that with overlap the words copied are the words
                        already copied (the PDP-10's way, not memmove's).

The pointers stay inside the array P (32 words); an operation that would leave it is not written.
"""
import os
import random
import sys

M = 1 << 64
NW = 32


def w(x):
    x &= M - 1
    return x - M if x >> 63 else x


def u(x):
    return x & (M - 1)


EDGES = [0, 1, 2, 3, 127, 128, 255, 256, 65535, 65536, (1 << 31) - 1, 1 << 31, (1 << 32) - 1, 1 << 32,
         (1 << 36) - 1, 1 << 36, (1 << 53) + 1, (1 << 62) + 5, (1 << 63) - 1]


def value(r):
    v = r.choice(EDGES) if r.random() < 0.7 else r.randint(0, 1 << r.randint(1, 63))
    return -v if r.random() < 0.4 else v


def lit(v):
    return str(v) if v >= 0 else "(0-%d)" % (-v)


SIZES = [1, 2, 3, 4, 6, 8, 9, 12, 16, 18, 24, 31, 32, 36, 48, 63]
BIG = sys.byteorder == "big"


class Mem:
    def __init__(self, words):
        self.p = [u(x) for x in words]

    def byte(self, a):
        sh = 8 * (7 - a % 8) if BIG else 8 * (a % 8)
        return (self.p[a // 8] >> sh) & 255

    def setbyte(self, a, v):
        sh = 8 * (7 - a % 8) if BIG else 8 * (a % 8)
        i = a // 8
        self.p[i] = (self.p[i] & ~(255 << sh)) | ((v & 255) << sh)


class Ptr:
    """a pointer: a character pointer (byte address a) or a field (word, size, position)"""

    def __init__(self, char, a=0, wd=0, sz=0, pos=0):
        self.char, self.a, self.wd, self.sz, self.pos = char, a, wd, sz, pos

    def step(self):
        if self.char:
            return Ptr(True, a=self.a + 1)
        np_ = self.pos - self.sz
        if np_ < 0:
            return Ptr(False, wd=self.wd + 1, sz=self.sz, pos=64 - self.sz)
        return Ptr(False, wd=self.wd, sz=self.sz, pos=np_)

    def inside(self):
        return 0 <= self.a < 8 * NW if self.char else 0 <= self.wd < NW

    def word(self):
        return self.a // 8 if self.char else self.wd

    def load(self, mem):
        if self.char:
            return mem.byte(self.a)
        return (mem.p[self.wd] >> self.pos) & ((1 << self.sz) - 1)

    def store(self, mem, v):
        if self.char:
            mem.setbyte(self.a, v)
            return
        mask = ((1 << self.sz) - 1) << self.pos
        mem.p[self.wd] = (mem.p[self.wd] & ~mask) | ((u(v) << self.pos) & mask)


def program(r, n):
    init_p = [value(r) for _ in range(NW)]
    init_q = [value(r) for _ in range(NW)]
    mem = Mem(init_p)
    q = [u(x) for x in init_q]
    var = {v: value(r) for v in "ABCDE"}
    lines = ["A,B,C,D,E,X ARE COMMON;", "P IS %d LONG;" % NW, "Q IS %d LONG;" % NW]
    for v in "ABCDE":
        lines.append("%s_%s;" % (v, lit(var[v])))
    for i in range(NW):
        lines.append("P[%d]_%s;" % (i, lit(w(mem.p[i]))))
        lines.append("Q[%d]_%s;" % (i, lit(w(q[i]))))
    exp = []
    ptr = None

    def newptr():
        k = r.randint(1, NW - 4)
        if r.random() < 0.25:
            return "X_BYTEP P[%d]<7,36>;" % k, Ptr(True, a=8 * k - 1)
        sz = r.choice(SIZES)
        pos = r.randint(0, 64 - sz)
        if (sz, pos) == (7, 36):
            pos = 0
        return "X_BYTEP P[%d]<%d,%d>;" % (k, sz, pos), Ptr(False, wd=k, sz=sz, pos=pos)

    for i in range(n):
        if ptr is None or r.random() < 0.12:
            text, ptr = newptr()
            lines.append(text)
            continue
        k = r.random()
        if k < 0.22:                               # read
            vn = r.choice("ABCDE")
            var[vn] = ptr.load(mem)
            lines.append("%s_<X>;" % vn)
            lines.append("PRINT STG 0,'%d ',IGR 0,%s,/;" % (i, vn))
            exp.append("%d %d" % (i, var[vn]))
        elif k < 0.50:                             # step and read
            nxt = ptr.step()
            if not nxt.inside():
                continue
            ptr = nxt
            vn = r.choice("ABCDE")
            var[vn] = ptr.load(mem)
            lines.append("%s_<+X>;" % vn)
            lines.append("PRINT STG 0,'%d ',IGR 0,%s,/;" % (i, vn))
            exp.append("%d %d" % (i, var[vn]))
        elif k < 0.70:                             # write, then the word
            vn = r.choice("ABCDE")
            ptr.store(mem, var[vn])
            lines.append("<X>_%s;" % vn)
            lines.append("PRINT STG 0,'%d ',IGR 0,P[%d],/;" % (i, ptr.word()))
            exp.append("%d %d" % (i, w(mem.p[ptr.word()])))
        elif k < 0.85:                             # step and write, then the word
            nxt = ptr.step()
            if not nxt.inside():
                continue
            ptr = nxt
            vn = r.choice("ABCDE")
            ptr.store(mem, var[vn])
            lines.append("<+X>_%s;" % vn)
            lines.append("PRINT STG 0,'%d ',IGR 0,P[%d],/;" % (i, ptr.word()))
            exp.append("%d %d" % (i, w(mem.p[ptr.word()])))
        else:                                      # a block move, then the words it wrote
            cnt = r.randint(0, 6)
            src = r.choice("PQ")
            dst = r.choice("PQ")
            sa = r.randint(0, NW - 1 - cnt)
            da = r.randint(0, NW - 1 - cnt) if r.random() < 0.6 or src != dst else max(0, min(NW - 1 - cnt, sa + r.randint(-3, 3)))
            sarr = mem.p if src == "P" else q
            darr = mem.p if dst == "P" else q
            for j in range(cnt + 1):               # ascending, a word at a time
                darr[da + j] = sarr[sa + j]
            lines.append("MOVE %s[%d] THROUGH %d TO %s[%d];" % (src, sa, cnt, dst, da))
            for j in range(cnt + 1):
                lines.append("PRINT STG 0,'%d.%d ',IGR 0,%s[%d],/;" % (i, j, dst, da + j))
                exp.append("%d.%d %d" % (i, j, w(darr[da + j])))
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
