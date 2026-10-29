#!/usr/bin/env python3
"""ir2ll-fuzz.py SEED COUNT DIR: write COUNT files DIR/zN.ir of random but valid pcc IR text for the LLVM IR writer (tools/ir2ll.c, src/ir2ll.imp): every operator and type of the
trees (the signed and unsigned compares, the divisions, the conversions between the integers and the real, the shifts), calls of every shape (no arguments, the registers, the stack,
the C library's variadic functions, _setjmp, the functions marked cfunc), temps, labels, GO TOs, and the assembler lines that mean something here (.local, .comm, .quad, .string with
its escapes, # varrec, # cfunc, labels).  tests/direct/ir2ll.sh gives the files to both writers and compares what they write."""
import random
import sys

TYPES = [3, 6, 8, 9, 13, 40, 296]
NAMES = ['gv', 'gw', 'tab', 'imp_x', '.L7', 'strlab', 'a.b']
FUNCS = ['foo', 'bar', 'printf', 'snprintf', 'fprintf', 'sscanf', '_setjmp', 'setjmp', 'longjmp', 'memcpy', 'impfoo', 'open', 'execlp', 'f1', 'f2']
BIN = [10, 11, 12, 13, 14, 15, 16, 17, 18, 19]
CMP = [25, 26, 27, 28, 29, 30, 31, 32, 33, 34]


