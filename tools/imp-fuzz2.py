#!/usr/bin/env python3
"""imp-fuzz2.py SEED COUNT DIR: random IMP programs with subroutines, arrays, loops and
conditionals (no recursion, bounded loops, indices masked into range, small values), for
comparing the original compiler with imp72stage0.  See imp-fuzz.py for the expressions."""
import os, random, sys

REL = ["=", "NE", "<", ">", "LE", "GE"]


class G:
    def __init__(self, r):
        self.r = r
        self.subrs = []          # (name, nparams)
        self.nvar = 0            # loop variables are global in IMP: each loop gets its own

    def atom(self, vs):
        r = self.r
        k = r.random()
        if k < 0.5 and vs:
            return r.choice(vs)
        if k < 0.65:
            return "V[%s AND 15]" % (r.choice(vs) if vs else str(r.randint(0, 15)))
        return str(r.randint(0, 30))

    def expr(self, vs, d):
        r = self.r
        if d == 0 or r.random() < 0.2:
            return self.atom(vs)
        a, b = self.expr(vs, d - 1), self.expr(vs, d - 1)
        k = r.random()
        if k < 0.22:
            return "(%s+%s)" % (a, b)
        if k < 0.38:
            return "(%s-%s)" % (a, b)
        if k < 0.48:
            return "((%s AND 15)*(%s AND 15))" % (a, b)
        if k < 0.55:
            return "(%s AND %s)" % (a, b)
        if k < 0.60:
            return "(%s OR %s)" % (a, b)
        if k < 0.65:
            return "(%s XOR %s)" % (a, b)
        if k < 0.75:
            return "(%s %s %s)" % (a, r.choice(REL), b)
        if k < 0.85 and self.subrs:
            n, np_ = r.choice(self.subrs)
            args = ",".join(self.expr(vs, 1) for _ in range(np_))
            return "%s(%s)" % (n, args)
        return "(%s %s %s=>%s ELSE %s)" % (a, r.choice(REL), b, self.expr(vs, d - 1), self.expr(vs, d - 1))

    def stmt(self, vs, loopvars, d):
        r = self.r
        k = r.random()
        t = r.choice([v for v in vs if v[0] not in 'IW'])   # never assign to a loop variable
        if k < 0.35:
            return "%s_((%s) AND 4095)" % (t, self.expr(vs, 2))
        if k < 0.50:
            return "V[%s AND 15]_((%s) AND 4095)" % (self.expr(vs, 1), self.expr(vs, 2))
        if k < 0.65 and d > 0:
            self.nvar += 1
            lv = "I%d" % self.nvar
            body = self.stmt(vs + [lv], loopvars + [lv], d - 1)
            return "(%s) FOR %s TO %d" % (body, lv, r.randint(1, 6))
        if k < 0.80 and d > 0:
            return "%s %s %s=>(%s; %s)" % (self.expr(vs, 1), r.choice(REL), self.expr(vs, 1),
                                           self.stmt(vs, loopvars, d - 1), self.stmt(vs, loopvars, d - 1))
        if k < 0.90 and d > 0:
            self.nvar += 1
            c = "W%d" % self.nvar
            return "(%s_0; WHILE %s<%d DO (%s; %s_%s+1))" % (c, c, r.randint(1, 5),
                                                             self.stmt(vs, loopvars + [c], d - 1), c, c)
        return "%s_((%s+%s) AND 4095)" % (t, t, self.expr(vs, 1))

    def subr(self, i):
        r = self.r
        np_ = r.randint(1, 3)
        ps = ["P%d" % j for j in range(np_)]
        locs = ["L%d_%d" % (i, j) for j in range(2)]
        vs = ps + [l.replace("_", "") for l in locs]
        lv = [l.replace("_", "") for l in locs]
        body = ["%s_((%s) AND 4095)" % (l, self.expr(ps, 1)) for l in lv]
        for _ in range(r.randint(1, 3)):
            body.append(self.stmt(vs, [], 2))
        body.append("((%s) AND 4095)" % self.expr(vs, 2))
        name = "S%d" % i
        text = "SUBR %s(%s) IS (%s)" % (name, ",".join(ps), "; ".join(body))
        self.subrs.append((name, np_))
        return text

    def program(self):
        r = self.r
        vars_ = ["A", "B", "C", "D"]
        out = ["V IS 16 LONG;"]
        for v in vars_:
            out.append("%s_%d;" % (v, r.randint(0, 20)))
        out.append("(V[I]_I*%d+%d) FOR I TO 15;" % (r.randint(1, 5), r.randint(0, 9)))
        subs = []
        # the subroutines are defined after the main statements, but may only call earlier ones
        for i in range(4):
            subs.append(self.subr(i))
        main = []
        for _ in range(14):
            main.append(self.stmt(vars_, [], 2) + ";")
            n, np_ = r.choice(self.subrs)
            main.append("PRINT IGR 10,%s(%s),/;" % (n, ",".join(self.expr(vars_, 1) for _ in range(np_))))
        main.append("PRINT IGR 8,A,IGR 8,B,IGR 8,C,IGR 8,D,/;")
        main.append("(PRINT IGR 6,V[I]) FOR I FROM 0 TO 15;" if False else "(PRINT IGR 6,V[I]) FOR I TO 15;")
        main.append("PRINT /;")
        main.append("FINI(0);")
        return "\n".join(out + main + [s + ";" for s in subs[:-1]] + [subs[-1]] + ["%%"]) + "\n"


if __name__ == "__main__":
    seed, count, d = int(sys.argv[1]), int(sys.argv[2]), sys.argv[3]
    os.makedirs(d, exist_ok=True)
    for i in range(count):
        open(os.path.join(d, "gz%02d.imp" % i), "w").write(G(random.Random(seed * 1000 + i)).program())
