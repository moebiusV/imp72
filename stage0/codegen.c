/*
 * codegen.c -- the code generating semantic routines.  Ports decus/code.imc,
 * code2.imc and code3.imc: ADDCODE, UNPCK, STORE, FETCH, REGOF, ADDOP, DEWOP,
 * TAG, COPYCO, MCODE, SUBARG, HOOK, CONOP, FREEZE, SUBRCALL, RETURN, ONEWORD,
 * COPPY, DEWFUN, FLOAT, FIX, DATAST, SUBRPAR, OJUMPOP, ADDR, SUBCALL,
 * SUBSCRIPT.  (PRINT statements, PRINCAL/PRINPAR, and BYTEP are not ported yet.)
 *
 * These generate the PDP-10 code of the original compiler, as a chain of
 * instruction words in free array CO: the code of a stack entry is the chain
 * between CB and CE in its third word.  Their meaning is the PDP-10's;
 * trans.c turns the chains into IMP-IR, from which the pcc IR is made, in
 * place of the assembler (amod), register allocator (rmod) and output module
 * (omod) of the original, which are not ported.
 *
 * The words of CO (36 bit; TYPE = bits 35-34):
 *
 *   2  LINK: bits 17-0 the index of the next word (0 ends the chain)
 *   1  SPECIAL FUNCTION: bits 33-27 the function, bits 17-0 its argument
 *        0  define a tag (V = directory index of its name)
 *        1  save the registers in use in the range lo-hi (V = lo<<6 | hi)
 *        2  restore saved registers, and make a dummy reference to register V
 *        3  enter "delete no instructions" mode      4  leave it
 *        5  a reference to register V to set it aside for the user
 *        6  reserve register V until the end of the program
 *        7  the following instruction is a move from a user register to a
 *           temporary, optimisable out ...
 *        8  make register V available   9  declare it scratch  10  protected
 *        11 suppress the reference count of the register in the next
 *           instruction word
 *   3  CONSTANT: the next word is its value; bits 17-0 a directory index
 *   0  INSTRUCTION, two words:
 *        word 1  bit 33 indirect, bits 32-24 opcode, 22-12 AC (a register
 *                index), bit 11 address field is a register index, 10-0
 *                index register
 *        word 2  bits 35-18 constant added to the address, 17-0 the
 *                directory index of the variable
 *
 * A register index is not a PDP-10 register: it names an entry of the
 * register array RG (impsem.c), with limits; allocation is left to pcc.
 */
#include <stdlib.h>
#include <string.h>
#include "imp.h"

word co, nco;
word anco;                              /* ANCO: has real code been generated? */
static word copys, ncop;
static word remo;
static int cinit;

/* the unpacked stack entry (UNPCK): shared as in the original */
static word se, tse, dse, se1, rse, cse;

#define IMM_(j) (immv[j])
static const word immv[2]  = { 043600000L, 0177777L };
static const word selfv[2] = { 043770000L, 0177777L };
static const word bothv[2] = { 043170000L, 0000377L };

static void
unpck(word s)
{
    se = FREE(s);
    tse = se & 077;
    dse = L18(se);
    se1 = FREE(s + 1);
    rse = 03777 & (u36(se1) >> 18);
    cse = R18(se1);
}

word
regof(word s)
{
    return 03777 & (u36(FREE(s + 1)) >> 18);
}

/*
 * ADDCODE(S,W1,W2): add the word w1, and w2 if it is an instruction or a
 * constant, to the code of stack entry s.
 */
word
addcode(word s, word w1, word w2)
{
    word aty, se2, z, ad;

    anco = anco == -1 ? 0 : 1;
    aty = u36(w1) >> 34;
    if (aty == 3)
        aty = 0;
    se2 = FREE(s + 2);
    if (se2 == 0)
        z = nco << 18;
    else {
        z = se2 & ~MASK18;
        ad = se2 & MASK18;
        if (ad == nco - 1)
            nco--;
        else
            FREES(co + ad, sx36(0400000000000L + nco));
    }
    if (flen(co) <= nco + 3)
        faddex(&co, nco + 3);
    FREES(co + nco, sx36(w1));
    nco++;
    if (aty == 0) {
        FREES(co + nco, sx36(w2));
        nco++;
    }
    z |= nco;
    FREES(co + nco, sx36(0400000000000L));
    nco++;
    FREES(s + 2, sx36(z));
    return s;
}

/* HOOK(D,A,B): hook the code of a to that of b, put it in d; the value of d
 * is set to the value of b. */
word
hook(word d, word a, word b)
{
    word j, k;

    FREES(d, FREE(b));
    FREES(d + 1, FREE(b + 1));
    if ((j = FREE(b + 2)) == 0) {
        FREES(d + 2, FREE(a + 2));
        return d;
    }
    if ((k = FREE(a + 2)) == 0) {
        FREES(d + 2, j);
        return d;
    }
    FREES(co + (k & MASK18), sx36(0400000000000L | (u36(j) >> 18)));
    FREES(d + 2, sx36((k & ~MASK18 & MASK36) | (j & MASK18)));
    return d;
}

/* OCHK: what an opcode alters: bit 0 the accumulator, 1 a memory location,
 * 2 after the operation [AC] = [memory].  (rmod.imc) */
