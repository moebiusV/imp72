/*
 * impsem.c -- the machine-independent semantic routines of decus/impsem.imc
 * that declarations need: NAME, TSTRN, AREG, AREG1, REG2, DECL, DECLARE, VAL,
 * SVAL, STACKUP, SWITCH, NSUBP.  (NEWLIST, ENLIST, GETLIST are in sem.c.)
 *
 * NAME(S) turns the raw identifier at stack entry s into what it names: a
 * hardware register (0R, 1R, ... 15R), a constant, a formal argument of the
 * subroutine being compiled, a register declared for a name, or a plain
 * identifier.  DECL(I,S) collects the properties of a declaration
 * (`X, Y IS COMMON, REAL, 5 LONG`) and DECLARE applies them to the names on
 * the current list.
 *
 * Register declarations call DEWFUN, which generates code and is not ported.
 *
 * REGISTERS are described by words of array RG, in the layout of the
 * original (the comment at AREG1 in impsem.imc): bit 35 a hardware register
 * has been assigned, bits 29-18 the reference count, 17-13 the hardware
 * register, bit 12 not a temporary, bit 11 a subroutine call, bits 10-6 the
 * lower limit, bit 5 two successive registers, bits 4-0 the upper limit.
 */
#include "imp.h"

static word locv, nlocv, subpar, nsubpar, regdec, nregdec, arr, narr, rg, regn;
static word props, plong, pregsyn, subtmpn, subtmp;
static int ginit;

static void
init(void)
{
    word i;

    if (ginit)
        return;
    ginit = 1;
    frelot(&subpar, "SUBRP", 5, NULL);
    nsubpar = 0;
    frelot(&regdec, "RGDEC", 5, NULL);
    nregdec = 0;
    frelot(&locv, "LOCV", 5, NULL);
    nlocv = 0;
    frelot(&arr, "ARRYS", 5, NULL);
    narr = 0;
    frelot(&rg, "REGS", 300, NULL);
    regn = 65;
    for (i = 0; i <= 63; i++)
        FREES(rg + i + 1, (word)((10000L & (i << 7)) | (0101L * (i & 037))));
    dir("IMPM.");
    subtmpn = 0;
    subtmp = newname("PAR");
}

/* AREG(I) is the register index of hardware register i (not a new one); with
 * the 40B bit set it is a register defined by the user. */
word areg(word i) { return i + 1; }
word reg2(word n) { return n + 1; }

/* AREG1(I,J): assign a register between i and j, giving its index. */
word
areg1(word i, word j)
{
    word s;

    init();
    if (((i | j) & ~0177L) != 0) {
        imp_error(1, "IN AREG1 - I OR J OUT OF RANGE.");
        i = 0;
        j = 037;
    }
    s = regn;
    fadd(&rg, &regn, j | (i << 6));
    if (j & 040)
        fadd(&rg, &regn, (j & 037) | (i << 6));
    return s;
}

/* TSTRN(J): the register number if the name is one of 0R ... 15R, else -1. */
word
tstrn(word dirindex)
{
    const char *s = gname(dirindex);
    size_t n = 0;

    while (s[n])
        n++;
    if (n == 2 && s[1] == 'R' && s[0] >= '0' && s[0] <= '9')
        return s[0] - '0';
    if (n == 3 && s[0] == '1' && s[2] == 'R' && s[1] >= '0' && s[1] <= '5')
        return 10 + (s[1] - '0');
    return -1;
}

word
name(word s)
{
    word d, j, aty, k, i, r16;

    init();
    if ((FREE(s) & 077) != 1)
        return s;
    d = L18(FREE(s));
    aty = ls36(dprop(DP_ATYPE, d), 6);
    if (aty == 0) {
        dprops(DP_ATYPE, d, 1);
        aty = ls36(1, 6);
    }
    if ((k = tstrn(d)) >= 0) {
        FREES(s, sx36(2 | aty));
        k = areg(k + 040);
        FREES(s + 1, sx36(k << 18));
        return s;
    }
    if (convc(d)) {
        FREES(s, sx36(aty | 4 | (d << 18)));
        FREES(s + 1, convc_const);
        return s;
    }
    if (nlocv) {
        for (i = nlocv - 1; i >= 0; i--) {
            j = FREE(locv + i);
            if (d == L18(j)) {
                d = R18(j);
                aty = ls36(dprop(DP_ATYPE, d), 6);
                if (nsubpar) {
                    for (i = nsubpar - 1; i >= 0; i--) {
                        j = FREE(subpar + i);
                        if (d == j) {       /* subroutine parameter number i */
                            FREES(s, sx36(aty | 020));
                            r16 = areg(016);
                            FREES(s + 1, sx36(0400000000000L | i
                                | (r16 << 18)));
                            return s;
                        }
                    }
                }
                break;
            }
        }
    }
    if (nregdec) {
        for (i = nregdec - 1; i >= 0; i--) {
            j = FREE(regdec + i);
            if (d == R18(j)) {              /* was declared a register */
                FREES(s, sx36(aty | 2));
                FREES(s + 1, sx36(j & ~MASK18));
                return s;
            }
        }
    }
    FREES(s, sx36(010 | aty | (d << 18)));
    FREES(s + 1, 0);
    return s;
}

word val(word s) { return R18(FREE(s + 1)); }

word
sval(word n)
{
    word j = enstack(0);

    FREES(j + 1, n);
    return j;
}