class G:
    def __init__(self, rnd):
        self.r = rnd
        self.out = []
        self.lab = 100
        self.ntemp = 0

    def e(self, s):
        self.out.append(s)

    def num(self):
        r = self.r
        k = r.random()
        if k < 0.5:
            return r.randint(-5, 300)
        if k < 0.7:
            return r.randint(-2**31, 2**31)
        if k < 0.85:
            return r.randint(-2**63, 2**63 - 1)
        return r.choice([0, 1, -1, 255, 256, 2**63 - 1, -2**63, 2**31, -2**31, 2**32])

    def leaf(self, t):
        r = self.r
        k = r.random()
        if k < 0.25:
            self.e('" 4 %d 0 %d ' % (t, self.num()))
        elif k < 0.35:
            self.e('" 4 %d 0 %d %s' % (t, r.randint(-16, 64), r.choice(NAMES + FUNCS)))
        elif k < 0.55:
            self.e('" 2 %d 0 %d %s' % (t, r.choice([0, 0, 8, 16, -8]), r.choice(NAMES)))
        elif k < 0.75:
            self.e('" 8 %d 0 0 %d' % (t, 100 + r.randint(0, 7)))
        elif k < 0.9:
            self.e('" 6 %d 0 0 %d' % (t, r.choice([5, 4, 1, 2, 8, 9])))
        else:
            # a stack parameter: *(rbp + 16 + 8k)
            self.e('" 23 %d 0 0' % t)
            self.e('" 10 40 0')
            self.e('" 6 40 0 0 6')
            self.e('" 4 40 0 %d ' % (16 + 8 * r.randint(0, 5)))

    def call(self, t, stmt=False):
        r = self.r
        name = r.choice(FUNCS)
        n = r.choice([0, 0, 1, 2, 3, 4, 6, 7, 9])
        if n == 0:
            self.e('" 42 %d 0 0' % t)
            self.e('" 4 296 0 0 %s' % name)
            return
        self.e('" 41 %d 0' % t)
        self.e('" 4 296 0 0 %s' % name)
        regs = [5, 4, 1, 2, 8, 9]
        # CM nesting to the left: CM(CM(a1, a2), a3)
        for _ in range(n - 1):
            self.e('" 48 6 0')
        for k in range(n):
            self.e('" 49 %d 0' % 8)
            if k < 6:
                self.e('" 6 40 0 0 %d' % regs[k])
            else:
                self.e('" 23 40 0 0')
                self.e('" 10 40 0')
                self.e('" 6 40 0 0 7')
                self.e('" 4 40 0 %d ' % (8 * (k - 6)))
            self.expr(r.choice([8, 8, 6, 9, 296, 13, 3]), 1)
        if n and r.random() < 0.3:
            pass

    def expr(self, t, d):
        r = self.r
        if d <= 0 or r.random() < 0.25:
            self.leaf(t)
            return
        k = r.random()
        if k < 0.45:
            op = r.choice(BIN)
            if t == 13 and op in (13, 15, 16, 17, 18, 19):
                op = r.choice([10, 11, 12, 14])
            self.e('" %d %d 0' % (op, t))
            self.expr(r.choice(TYPES), d - 1)
            self.expr(r.choice(TYPES), d - 1)
        elif k < 0.6:
            op = r.choice(CMP)
            self.e('" %d %d 0' % (op, t))
            lt = r.choice(TYPES)
            self.expr(lt, d - 1)
            self.expr(r.choice(TYPES), d - 1)
        elif k < 0.7:
            self.e('" %d %d 0 0' % (r.choice([37, 38]), t))
            self.expr(r.choice(TYPES), d - 1)
        elif k < 0.78:
            self.e('" 24 %d 0 0' % t)
            self.expr(r.choice(TYPES), d - 1)
        elif k < 0.82 and t != 13:
            self.e('" 20 %d 0 0' % t)
            self.expr(r.choice(TYPES), d - 1)
        elif k < 0.92:
            self.e('" 23 %d 0 0' % t)
            self.expr(r.choice([8, 296, 40]), d - 1)
        else:
            self.call(t)

    def stmt(self, d):
        r = self.r
        k = r.random()
        if k < 0.4:
            t = r.choice(TYPES)
            self.e('" 49 %d 0' % t)
            kk = r.random()
            if kk < 0.4:
                self.e('" 2 %d 0 %d %s' % (t, r.choice([0, 8]), r.choice(NAMES)))
            elif kk < 0.8:
                self.e('" 8 %d 0 0 %d' % (t, 100 + r.randint(0, 7)))
            else:
                self.e('" 23 %d 0 0' % t)
                self.expr(8, 1)
            self.expr(r.choice(TYPES), d)
        elif k < 0.5:
            self.e('" 49 8 0')
            self.e('" 6 8 0 0 0')
            self.expr(r.choice(TYPES), d)
        elif k < 0.7:
            self.e('" 35 6 0')
            if r.random() < 0.7:
                op = r.choice(CMP)
                self.e('" %d 6 0' % op)
                self.expr(r.choice(TYPES), d - 1)
                self.expr(r.choice(TYPES), d - 1)
            else:
                self.expr(r.choice(TYPES), d)
            self.e('" 4 6 0 %d ' % r.randint(100, 120))
        elif k < 0.78:
            self.e('" 54 6 0 0')
            self.e('" 4 6 0 %d ' % r.randint(100, 120))
        elif k < 0.9:
            self.call(8, True)
        elif k < 0.95:
            self.e('^ %d' % r.randint(100, 120))
        else:
            self.e('* ' + r.choice(['\t.text', '\t.align 4', '  .local lcl%d' % r.randint(0, 3), '.comm cm%d,%d,8' % (r.randint(0, 3), r.randint(1, 100)),
                                   '# cfunc foo', '# cfunc f1', '# varrec vr%d %d mod:vr' % (r.randint(0, 3), r.randint(8, 64))]))

    def data(self):
        r = self.r
        for _ in range(r.randint(0, 5)):
            k = r.random()
            if k < 0.3:
                self.e('* .local lcl%d' % r.randint(0, 3))
            if k < 0.6:
                self.e('* %s%d:' % (r.choice(['dat', '.Ld', '.Lx']), r.randint(0, 5)))
                if r.random() < 0.5:
                    self.e('*  .data')
                    for _ in range(r.randint(0, 5)):
                        self.e('*  .quad %d' % self.num())
                else:
                    s = ''.join(r.choice(['a', 'Z', '\\n', '\\t', '\\\\', '\\"', '\\x41', '\\x4g', '\\101', '\\7', '\\1234', '\\q', ' ', '%', '"x']) for _ in range(r.randint(0, 10)))
                    self.e('*  .string "%s"' % s)
            elif k < 0.7:
                self.e('* .comm cm%d,%d,8' % (r.randint(0, 3), r.randint(1, 100)))
            elif k < 0.75:
                self.e('*  .comm  sp%d , 24' % r.randint(0, 3))
            elif k < 0.85:
                self.e('* # varrec vr%d %d mod:vr%d' % (r.randint(0, 3), r.randint(8, 64), r.randint(0, 3)))
            elif k < 0.9:
                self.e('* # cfunc %s' % r.choice(FUNCS))
            else:
                self.e('* \t.text')

    def func(self, i):
        r = self.r
        self.data()
        name = r.choice(['fn%d' % i, '.Lfn%d' % i, 'imp_main'])
        self.e('* %s:' % name)
        self.e('! 8 1 %d 100 %d %s' % (65536 * (i + 1) + 100, 65536 * (i + 1) + 100, name))
        self.e('^ %d' % (65536 * (i + 1) + 100))
        # temps: use a few
        for _ in range(r.randint(1, 10)):
            self.stmt(r.randint(1, 3))
        if r.random() < 0.5:
            self.e('%% 0 %d 100 100 %s' % (65536 * (i + 1) + 101, name))
        else:
            self.e('%% 0 %d 100 100 %s' % (65536 * (i + 1) + 101, name))


def main():
    seed, count, d = int(sys.argv[1]), int(sys.argv[2]), sys.argv[3]
    for n in range(count):
        g = G(random.Random(seed * 100003 + n))
        for i in range(g.r.randint(1, 4)):
            g.func(i)
        g.data()
        open('%s/z%d.ir' % (d, n), 'w').write('\n'.join(g.out) + '\n')


main()