static word
ochk(word opc)
{
    static const word op1v[2] = { 002132130L, 07211L };
    static const word op2v[3] = { 0111301110111L, 0131213110000L, 05615L };
    static const word op3v = 071715000L;
    static const word op4v[2] = { 05015L, 06600L };
    static const word op5v = 03211L;
    word rtv = 0, s = 0, i = 0;

    switch (opc >> 6) {
    case 0:
    case 7:
        return 1;
    case 1:
        if (opc < 0140) {
            s = op1v[0];
            i = opc & 7;
            goto testo;
        }
    op01:
        s = op1v[1];
    op02:
        i = opc & 3;
    testo:
        return rtv | (7 & (u36(s) >> (3 * i)));
    case 2:
        if (opc < 0204) {
            s = op2v[2];
            goto op02;
        }
        if (opc < 0240)
            goto op01;
        if (opc >= 0270)
            goto op01;
        if (opc < 0254) {
            s = op2v[0];
            i = opc - 0240;
        } else {
            s = op2v[1];
            i = opc - 0254;
        }
        goto testo;
    case 3:
        s = op3v;
        i = 7 & (opc >> 3);
        if ((opc & 0770) == 0330)
            rtv = 010;
        goto testo;
    case 4:
        if (opc & 4) {
            if ((opc & 070) == 010) {
                s = op4v[0];
                goto op02;
            }
            if ((opc & 070) == 020) {
                s = op4v[1];
                goto op02;
            }
        }
        goto op01;
    case 5:
        if (opc < 0510) {
            s = op5v;
            goto op02;
        }
        if (opc < 0540)
            goto op01;
        if (opc < 0550) {
            s = op5v;
            goto op02;
        }
        goto op01;
    default:                            /* 6 */
        return opc < 0620 ? 0 : 1;
    }
}

word ochk_(word opc) { return ochk(opc); }

/*
 * DEWOP(OP,R,S): add to the code of s an instruction doing opcode op with
 * accumulator r (0: none) and the operand s.  S becomes type 2 (register r)
 * unless r = 0.  A negative r means: do not protect a user register.
 */
word
dewop(word opp, word rr, word s)
{
    word op = opp, r = rr, j, k, ise, ic, isreal;

    if (r <= 0) {
        r = -rr;
        goto l72;
    }
    if (op & 01000) {                           /* hit a user register on purpose */
        op &= ~01000L;
        goto l72;
    }
    /* does op clobber a programmer-specified register? */
    if ((op & 0740) != 0340 && (ochk(op) & 1) && hisreg(r)) {
        j = areg1(1, 015);
        addcode(s, 0207000000000L, 0);
        addcode(s, sx36(020000004000L | (j << 12)), r);
        r = j;
    }
l72:
    unpck(s);
    isreal = (se & 0300) == 0200;
    ise = 0100000000000L & (u36(se1) >> 2);
    if (tse == 4) {                             /* a constant: immediate? */
        rse = ise = cse = 0;
        if (se & 0200)                          /* a real: all 64 bits, pooled */
            goto pool;
        if ((op & 0770) == 0310 && ((uword)se1 >> 18) == 0) {
            op = 0300 | (op & 7);               /* CAMX to CAIX */
            cse = se1;
            dse = 0;
            goto l6;
        }
        if ((op & 01003) == 0) {
            if ((j = (word)((uword)se1 >> 18)) == 0) {
                ic = op >> 8;
                k = 037 & (op >> 3);
                if (1 & (u36(immv[ic]) >> k)) {
                    op |= 1;
                    cse = se1;
                    dse = 0;
                    goto l6;
                }
            }
            if (op == 0200 && se1 == -1) {
                op = 0474;
                cse = 0;
                dse = 0;
                goto l6;
            }
            /* The original also uses HRROI, HRLZI and HRLOI here for
             * constants whose halves are all ones or zero, and immediate
             * floating point operands: all 18 bit halfword tricks.  Halves
             * are 32 bits here, so those constants go through the pool. */
        }
pool:
        if (dse == 0)
            dse = stcon(se1);
    }
    if (tse == 2) {
        ise |= 04000;
        dse = rse;
        rse = 0;
    }
l6:
    if (isreal && ((opp & 0700) == 0300 || (opp >= 0210 && opp <= 0217)))     /* compare, skip, jump, negate, absolute value of a real */
        ise |= REALOP;
    addcode(s, sx36(ise | rse | (r << 12) | (op << 24)), dse | (cse << 18));
    if (r && subreg(r) == 0) {
        FREES(s, sx36(2 | (se & 0777700L)));
        FREES(s + 1, sx36(r << 18));
    }
    return s;
}

word
fetch(word s)
{
    if ((FREE(s) & 077) != 2)
        dewop(0200, areg1(1, 015), s);
    return s;
}

word
addop(word op, word s, word t)
{
    return hook(s, t, dewop(op, regof(fetch(t)), s));
}

word
tag(word s)
{
    word qq = FREE(s);

    addcode(s, sx36(0200000000000L | (u36(qq) >> 18)), 0);
    return s;
}

/* RCOCHK(N,C,R,R1): look in the code from c for references to register r
 * (n = 1: any, 2: not as an accumulator, 3: change r to r1 as accumulator). */
static word
rcochk(word n, word c, word r, word r1)
{
    word cj = c, v = 0, cc, ra, ri, rr;

    while (cj) {
        cc = FREE(co + cj);
        switch (u36(cc) >> 34) {
        case 1:
            cj++;
            break;
        case 2:
            cj = cc & MASK18;
            break;
        case 3:
            cj += 2;
            break;
        default:
            ra = 03777 & (u36(cc) >> 12);
            ri = 03777 & u36(cc);
            rr = 0;
            if (04000 & cc)
                rr = FREE(co + cj + 1) & MASK18;
            if (n == 1) {
                if (ra == r)
                    v++;
                if (ri == r)
                    v++;
                if (rr == r)
                    v++;
            } else if (n == 2) {
                if (ri == r)
                    v++;
                if (rr == r)
                    v++;
            } else if (n == 3 && ra == r) {
                cc &= ~037770000L;
                FREES(co + cj, sx36(cc | (r1 << 12)));
            }
            cj += 2;
            break;
        }
    }
    return v;
}