word
stackup(word v)
{
    word t = enstack(0);

    FREES(t, 4);
    FREES(t + 1, v);
    return t;
}

word
switch_(word n)
{
    finam5 |= (word)1 << (n - 1);
    return enstack(0);
}

word nsubp(void) { return nsubpar; }

/*
 * DECL(I,S): note one property of a declaration: 0 a synonym for the
 * register named at s, 1 a length (s is a constant), 2 and over a flag
 * (bit i of PROPS).
 */
word
decl(word i, word s)
{
    word j, k;

    init();
    if (i > 1) {
        props |= ls36(1, i);
        return 0;
    }
    if (i == 0) {
        if (pregsyn)
            imp_error(1, "TWO REGISTERS IN SAME DECLARATION.");
        j = L18(FREE(s));
        if ((k = tstrn(j)) >= 0)
            pregsyn = areg(k + 040);
        else
            imp_error(1, "ANTECEDENT NOT A HARDWARE REGISTER IN SYNONYM "
                "DECLARATION.");
        return 0;
    }
    if (plong)
        imp_error(1, "TWO LENGTHS IN SAME DECLARATION. ");
    if ((077 & FREE(s)) == 4)
        plong = FREE(s + 1);
    else
        imp_error(1, "N NOT A CONSTANT IN N LONG.");
    return 0;
}

static void
dewfun(word t, word code, word jj)
{
    (void)t;
    (void)code;
    (void)jj;
    imp_error(0, "SEMANTIC ROUTINE DEWFUN IS NOT YET PORTED");
}

/* DECLARE(S): apply the declared properties to the names of the current
 * list (their entries are put in the stack at s one at a time). */
word
declare(word s)
{
    word t, j, jj, k, i, old;

    init();
    t = enstack(0);
    while (getlist(s)) {
        j = L18(FREE(s));
        jj = 0;
        if (nlocv) {
            for (i = nlocv - 1; i >= 0; i--) {
                k = FREE(locv + i);
                if (j == L18(k)) {
                    j = R18(k);
                    break;
                }
            }
        }
        if (props & 2048) {                     /* LOCAL */
            old = j;
            j = newname(gname(j));
            fadd(&locv, &nlocv, sx36(j | (old << 18)));
        }
        if (nregdec) {
            for (i = nregdec - 1; i >= 0; i--) {
                k = FREE(regdec + i);
                if (j == R18(k)) {
                    jj = L18(k);
                    break;
                }
            }
        }
        if ((k = tstrn(j)) >= 0)
            jj = areg(k + 040);
        if (pregsyn) {
            jj = pregsyn;
            fadd(&regdec, &nregdec, sx36(j | (jj << 18)));
        }
        if ((props & 4) && jj == 0)
            dprops(DP_COM, j, 1);
        if (props & 8)
            dprops(DP_ATYPE, j, 2);
        if (props & 16)
            dprops(DP_ATYPE, j, 1);
        if (props & 32) {                       /* REGISTER */
            if (jj == 0) {
                jj = areg1(0101, 015);
                fadd(&regdec, &nregdec, sx36(j | (jj << 18)));
            }
            dewfun(t, 5, jj);
        }
        if ((props & 64) && jj)
            dewfun(t, 6, jj);                   /* RESERVED */
        if ((props & 128) && jj)
            dewfun(t, 9, jj);                   /* SCRATCH */
        if ((props & 256) && jj)
            dewfun(t, 10, jj);                  /* PROTECTED */
        if ((props & 512) && jj)
            dewfun(t, 8, jj);                   /* AVAILABLE */
        if ((props & 1024) && jj) {             /* RELEASED */
            for (i = nregdec - 1; i >= 0; i--)
                if (j == R18(FREE(regdec + i)))
                    FREES(regdec + i, 0);
            dewfun(t, 8, jj);
        }
        if (plong && jj == 0)
            fadd(&arr, &narr, sx36(j | (plong << 18)));
        if (jj && ((plong | (props & 28)) != 0))
            imp_error(1, "PROPERTY DECLARED IS NOT APPLICABLE TO REGISTER.");
        if (jj == 0 && (props & 1984))
            imp_error(1, "PROPERTY MAY ONLY BE DECLARED FOR REGISTERS.");
    }
    props = plong = pregsyn = 0;
    return t;
}

static word w_name(const word *p) { return name(p[0]); }
static word w_areg1(const word *p) { return areg1(p[0], p[1]); }
static word w_areg(const word *p) { return areg(p[0]); }
static word w_reg2(const word *p) { return reg2(p[0]); }
static word w_val(const word *p) { return val(p[0]); }
static word w_sval(const word *p) { return sval(p[0]); }
static word w_decl(const word *p) { return decl(p[0], p[1]); }
static word w_declare(const word *p) { return declare(p[0]); }
static word w_stackup(const word *p) { return stackup(p[0]); }
static word w_switch(const word *p) { return switch_(p[0]); }

void
impsem_init(void)
{
    dsem("NAME", w_name);
    dsem("AREG1", w_areg1);
    dsem("AREG", w_areg);
    dsem("REG2", w_reg2);
    dsem("VAL", w_val);
    dsem("SVAL", w_sval);
    dsem("DECL", w_decl);
    dsem("DECLARE", w_declare);
    dsem("STACKUP", w_stackup);
    dsem("SWITCH", w_switch);
}