/*
 * STORE(S,T): get t into s (which is not type 4), hooking the code of t
 * before that of s.  An add, subtract, etc. to a register just stored is
 * turned into the instruction's "to memory" form where the machine has one.
 */
word
store(word s, word t)
{
    word j, ct, ti, c, t2, k, s1, tco, rt, op;

    unpck(s);
    if (tse & 030) {                            /* a possible self/both instruction */
        j = FREE(s + 2);
        if (j)
            goto l26;
        ct = FREE(t + 2);
        if (ct == 0)
            goto l26;
        j = FREE(t);
        if ((2 & j) == 0)
            goto l26;
        ti = FREE(co + (c = (MASK18 & ct)) - 2);
        if (u36(ti) >> 34)
            goto l26;
        t2 = FREE(co + c - 1);
        if (L18(t2) != cse)
            goto l26;
        if (R18(t2) != dse)
            goto l26;
        if ((((u36(se1) >> 35) + 1) & (u36(ti) >> 33)) == 1)
            goto l26;
        if (ti & 04000)
            goto l26;
        if (regeq(ti & 03777, rse) == 0)
            goto l26;
        j = 037 & (u36(ti) >> 27);
        k = 1 & (u36(ti) >> 32);
        if (3 & (u36(ti) >> 24))
            goto l26;
        if ((1 & (u36(selfv[k]) >> j)) == 0)
            goto l26;
        if (reg0(rse) && (1 & (u36(bothv[k]) >> j)) == 0)
            goto l26;
        FREES(co + c - 2, sx36(ti | 0300000000L));
        hook(s, t, s);
        return s;
    }
l26:
    if (tse == 2) {
        if ((FREE(t) & 077) != 2) {
l27:
            s1 = rse;
            hook(s, dewop(0200, -s1, t), s);
            return s;
        }
        /* register to register: try to leave out the destination */
        tco = u36(FREE(t + 2)) >> 18;
        rt = 03777 & (u36(FREE(t + 1)) >> 18);
        if (tmpreg(rt) == 0)
            goto l27;
        if (rcochk(1, tco, rse, 0))
            goto l27;
        if (rcochk(2, tco, rt, 0))
            goto l27;
        FREES(t + 1, sx36((rse << 18) | (FREE(t + 1) & 0774000777777L)));
        rcochk(3, tco, rt, rse);
        return t;
    }
    if ((FREE(t) & 077) == 4) {                 /* a 0 or -1 */
        s1 = FREE(t + 1);
        if (s1 == 0) {
            op = 0402;
l1:
            hook(s, t, dewop(op, 0, s));
            FREES(s, FREE(t));
            FREES(s + 1, FREE(t + 1));
            return s;
        }
        if (s1 == -1) {
            op = 0476;
            goto l1;
        }
    }
    fetch(t);
    hook(s, t, s);
    dewop(0202, regof(t), s);
    return s;
}

/* IMP72WIDE=1: the program is written for 64 bit words (the compiler's own sources), so its constants are folded in 64 bits; otherwise the folding wraps at the
 * 36 bits of the original, as the oracle does */
static int
imp_wide(void)
{
    static int w = -1;

    if (w < 0)
        w = getenv("IMP72WIDE") != NULL;
    return w;
}

/* ---- copying and comparing code ---------------------------------------- */

static word
copyc1(word j)
{
    word i, k;

    if (ncop) {
        for (i = ncop - 1; i >= 0; i--) {
            k = FREE(copys + i);
            if ((u36(k) >> 18) == j)
                return k & MASK18;
        }
    }
    i = samereg(j);
    fadd(&copys, &ncop, sx36(i | (j << 18)));
    return i;
}

/* COPYCO(S): a fresh copy (with new registers) of the code of stack entry s */
void
copyco(word s)
{
    word cj, cc, cd, j, ct;

    ncop = 0;
    if (copys == 0)
        copys = fallot_new("CPYCO", 20);
    cj = u36(FREE(s + 2)) >> 18;
    FREES(s + 2, 0);
    while (cj) {
        cc = FREE(co + cj);
        switch (u36(cc) >> 34) {
        case 3:
            addcode(s, cc, FREE(co + cj + 1));
            cj += 2;
            break;
        case 1:
            j = cc & MASK18;
            cd = 0177 & (u36(cc) >> 27);
            if (cd == 2 || (cd >= 5 && cd <= 6) || (cd >= 8 && cd <= 10))
                j = copyc1(j);
            cc = j | (cc & ~MASK18);
            addcode(s, cc, 0);
            cj++;
            break;
        case 2:
            cj = cc & MASK18;
            break;
        default:
            cd = FREE(co + cj + 1);
            if ((j = 03777 & (u36(cc) >> 12))) {
                j = copyc1(j);
                cc = (j << 12) | (cc & ~037770000L);
            }
            if (cc & 04000) {
                j = copyc1(cd & MASK18);
                cd = j | (cd & ~MASK18);
            }
            if ((j = 03777 & u36(cc))) {
                j = copyc1(j);
                cc = j | (cc & ~03777L);
            }
            addcode(s, cc, cd);
            cj += 2;
            break;
        }
    }
    cd = FREE(s + 1);
    ct = FREE(s) & 077;
    if (ct == 2 || ct == 020) {
        if ((j = 03777 & (u36(cd) >> 18)))
            cd = sx36((copyc1(j) << 18) | (cd & ~(03777L << 18)));
    }
    FREES(s + 1, cd);
}

static word
nexco(word *i)
{
    word a;

    for (;;) {
        if (*i == 0)
            return 0;
        a = FREE(co + *i);
        if ((u36(a) >> 34) == 2) {
            *i = a & MASK18;
            continue;
        }
        (*i)++;
        return a;
    }
}

static word
mcode1(word i, word j)
{
    word b, k, a, t;

    if (i == j)
        return 1;
    b = j | (i << 18);
    if (ncop) {
        for (k = ncop - 1; k >= 0; k--) {
            a = FREE(copys + k);
            if (a == b)
                return 1;
            t = u36(a) >> 18;
            if (t == i || t == j)
                return 0;
            t = a & MASK18;
            if (t == i || t == j)
                return 0;
        }
    }
    if (regsim(i, j) == 0)
        return 0;
    fadd(&copys, &ncop, sx36(b));
    return 1;
}

/* MCODE(S,T): 1 if the code of s and of t is the same, up to a
 * correspondence of registers. */
word
mcode(word s, word t)
{
    word js, jt, se_, te, sty, j;

    if (copys == 0)
        copys = fallot_new("CPYCO", 20);
    ncop = 0;
    js = FREE(s + 1);
    jt = FREE(t + 1);
    if (mcode1(03777 & (u36(js) >> 18), 03777 & (u36(jt) >> 18)) == 0)
        return 0;
    js = u36(FREE(s + 2)) >> 18;
    jt = u36(FREE(t + 2)) >> 18;
    for (;;) {
        se_ = nexco(&js);
        te = nexco(&jt);
        if ((js | jt) == 0)
            return 1;
        if (js == 0 || jt == 0)
            return 0;
        if ((sty = u36(se_) >> 34) != (u36(te) >> 34))
            return 0;
        if (sty) {
            if (se_ != te)
                return 0;
            if (sty == 1)
                continue;
            se_ = FREE(co + js);
            te = FREE(co + jt);
            if (se_ != te)
                return 0;
        } else {
            if ((u36(se_) & 0777700004000L) != (u36(te) & 0777700004000L))
                return 0;
            if (mcode1(se_ & 03777, te & 03777) == 0)
                return 0;
            if (mcode1(03777 & (u36(se_) >> 12), 03777 & (u36(te) >> 12)) == 0)
                return 0;
            j = se_ & 04000;
            se_ = FREE(co + js);
            te = FREE(co + jt);
            if (se_ != te) {
                if (j == 0)
                    return 0;
                if ((u36(se_) >> 18) != (u36(te) >> 18))
                    return 0;
                if (mcode1(03777 & se_, 03777 & te) == 0)
                    return 0;
            }
        }
        js++;
        jt++;
    }
}

/* ONEWORD(S): does stack entry s have exactly one word of code? */
word
oneword(word s)
{
    word val = 0, j = FREE(s + 2), t, ty;

    if (j == 0)
        return 0;
    j = u36(j) >> 18;
    while (j) {
        t = FREE(co + j);
        ty = u36(t) >> 34;
        if (ty == 2) {
            j = t & MASK18;
            continue;
        }
        if (ty == 1)
            return 0;                   /* can't trust special functions */
        j += 2;
        if (val == 0) {
            val = 1;
            continue;
        }
        return 0;
    }
    return val;
}

/* ---- subroutine calls ---------------------------------------------------- */

/*
 * SUBARG(S,P): force the argument p into an address for a subroutine call,
 * storing it if need be, perhaps adding a tag to s for storing an address
 * in the next word of code to be added.  Returns p (with the tag in its
 * left half when the code is reentrant).
 */
static word
subarg(word s, word pp)
{
    word p = pp, tm, s1, s2, j, qm, rntrnt;

    unpck(p);
    if (tse == 010)
        goto l14;
    if (tse == 2) {
        tm = subprm(0);
        s1 = FREE(tm);
        s2 = FREE(tm + 1);
        store(tm, p);
        FREES(tm, s1);
        FREES(tm + 1, s2);
        p = tm;
        goto l14;
    }
    if (tse == 4) {
        if ((u36(se) >> 18) == 0) {
            j = stcon(FREE(p + 1));
            FREES(p, sx36((se & MASK18) | (j << 18)));
        }
        FREES(p + 1, 0);
        goto l14;
    }
    /* type 20, a subscripted variable */
    if (dse == 0 && cse == 0 && (1 & (u36(se1) >> 35)) == 0) {
        FREES(p, sx36(2 | (FREE(p) & ~077L)));
        goto l15;
    }
    dewop(0201, areg1(1, 015), p);
l15:
    tm = newname("CAL");
    rntrnt = (finam5 & 0400000L) != 0;
    qm = name(enstack(tm));
    dewop(0542, regof(p), qm);
    hook(p, p, qm);
    if (rntrnt)
        p = sx36(p | (tm << 18));
    else
        addcode(s, sx36(0200000000000L | tm), 0);
l14:
    return p;
}

word
subrpar(word s, word pp)
{
    word p = subarg(s, pp), tm, ae;

    tm = u36(p) >> 18;
    p &= MASK18;
    if (tm)
        addcode(s, 0132000000000L, tm);
    else {
        tm = FREE(p);
        ae = FREE(p + 1);
        addcode(s, 032000000000L, sx36((ae << 18) | (u36(tm) >> 18)));
    }
    return hook(s, p, s);
}

word
dewfun(word s, word i, word j)
{
    if (anco == 0)
        anco = -1;
    return addcode(s, sx36(0200000000000L | j | (i << 27)), 0);
}

word
subrcall(word t)
{
    word tt, dse_, j;

    subpr0(0);
    tt = FREE(t);
    dse_ = L18(tt);
    if (dprop(DP_COM, dse_) != 2)
        dprops(DP_COM, dse_, 1);
    dewop(0266, areg(016), dewfun(t, 1, 015));
    tt = FREE(t);
    FREES(t, sx36(2 | (tt & ~077L)));
    j = areg1(0140, 0);
    FREES(t + 1, sx36(j << 18));
    return t;
}

word
return_(word s)
{
    word j, k;

    dewop(0200, -areg1(0100, 0), s);
    j = nsubp();
    k = areg(016);
    addcode(s, sx36(026700000000L | k | (k << 12)), sx36(j << 18));
    return s;
}

word
freeze(word s)
{
    word t = enstack(0);

    addcode(t, 0203000000000L, 0);
    addcode(s, 0204000000000L, 0);
    return hook(s, t, s);
}

word
subcall(word a, word b)
{
    a = subrcall(name(a));
    while (getlist(b))
        a = subrpar(a, b);
    dewfun(a, 2, regof(a));
    subpr0(1);
    return a;
}

word
coppy(word s)
{
    word w = enstack(0);

    FREES(w, FREE(s));
    FREES(w + 1, FREE(s + 1));
    FREES(w + 2, FREE(s + 2));
    if (FREE(w + 2))
        copyco(w);
    return w;
}

/* ADDR(S): make s type 20, so that what was the value winds up in the
 * address field when s is the operand of DEWOP. */
word
addr_(word s)
{
    word ae = FREE(s);

    if ((ae & 077) == 4) {
        FREES(s + 1, FREE(s + 1) & MASK18);
        ae &= MASK18;
    } else if ((ae & 077) != 2) {
        fetch(s);
        ae = FREE(s);
    }
    FREES(s, sx36(020 | (ae & ~077L)));
    return s;
}

/* OJUMPOP(I): the opcode for the complementary jump to opcode i */
word
ojumpop(word i)
{
    word val = 7 & (i + 4);

    if (i >= 8 && (1 & i))
        val = (2 & (i + 2)) | (5 & i);
    return val;
}

/* CONOP(S,T,I,V,W): an operation on one or two constants. */
word
conop(word s, word t, word i, word v, word w)
{
    word co1 = FREE(s + 1), co2 = 0, c, j;
    int nowrap = 0;

    if (t)
        co2 = FREE(t + 1);
    if ((FREE(s) & 0300) == 0200 || (t && (FREE(t) & 0300) == 0200)) {      /* a real constant: the words are the bits of doubles */
        double d1, d2, r = 0;
        int rel = 0;

        if ((FREE(s) & 0300) == 0200)
            memcpy(&d1, &co1, sizeof d1);
        else
            d1 = (double)co1;
        if (t && (FREE(t) & 0300) == 0200)
            memcpy(&d2, &co2, sizeof d2);
        else
            d2 = (double)co2;
        switch (i & MASK18) {
        case 0: r = -d1; break;
        case 1: r = d1 + d2; break;
        case 2: r = d1 - d2; break;
        case 3: r = d1 * d2; break;
        case 4: r = d2 != 0 ? d1 / d2 : 0; break;
        case 9: rel = 1; co1 = -(word)(d1 < d2); break;
        case 10: rel = 1; co1 = -(word)(d1 == d2); break;
        case 11: rel = 1; co1 = -(word)(d1 <= d2); break;
        case 13: rel = 1; co1 = -(word)(d1 >= d2); break;
        case 14: rel = 1; co1 = -(word)(d1 != d2); break;
        case 15: rel = 1; co1 = -(word)(d1 > d2); break;
        default:
            imp_error(1, "OPERATION ON A REAL CONSTANT. ");
            rel = 1;
            break;
        }
        if (!rel) {
            memcpy(&co1, &r, sizeof co1);
            FREES(s, sx36((FREE(s) & ~0377L) | 0204));                      /* the result is a real */
        } else
            FREES(s, sx36((FREE(s) & ~0377L) | 0104));                      /* a comparison is an integer */
        goto folded;
    }
    switch (i & MASK18) {
    case 0: co1 = -co1; break;
    case 1: co1 = co1 + co2; break;
    case 2: co1 = co1 - co2; break;
    case 3: co1 = co1 * co2; break;
    case 4: co1 = co2 ? co1 / co2 : 0; break;
    case 5:
        /* a left shift is folded as the program runs it, in 64 bits: the masks and tags that the
         * ported modules build (7 LS 48) do not fit the 36 bits of the original */
        if (co2 >= 0 && co2 < 64) {
            co1 = (word)((uword)co1 << co2);
            nowrap = 1;
        } else
            co1 = ls36(co1, co2);
        break;
    case 6: co1 = ~co1; break;
    case 7: co1 = co1 & co2; break;
    case 8: co1 = co1 | co2; break;
    case 9: co1 = -(word)(co1 < co2); break;       /* IMP's true is -1 */
    case 10: co1 = -(word)(co1 == co2); break;       /* IMP's true is -1 */
    case 11: co1 = -(word)(co1 <= co2); break;       /* IMP's true is -1 */
    case 13: co1 = -(word)(co1 >= co2); break;       /* IMP's true is -1 */
    case 14: co1 = -(word)(co1 != co2); break;       /* IMP's true is -1 */
    case 15: co1 = -(word)(co1 > co2); break;       /* IMP's true is -1 */
    case 16:
        if (co2 >= 0 && co2 < 64) {
            co1 = (word)((uword)co1 << co2);
            nowrap = 1;
        } else if (co2 < 0) {
            /* A ARS B is A ALS -B: an arithmetic right shift keeps the sign (ASH), unlike LS */
            word n = co2 < -63 ? 63 : -co2;

            co1 = co1 < 0 ? ~(~co1 >> n) : co1 >> n;
        } else
            co1 = ls36(co1, co2);
        break;
    case 17: co1 = (word)((u36(co1) << (co2 % 36) | u36(co1) >> (36 - co2 % 36))
        & MASK36); break;
    case 18: co1 = co1 ^ co2; break;
    case 19: co1 = ~(co1 ^ co2); break;
    default: break;
    }
    if (!imp_wide() && (!nowrap || co1 == sx36(co1)))
        co1 = sx36(co1);
folded:
    c = i >> 18;
    if (c) {                                    /* special for conditionals */
        /* the chosen arm replaces the whole entry: FREES(S+I,FREE(V+I)) FOR I FROM 2,
         * which counts 2, 1, 0 */
        if (c == 1) {
            if (co1 == 0)
                goto cxc;
            for (j = 2; j >= 0; j--)
                FREES(s + j, FREE(v + j));
            return s;
        }
        if (c == 2) {
            word arm = co1 == 0 ? w : v;

            for (j = 2; j >= 0; j--)
                FREES(s + j, FREE(arm + j));
            return s;
        }
    }
cxc:
    FREES(s + 1, co1);
    j = FREE(s);
    FREES(s, sx36(j & MASK18));
    if (t)
        hook(s, t, s);
    return s;
}

/* FLOAT(S) / FIX(S): the arithmetic type of s.  Constants are converted;
 * anything else needs the floating point instructions and the FIX routine
 * of the library, which are not ported. */
word
float_(word s)
{
    word sf = FREE(s);

    if ((sf & 0300) != 0200) {
        if ((sf & 077) == 4) {
            double x = (double)FREE(s + 1);
            word bits;

            memcpy(&bits, &x, sizeof bits);
            FREES(s + 1, bits);
            FREES(s, sx36(0204 | (sf & 0777000L)));
            return s;
        }
        /* FSC j,233: the integer to floating (trans.c knows the idiom) */
        {
            word j;

            fetch(s);
            j = regof(s);
            addcode(s, sx36(013200000000L | (j << 12)), 0233000000L);
            FREES(s, sx36(0202 | (sf & ~0377L)));
        }
    }
    return s;
}

word
fix_(word s)
{
    word sf = FREE(s);

    /* An entry of no arithmetic type (a subroutine argument) is taken to be
     * an integer; the original calls the library routine IFIX on it, which
     * is not ported (there is no floating point yet). */
    if ((sf & 0300) == 0) {
        FREES(s, sx36(sf | 0100));
        return s;
    }
    if ((sf & 0300) != 0100) {
        if ((sf & 077) == 4) {
            double x;
            word v = FREE(s + 1);

            memcpy(&x, &v, sizeof x);
            FREES(s + 1, (word)x);
            FREES(s, sx36(0104 | (sf & 0777000L)));
            return s;
        }
        /* the library routine IFIX takes the real and returns the integer */
        {
            word t = name(enstack(dir("IFIX"))), tf;

            subrcall(t);
            dewfun(subrpar(t, s), 2, regof(t));
            subpr0(1);
            tf = FREE(t);
            FREES(s, sx36(0100 | (tf & ~0300L)));
            FREES(s + 1, FREE(t + 1));
            FREES(s + 2, FREE(t + 2));
        }
    }
    return s;
}

/* DATAST(B): constants for a DATA statement. */
word
datast(word b)
{
    word a = enstack(dir("0")), i, j, k;

    FREES(a, 4 | (FREE(a) & ~077L));
    while (getlist(b)) {
        if (FREE(b + 2)) {
            imp_error(1, "NON-CONSTANT EXPRESSION IN DATA STATEMENT.");
            continue;
        }
        i = FREE(b);
        if ((i & 077) == 4 && L18(i) == 0) {
            addcode(a, 0600000000000L, FREE(b + 1));
            continue;
        }
        i = L18(i);
        if ((j = convc(i))) {
            if (j < 0) {
                addcode(a, 0600000000000L, convc_const);
                continue;
            }
            i = FREE(j);
            for (k = 1; k <= i; k++)
                addcode(a, 0600000000000L, FREE(j + k));
            continue;
        }
        hook(a, a, dewop(0, 0, b));
    }
    return a;
}


/*
 * PRINPAR(N,A,B,C,D): process a print list item.  N: 0 an expression, 1 OCT,
 * 2 IGR, 3 STG, 5 / (new line), 6 FILE (name, extension, project, programmer),
 * 7 IMAGE MODE, 8 DEVICE, 9 TAB, 10 FILL, 11 FLT m.n.  A is the item's operand
 * (B, C, D the rest of a FILE).  The result is a funny stack entry with the
 * code that computes the item in word 2 and, in word 1, the code that enters
 * it in the print list: constant words after the call of the print routine,
 *
 *     value = N << 24 | offset (| 1 << 23 if the address is the contents of
 *             the variable) plus the address of the variable named in the word.
 */
word
prinpar(word n, word aa, word b, word c, word d)
{
    word a = aa, t = enstack(0), i, j, kk, tm = 0, ae = 0, sh, s[3], sa;

    if (n == 5) {
        tm = ae = 0;
        goto l8;
    }
    if (n == 11) {
        if ((FREE(a) & 077) == 4 && (FREE(b) & 077) == 4) {
            FREES(a + 1, sx36(0) | ((FREE(a + 1) & 0xFFFFFFFFL) | ((FREE(b + 1) & 0xFFFFFFFFL) << 32)));
            FREES(a, FREE(a) & ~(MASK18 << 18) & MASK36);
            goto ppr1;
        }
        i = regof(dewop(0540, areg1(1, 013), a));
        hook(a, a, dewop(0504, i, b));
ppr1:
        ;
    }
    a = subarg(t, a);
    tm = u36(a) >> 18;
    a &= MASK18;
    if (tm)
        ae = 020000000L;
    else {
        tm = L18(FREE(a));
        ae = FREE(a + 1) & MASK18;
    }
l8:
    addcode(t, sx36(0600000000000L | tm), sx36(ae | (n << 24)));
    if (n != 6)
        goto l9;
    s[0] = b;
    s[1] = c;
    s[2] = d;
    for (i = 0; i <= 2; i++) {
        sa = s[i];
        if (sa == 0) {
            tm = ae = 0;
            goto l10;
        }
        sa = subarg(t, sa);
        tm = u36(sa) >> 18;
        sa &= MASK18;
        if (tm) {
            ae = 020000000L;
            goto l10;
        }
        tm = L18(FREE(sa));
        ae = FREE(sa + 1) & MASK18;
        if ((FREE(sa) & 077) == 4) {
            if (i) {                    /* project, programmer: octal */
                convc(tm);
                j = convc_const;
                sh = 0;
                tm = 0;
                ae = 0;
                while (j) {
                    kk = j / 10;
                    if ((kk = j - 10 * kk) > 7)
                        imp_error(1, "NON-OCTAL PJ OR PN **");
                    ae |= kk << sh;
                    sh += 3;
                    j = j / 10;
                }
            }
            if (tm == 0)
                tm = stcon(ae);
            ae = 0;
        }
l10:
        addcode(t, sx36(0600000000000L | tm), sx36(ae | (n << 24)));
        if (sa)
            hook(a, sa, a);
    }
l9:
    FREES(a + 1, FREE(t + 2));
    FREES(a, 4);                        /* so the matcher does not read word 2 */
    return a;
}

/* PRINCAL(A,B): add print list item b to the print call a. */
word
princal(word a, word b)
{
    word c = enstack(0);

    FREES(c + 2, FREE(b + 1));
    hook(b, b, hook(c, a, c));
    FREES(b, FREE(a));
    FREES(b + 1, FREE(a + 1));
    subpr0(1);
    return b;
}


/* the directory entry v is a text constant ('...') */
static int
istext(word v)
{
    char buf[4];

    dname_str(v, buf, sizeof buf);
    return buf[0] == '\'';
}

/*
 * BYTEP(A,S,P,FLAG): the byte pointer for the field of size s at position p
 * of a (A<S,P>).  The original builds a PDP-10 byte pointer word as constant
 * code and the instruction that uses it refers to that word.  Here the
 * result is a variable entry named for a descriptor kept in a table: the
 * operand a was (its two words), and s and p, each a constant or the number
 * of the register that holds it.  trans.c reads the descriptor where the
 * original has the pointer word: LDB and DPB do the field, MOVE of it gives
 * the byte pointer value (see trans.c).  FLAG is not used.
 */
struct bytedesc bytedescs[256];
int nbytedescs;

word
bytep(word a, word s, word p, word flag)
{
    word td, t, pty, sty;
    struct bytedesc *d;

    (void)flag;
    if (nbytedescs == 256) {
        imp_error(0, "TOO MANY BYTE POINTERS IN ONE PROGRAM");
        return a;
    }
    td = newname("BYTE");
    t = enstack(td);
    name(t);
    d = &bytedescs[nbytedescs++];
    d->name = td;
    /* the operand a as dewop would put it in an instruction's second word */
    unpck(a);
    {
        word ise = 0100000000000L & (u36(se1) >> 2), r = rse, dd = dse;

        if (tse == 2) {
            ise |= 04000;
            dd = rse;
            r = 0;
        } else if (tse == 4) {              /* A<S,P> of a constant (BYTEP 'HI'<7,36>): the pooled constant, in memory */
            ise = 0;
            r = 0;
            if (!(dse && istext(dse)))    /* a text constant is its own pooled string; a number is pooled by its value */
                dd = stcon(se1);
            cse = 0;
        }
        d->a0 = sx36(ise | r);
        d->a1 = sx36(dd | (cse << 18));
    }
    pty = FREE(p) & 077;
    if (pty == 4) {
        d->pconst = 1;
        d->p = FREE(p + 1);
    } else {
        d->pconst = 0;
        d->p = regof(fetch(p));
    }
    sty = FREE(s) & 077;
    if (sty == 4) {
        d->sconst = 1;
        d->s = FREE(s + 1);
    } else {
        d->sconst = 0;
        d->s = regof(fetch(s));
    }
    hook(a, hook(p, p, s), a);
    FREES(a + 1, 0);
    FREES(a, sx36(0110 | (td << 18)));
    return a;
}

/*
 * SUBSCRIPT(S,T): the subscripted variable s[t] (s = 0: [t], the word at
 * address t).
 */
word
subscript(word s, word t)
{
    word tty = FREE(t) & 077, sty, te, i, se_, sc;

    if (tty == 2 && reg0(03777 & (u36(FREE(t + 1)) >> 18)))
        goto l46;
    if (tty == 020 && FREE(t + 1) < 0) {
l46:
        dewop(0200, areg1(1, 015), t);
        tty = 2;
    }
    if (s == 0) {
        if (tty != 2 && tty != 4) {
            if (FREE(t + 1) < 0) {
                dewop(0200, areg1(1, 015), t);
                tty = 020;
                FREES(t, sx36(020 | (FREE(t) & 077L)));
            } else
                FREES(t + 1, sx36(FREE(t + 1) | ((word)1 << 35)));
        }
        te = 020 | (FREE(t) & ~077L);
        if (tty == 4) {
            te = 010 | (te & 0777700L);
            FREES(t + 1, FREE(t + 1) & MASK18);
        }
        FREES(t, sx36(te));
        return t;
    }
    if (tty != 2 && tty != 4) {
        dewop(0200, areg1(1, 015), t);
        tty = 2;
    }
    sty = FREE(s) & 077;
    if (sty == 020 && FREE(s + 1) < 0) {
        dewop(0201, areg1(1, 015), s);
        sty = 020;
        FREES(s, sx36(020 | (FREE(s) & ~077L)));
    }
    if (sty == 2) {
        imp_error(1, "SUBSCRIPTED REGISTER - IGNORED. ");
        return s;
    }
    if (sty == 4) {
        se_ = FREE(s) & ~077L;
        if ((u36(se_) >> 18) == 0) {
            imp_error(2, "CALCULATED CONSTANT IS SUBSCRIPTED. ");
            i = stcon(FREE(s + 1));
            se_ = (i << 18) | (se_ & MASK18);
        }
        FREES(s, sx36(se_ | 010));
        FREES(s + 1, 0);
    }
    if (tty == 2) {
        if (sty != 020) {
            FREES(s, sx36(020 | (FREE(s) & ~077L)));
            FREES(s + 1, sx36(FREE(s + 1) | (regof(t) << 18)));
        } else
            addcode(s, sx36(((word)0270 << 24) + (regof(s) << 12) + ((word)1 << 11)),
                regof(t));
    }
    if (tty == 4) {
        sc = ((FREE(s + 1) & MASK18) + FREE(t + 1)) & MASK18;
        FREES(s + 1, sx36(sc | (FREE(s + 1) & ~MASK18)));
    }
    return hook(s, t, s);
}

/* ---- registration ------------------------------------------------------ */

static word w_store(const word *p) { return store(p[0], p[1]); }
static word w_fetch(const word *p) { return fetch(p[0]); }
static word w_regof(const word *p) { return regof(p[0]); }
static word w_dewop(const word *p) { return dewop(p[0], p[1], p[2]); }
static word w_addop(const word *p) { return addop(p[0], p[1], p[2]); }
static word w_tag(const word *p) { return tag(p[0]); }
static word w_dewfun(const word *p) { return dewfun(p[0], p[1], p[2]); }
static word w_hook(const word *p) { return hook(p[0], p[1], p[2]); }
static word w_freeze(const word *p) { return freeze(p[0]); }
static word w_subrcall(const word *p) { return subrcall(p[0]); }
static word w_return(const word *p) { return return_(p[0]); }
static word w_subrpar(const word *p) { return subrpar(p[0], p[1]); }
static word w_conop(const word *p) { return conop(p[0], p[1], p[2], p[3], p[4]); }
static word w_float(const word *p) { return float_(p[0]); }
static word w_fix(const word *p) { return fix_(p[0]); }
static word w_ojumpop(const word *p) { return ojumpop(p[0]); }
static word w_datast(const word *p) { return datast(p[0]); }
static word w_prinpar(const word *p) { return prinpar(p[0], p[1], p[2], p[3], p[4]); }
static word w_princal(const word *p) { return princal(p[0], p[1]); }
static word w_addr(const word *p) { return addr_(p[0]); }
static word w_copy(const word *p) { return coppy(p[0]); }
static word w_subscript(const word *p) { return subscript(p[0], p[1]); }
static word w_subcall(const word *p) { return subcall(p[0], p[1]); }
static word w_bytep(const word *p) { return bytep(p[0], p[1], p[2], p[3]); }

void
codegen_init(void)
{
    if (cinit)
        return;
    cinit = 1;
    nco = 1;
    frelot(&co, "CODE", 200, NULL);
    fincset(co, 200);
    dsem("STORE", w_store);
    dsem("FETCH", w_fetch);
    dsem("REGOF", w_regof);
    dsem("DEWOP", w_dewop);
    dsem("ADDOP", w_addop);
    dsem("TAG", w_tag);
    dsem("DEWFUN", w_dewfun);
    dsem("HOOK", w_hook);
    dsem("FREEZE", w_freeze);
    dsem("SUBRCALL", w_subrcall);
    dsem("RETURN", w_return);
    dsem("SUBRPAR", w_subrpar);
    dsem("CONOP", w_conop);
    dsem("FLOAT", w_float);
    dsem("FIX", w_fix);
    dsem("OJUMPOP", w_ojumpop);
    dsem("DATAST", w_datast);
    dsem("PRINPAR", w_prinpar);
    dsem("PRINCAL", w_princal);
    dsem("ADDR", w_addr);
    dsem("COPY", w_copy);
    dsem("SUBSCRIPT", w_subscript);
    dsem("SUBCALL", w_subcall);
    dsem("BYTEP", w_bytep);
    (void)remo;
}

/* ANYCODE: has any real code been generated? */
word
anyco(void)
{
    return anco;
}
