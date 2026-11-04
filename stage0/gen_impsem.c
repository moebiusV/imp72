/*
 * gen_impsem.c -- ports src/llvm-ir/impsem.imp: the machine-independent semantic routines of the stage compilers for the
 * 64 bit port that compiles to IR: NAME, AREG, AREG1, REG2, TSTRN, VAL, SVAL, STACKUP, SWITCH, DECL, DECLARE, the register
 * tests (HISREG, PAIRFIRST, SUBREG, TMPREG, SAMEREG, REGEQ, REGSIM, REG0, RMATCH), the subroutine routines (SUBPR0, SUBPRM,
 * SUBTMPF, SUBBEG, SUBEND, NSUBP), REMOT, CALLME, REG2S, the lists (NEWLIST, ENLIST, GETLIST), ASSEMC, LETSYN, ERRSRT, CVAL
 * and FCON.  A register is a word of the array RG (the layout of the comment at AREG1 of the original): see gen.h for the
 * names.  Each C routine says which routine of impsem.imp it ports.
 *
 * The IMP locals are static; a routine here that is entered again (through SEMANTICS, which LETSYN calls) shares nothing it
 * would need to keep, so the C ones are automatic, except where a comment says so.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gen.h"

word g_arr, g_narr, g_regn;
word g_remo;

static int ginit;                               /* GINIT */
static word subpar, nsubpar, partmp, npartmp, lists, listb, liste, blist, regdec, nregdec, locv, nlocv, rg;
static word subtmpn, subtmp, subtmpk, subcnt, subrtn, subnam, relnm;
static word props, plong, pregsyn;
static word cvt, ncvt;
static word cpr, ncpr;                          /* CPR, NCPR: the C functions called or declared in the unit (CPADD) */

/* NEWNAME of a name given as a word: its first eight characters (imp.h's newname takes a string) */
word
g_newname_w(word w)
{
    char b[9];
    int k;

    for (k = 0; k < 8; k++)
        b[k] = (char)g_chr(w, k);
    b[8] = '\0';
    return newname(b);
}

/* IINIT(NIL): the arrays of this module, once */
word
g_iinit(void)
{
    word i;

    if (ginit != 0)
        return 0;
    ginit = 1;
    frelot(&subpar, "SUBRP", 5, NULL); nsubpar = 0;
    frelot(&partmp, "PARTM", 5, NULL); npartmp = 0;
    frelot(&lists, "LISTS", 50, NULL); listb = liste = blist = 0;
    frelot(&regdec, "RGDEC", 5, NULL); nregdec = 0;
    frelot(&locv, "LOCV", 5, NULL); nlocv = 0;
    frelot(&g_arr, "ARRYS", 5, NULL); g_narr = 0;
    frelot(&rg, "REGS", 300, NULL); g_regn = 65;
    for (i = 0; i <= 63; i++)
        FREES((rg + i) + 1, ((010000 & (i << 7)) | (0101 * (i & 037))));
    subtmpn = 0;
    subtmp = newname("PAR");
    return 0;
}

/* AREG(I): the register index of hardware register I (not a new one); with the 40B bit set it is a register defined by the user */
word
g_areg(word i)
{
    return i + 1;
}

word
g_reg2(word n)
{
    return n + 1;
}

/* AREG1(I,J): assign a register between I and J, giving its index */
word
g_areg1(word i, word j)
{
    word s, ii, jj;

    g_iinit();
    ii = i; jj = j;
    if (((ii | jj) & ~(word)0177) != 0) {
        imp_error(1, "IN AREG1 - I OR J OUT OF RANGE.");
        ii = 0; jj = 037;
    }
    s = g_regn;
    fadd(&rg, &g_regn, (jj | (ii << 6)));
    if ((jj & 040) != 0)
        fadd(&rg, &g_regn, ((jj & 037) | (ii << 6)));
    if (g_regn > 2048)
        imp_error(0, "MORE THAN 2047 REGISTERS IN ONE COMPILATION (THE INDEX IS 11 BITS IN THE RECORDS): SPLIT THE FILE.");
    return s;
}

/* TSTRN(DI): the register number if the name is one of 0R ... 15R, else -1.  The name is eight characters in the first word */
word
g_tstrn(word di)
{
    word w, c1, c2, c3, c4;

    w = dname(di, 0);
    c1 = g_chr(w, 0); c2 = g_chr(w, 1); c3 = g_chr(w, 2); c4 = g_chr(w, 3);
    if (c1 >= 060 && c1 <= 071 && c2 == 0122 && c3 == 0)
        return c1 - 060;
    if (c1 == 061 && c2 >= 060 && c2 <= 065 && c3 == 0122 && c4 == 0)
        return (c2 - 060) + 10;
    return -1;
}

/* NAME(S): turn the raw identifier at S into what it names (a register, a constant, a formal, a declared register, a plain name) */
word
g_name(word s)
{
    word d, j, aty, k, i, r16;

    g_iinit();
    if ((FREE(s) & 077) != 1)
        return s;
    d = HL(FREE(s));
    aty = (dprop(DP_ATYPE, d) << 6);
    if (aty == 0) {
        dprops(DP_ATYPE, d, 1);
        aty = (1 << 6);
    }
    if ((k = g_tstrn(d)) >= 0) {
        FREES(s, (2 | aty));
        k = g_areg(k + 040);
        FREES(s + 1, G_L32(k));
        return s;
    }
    if (convc(d) != 0) {
        FREES(s, ((aty | 4) | G_L32(d)));
        FREES(s + 1, convc_const);
        return s;
    }
    if (nlocv != 0) {
        i = nlocv - 1;
        while (i >= 0) {                                        /* NL1 */
            j = FREE(locv + i);
            if (d == HL(j)) {
                d = HR(j);
                aty = (dprop(DP_ATYPE, d) << 6);
                if (nsubpar != 0) {
                    i = nsubpar - 1;
                    while (i >= 0) {                            /* NL2 */
                        j = FREE(subpar + i);
                        if (d == j) {
                            FREES(s, (aty | 020));              /* the word at the address in the parameter's temporary */
                            r16 = FREE(partmp + i);
                            FREES(s + 1, G_L32(r16));
                            return s;
                        }
                        i = i - 1;
                    }
                }
                break;                                          /* GO TO NL3 */
            }
            i = i - 1;
        }
    }
    if (nregdec != 0) {                                         /* NL3 */
        i = nregdec - 1;
        while (i >= 0) {                                        /* NL4 */
            j = FREE(regdec + i);
            if (d == HR(j)) {
                FREES(s, (aty | 2));
                FREES(s + 1, j & ~G_HM);                        /* the left part of J */
                return s;
            }
            i = i - 1;
        }
    }
    FREES(s, (((010 | aty)) | G_L32(d)));
    FREES(s + 1, 0);
    return s;
}

word
g_val(word s)
{
    return HR(FREE(s + 1));
}

/* SVAL(N): a new entry holding N (J is a COMMON of the IMP module only by omission of its declaration) */
word
g_sval(word n)
{
    word j;

    j = enstack(0);
    FREES(j + 1, n);
    return j;
}

word
g_stackup(word v)
{
    word t;

    t = enstack(0);
    FREES(t, 4);
    FREES(t + 1, v);
    return t;
}

/* SWITCH 18 (TWOSEG) puts the code of the subroutines in the high segment behind a jump in the low one; there is one segment here */
word
g_switch(word n)
{
    if (n != 18)
        finam5 = (finam5 | g_ls(1, n - 1));
    return enstack(0);
}

word
g_nsubp(void)
{
    return nsubpar;
}

/* the tests of the registers */
word
g_hisreg(word i)
{
    g_iinit();
    return 010000 & FREE(rg + i);
}

word
g_pairfirst(word j)
{
    g_iinit();
    return 040 & FREE(rg + j);
}

word
g_subreg(word j)
{
    g_iinit();
    return 04000 & FREE(rg + j);
}

/* TMPREG(J): is register J a temporary that can be optimised out? */
word
g_tmpreg(word j)
{
    g_iinit();
    if (j <= 64)
        return 0;
    if ((FREE(rg + j) & 014040) != 0)
        return 0;
    return 1;
}

/* SAMEREG(J): a new register with the limits of register J */
word
g_samereg(word j)
{
    word k;

    if (g_tmpreg(j) == 0)
        return j;
    k = FREE(rg + j);
    return g_areg1((077 & g_rs(k, 6)), (077 & k));
}

word
g_regeq(word i, word j)
{
    word ii, jj;

    g_iinit();
    if (i == j)
        return 1;
    if (i == 0)
        return 0;
    if (j == 0)
        return 0;
    ii = FREE(rg + i);
    jj = FREE(rg + j);
    if ((037 & ii) == (037 & g_rs(ii, 6))) {
        if (ii == jj)
            return 1;
    }
    return 0;
}

word
g_regsim(word i, word j)
{
    g_iinit();
    if (i == j)
        return 1;
    if (i == 0)
        return 0;
    if (j == 0)
        return 0;
    return FREE(rg + i) == FREE(rg + j) ? 1 : 0;
}

word
g_reg0(word i)
{
    g_iinit();
    if (i == 0)
        return 0;
    return (FREE(rg + i) & 03700) == 0 ? 1 : 0;
}

/* RMATCH(I,J,RN): is register RN within the limits I..J? */
word
g_rmatch(word i, word j, word rn)
{
    word k;

    g_iinit();
    k = FREE(rg + rn);
    if (j < (037 & k))
        return 0;
    if (i > (037 & g_rs(k, 6)))
        return 0;
    return 1;
}

/* SUBPR0(I): 0 at subroutine calls, 1 at the end of a parameter list; keeps track of when the call temporaries can be reused */
word
g_subpr0(word i)
{
    if (i != 0) {
        subcnt = subcnt - 1;
        if (subcnt == 0)
            subtmpk = 0;
    } else
        subcnt = subcnt + 1;
    return 0;
}

word
g_subprm(word i)
{
    word j;

    (void)i;
    g_iinit();
    j = enstack(0);
    FREES(j, (010 | G_L32(subtmp)));
    FREES(j + 1, subtmpk);
    subtmpk = subtmpk + 1;
    if (subtmpk > subtmpn)
        subtmpn = subtmpk;
    return j;
}

word
g_subtmpf(void)
{
    g_iinit();
    if (subtmpn != 0) {
        fadd(&g_arr, &g_narr, (subtmp | G_L32(subtmpn)));
        subtmp = newname("PAR");
        subtmpn = 0;
        subtmpk = 0;
    }
    return 0;
}

/* SUBBEG(A): begin a subroutine named at stack entry A, whose formal arguments are the current list.  The code begins with an entry
 * record and, for each formal, a parameter record naming the temporary that gets the pointer; all subroutines can be re-entered */
word
g_subbeg(word a)
{
    word j, k, i, t;

    g_iinit();
    if (subrtn != 0)
        imp_error(1, "NESTED SUBROUTINES.");
    subrtn = 1;
    j = HL(FREE(a));
    subnam = j;
    dprops(DP_COM, j, 2);
    {
        word v1 = FREE(a + 1), v0 = FREE(a);            /* ADDREC(A,KOP,OENTRY,0,FREE(A),FREE(A+1),0) */

        g_addrec(a, KOP, OENTRY, 0, v0, v1, 0);
    }
    k = enstack(0);
    while (g_getlist(k) != 0) {                         /* SB1 */
        i = HL(FREE(k));
        j = g_newname_w(dname(i, 0));
        fadd(&locv, &nlocv, (j | G_L32(i)));
        fadd(&subpar, &nsubpar, j);
        t = g_areg1(0101, 0);
        fadd(&partmp, &npartmp, t);
        g_addrec(a, KOP, OPARAM, t, 0, 0, 0);
    }
    g_subtmpf();
    j = g_cphead(subnam);                               /* (a callback declared before the subr has its number of parameters) */
    if ((j & 1024) != 0 && ((j >> 4) & 31) != nsubpar) {
        char cmsg[320];

        snprintf(cmsg, 300, "CALLBACK %s HAS %ld PARAMETERS IN ITS DECLARATION AND THE SUBR HAS %ld.", g_irsymname(subnam), (long)((j >> 4) & 31), (long)nsubpar);
        imp_error(1, cmsg);
    }
    return a;
}

word
g_subend(void)
{
    subrtn = 0; subnam = 0; nlocv = 0; nsubpar = 0; npartmp = 0;
    return 0;
}

/* REMOT(A): code for the remote (high) segment.  It is kept in the chain REMO, which DEMIT puts at the end of the main program */
word
g_remot(word a)
{
    word m;

    g_iinit();
    if (g_remo == 0) {
        frelot(&g_remo, "REMOT", 3, NULL);
        FREES(g_remo + 2, 0);                           /* not a scratch entry of the stack: it lives until the unit is assembled */
    }
    m = enstack(0);                                     /* a marker in front of the code: the remote code of a routine is emitted with the routine */
    g_addrec(m, KOP, OREMOTE, 0, 0, 0, subnam);
    g_hook(m, m, a);
    g_hook(g_remo, g_remo, m);
    FREES(a + 2, 0);
    return a;
}

word
g_callme(word a)
{
    relnm = HL(FREE(a));
    return a;
}

word
g_reg2s(word s)
{
    word j, f, r;

    j = FREE(s + 1);
    f = (03777 & HL(j));                                /* the register field, bits 32-42 */
    r = g_reg2(f);
    FREES(s + 1, ((j & ~G_L32(03777)) | G_L32(r)));
    return s;
}

/* The lists: NEWLIST(S) opens a new list, with the stack entry S on it if S; ENLIST(S) adds S to it; GETLIST(S) puts the next entry of
 * the current list at S, and is 0 when there is none, which also closes the list and reopens the previous */
word
g_newlist(word s)
{
    g_iinit();
    if (flen(lists) <= (liste + 3))
        faddex(&lists, liste + 3);
    FREES(lists + liste, listb);
    FREES((lists + liste) + 1, liste);
    FREES((lists + liste) + 2, blist);
    liste = liste + 3; listb = liste; blist = liste;
    if (s != 0)
        g_enlist(s);
    return s;
}

word
g_enlist(word s)
{
    word v0, v1, v2;

    g_iinit();
    v0 = FREE(s); v1 = FREE(s + 1); v2 = FREE(s + 2);
    fadd(&lists, &liste, v0);
    fadd(&lists, &liste, v1);
    fadd(&lists, &liste, v2);
    return 0;
}

word
g_getlist(word s)
{
    word j;

    g_iinit();
    j = liste - blist;
    if (j != 0) {
        FREES(s, FREE(lists + blist));
        FREES(s + 1, FREE((lists + blist) + 1));
        FREES(s + 2, FREE((lists + blist) + 2));
        blist = blist + 3;
        return j / 3;
    }
    blist = FREE((lists + listb) - 1);
    liste = FREE((lists + listb) - 2);
    listb = FREE((lists + listb) - 3);
    return 0;
}

/* ASSEMC(A): the end of a compilation unit; A is the stack entry whose code is the whole program.  The chains are written as IR
 * (DEMIT), and listed first if /A */
word
g_assemc(word a)
{
    if (g_anycode() != 0) {
        g_subtmpf();
        if ((finam5 & 1) != 0) {
            fprintf(imp_out(), "\n** CODE PRODUCED BY PROGRAM\n");
            g_pcode(a, 077777777777L);
        }
        g_demit(a);
    }
    ncpr = 0;                                           /* the declarations of C functions are the unit's */
    g_streset(0);                                       /* and the structures */
    return 0;
}

/* DECL(I,S): note one property of a declaration: 0 a synonym for the register named at S, 1 a length (S is a constant), 2 and over a
 * flag (bit I of PROPS) */
word
g_decl(word i, word s)
{
    word j, k;

    g_iinit();
    if (i > 1) {
        props = (props | g_ls(1, i));
        return 0;
    }
    if (i == 0) {
        if (pregsyn != 0)
            imp_error(1, "TWO REGISTERS IN SAME DECLARATION.");
        j = HL(FREE(s));
        if ((k = g_tstrn(j)) >= 0)
            pregsyn = g_areg(k + 040);
        else
            imp_error(1, "ANTECEDENT NOT A HARDWARE REGISTER IN SYNONYM DECLARATION.");
        return 0;
    }
    if (plong != 0)
        imp_error(1, "TWO LENGTHS IN SAME DECLARATION. ");
    if ((FREE(s) & 077) == 4)
        plong = FREE(s + 1);
    else
        imp_error(1, "N NOT A CONSTANT IN N LONG.");
    return 0;
}

/* DECLARE(S): apply the declared properties to the names of the current list (their entries are put in the stack at S one at a time) */
word
g_declare(word s)
{
    word t, j, jj, k, i, old;

    g_iinit();
    t = enstack(0);
    while (g_getlist(s) != 0) {                         /* DL1 */
        j = HL(FREE(s));
        jj = 0;
        if (nlocv != 0) {
            i = nlocv - 1;
            while (i >= 0) {                            /* DL2 */
                k = FREE(locv + i);
                if (j == HL(k)) {
                    j = HR(k);
                    break;
                }
                i = i - 1;
            }
        }
        if ((props & 2048) != 0) {                      /* DL3: local */
            old = j;
            j = g_newname_w(dname(j, 0));
            fadd(&locv, &nlocv, (j | G_L32(old)));
        }
        if (nregdec != 0) {
            i = nregdec - 1;
            while (i >= 0) {                            /* DL4 */
                k = FREE(regdec + i);
                if (j == HR(k)) {
                    jj = HL(k);
                    break;
                }
                i = i - 1;
            }
        }
        if ((k = g_tstrn(j)) >= 0)                      /* DL5 */
            jj = g_areg(k + 040);
        if (pregsyn != 0) {
            jj = pregsyn;
            fadd(&regdec, &nregdec, (j | G_L32(jj)));
        }
        if ((props & 4) != 0)
            if (jj == 0)
                dprops(DP_COM, j, 1);
        if ((props & 4096) != 0)
            if (jj == 0)
                dprops(DP_COM, j, 3);                   /* external: a variable of the C library or of another object, nothing is defined here */
        if ((props & 8) != 0)
            dprops(DP_ATYPE, j, 2);
        if ((props & 16) != 0)
            dprops(DP_ATYPE, j, 1);
        if ((props & 32) != 0) {
            if (jj == 0) {
                jj = g_areg1(0101, 015);                /* register */
                fadd(&regdec, &nregdec, (j | G_L32(jj)));
            }
        }
        /* (PROPS AND 64): reserved, 128: scratch, 256: protected, 512: available -- nothing to do for a register of the IR */
        if ((props & 1024) != 0)
            if (jj != 0) {                              /* released */
                i = nregdec - 1;
                while (i >= 0) {                        /* DL6 */
                    if (j == HR(FREE(regdec + i)))
                        FREES(regdec + i, 0);
                    i = i - 1;
                }
            }
        if (plong != 0)
            if (jj == 0)
                fadd(&g_arr, &g_narr, (j | G_L32(plong)));
        if (jj != 0)
            if ((plong | (props & 28)) != 0)
                imp_error(1, "PROPERTY DECLARED IS NOT APPLICABLE TO REGISTER.");
        if (jj == 0)
            if ((props & 1984) != 0)
                imp_error(1, "PROPERTY MAY ONLY BE DECLARED FOR REGISTERS.");
    }
    props = plong = pregsyn = 0;                        /* DLX */
    return t;
}

/* LETSYN(S): LET X = Y.  (EQUOSE(S,NEWLIST(0)): the call evaluates its arguments from the right, so the list is opened before EQUOSE
 * reads it; EQUOSE takes the one argument.)  The IMP locals MAX, J and K are statics of the module and nothing reads them later. */
word
g_letsyn(word s)
{
    word max, j, k;

    max = FREE(s + 1);
    synt(enstack(dir("VBL")), 0);
    sytrm(enstack(-FREE(max)));
    syntax(1);
    j = max;
    while ((k = FREE(j + 1)) != 0) {
        FREES(j, k);
        j = j + 1;
    }
    FREES(j, 0);
    g_newlist(0);
    equose(s);
    semantics();
    return enstack(0);
}

/* ERRSRT(N,E): the semantic routine ERROR.  E is the (negated) directory index of the text constant; the IMP passes the address of the first
 * word of its name, which begins with the quote: the message here is the name without it */
word
g_errsrt(word n, word e)
{
    char buf[512];
    const char *m;

    dname_str(-e, buf, sizeof buf);
    m = buf[0] == '\'' ? buf + 1 : buf;
    imp_error((int)n, m);
    return g_name(enstack(dir("0")));
}

/* CVAL(A): the name A is a C function that takes its arguments by value (CCALL of lib/ffi.imc; not in the original).  ISCVAL(D): is the
 * name with directory index D one */
word
g_cval(word a)
{
    word d;

    d = HL(FREE(a));
    if (g_cpfind(d) < 0)
        g_cpadd(d, 0, 0, 0);                    /* (called in this unit: a declaration after this is an error) */
    if (g_iscval(d) != 0)
        return a;
    if (cvt == 0)
        frelot(&cvt, "CVALS", 16, NULL);
    fadd(&cvt, &ncvt, d);
    return a;
}

word
g_iscval(word d)
{
    word i;

    if (ncvt > 0) {
        i = 0;
        do {
            if (FREE(cvt + i) == d)
                return 1;
            i = i + 1;
        } while (i <= ncvt - 1);
    }
    return 0;
}

/* The typed declarations of C functions (EXTERNAL INT puts(PTR); of lib/ffi.imc; not in the original).  The types are numbered 1 CHAR, 2 UCHAR,
 * 3 SHORT, 4 USHORT, 5 INT, 6 UINT, 7 LONG, 8 ULONG, 9 LLONG, 10 ULLONG, 11 SIZE, 12 PTR, 13 DOUBLE, 14 FLOAT, 15 VOID.  CPR holds the names of the
 * unit that have been called or declared, four words each: the name (a directory index), the head, and the types of the parameters 0 to 7 and 8 to
 * 15, four bits each.  The head is the type of the result, plus 16 times the number of parameters, plus 512 if the function is variadic; 0 is a name
 * that has been called and is not declared.  The table is emptied when the unit ends (ASSEMC). */
word
g_ctypeof(word d)
{
    static const char *const tn[] = { "CHAR", "UCHAR", "SHORT", "USHORT", "INT", "UINT", "LONG", "ULONG", "LLONG", "ULLONG", "SIZE", "PTR",
                                      "DOUBLE", "FLOAT", "VOID" };
    const char *s = g_irsymname(d);
    int k;

    for (k = 0; k < 15; k++)
        if (strcmp(s, tn[k]) == 0)
            return k + 1;
    return 0;
}

/* the index in CPR of the name D, or -1 */
word
g_cpfind(word d)
{
    word i;

    if (ncpr > 0)
        for (i = 0; i <= ncpr - 4; i += 4)
            if (FREE(cpr + i) == d)
                return i;
    return -1;
}

void
g_cpadd(word d, word h, word t0, word t1)
{
    if (cpr == 0)
        frelot(&cpr, "CPROT", 16, NULL);
    fadd(&cpr, &ncpr, d); fadd(&cpr, &ncpr, h); fadd(&cpr, &ncpr, t0); fadd(&cpr, &ncpr, t1);
}

/* the head of the declaration of the name D, or 0 */
word
g_cphead(word d)
{
    word i = g_cpfind(d);

    return i < 0 ? 0 : FREE((cpr + i) + 1);
}

/* the type of parameter K of the declared name D */
word
g_cptype(word d, word k)
{
    word i = g_cpfind(d), w;

    if (i < 0)
        return 0;
    if (k < 8)
        return (FREE((cpr + i) + 2) >> (4 * k)) & 15;
    w = FREE((cpr + i) + 3);
    return (w >> (4 * (k - 8))) & 15;
}

/* EXTERNAL T F(L) and EXTERNAL T F(L, ...): V is 1 for the dots.  L is the list of the type names, or 0 */
word
g_cdecl(word t, word f, word l, word v)
{
    word r, d, np, nv, ty, h, t0, t1, e;
    char cmsg[320];

    d = HL(FREE(f));
    r = g_ctypeof(HL(FREE(t)));
    if (r == 0) {
        snprintf(cmsg, 300, "%s IS NOT A C TYPE.", g_irsymname(HL(FREE(t))));
        imp_error(1, cmsg);
        r = 5;
    }
    np = 0; nv = 0; t0 = 0; t1 = 0;
    if ((v & 4) != 0) {
        np = 1;
        t0 = 12;
    }
    if (l != 0)
        while (g_getlist(l) != 0) {
            ty = g_ctypeof(HL(FREE(l)));
            if (ty == 0) {
                snprintf(cmsg, 300, "%s IS NOT A C TYPE.", g_irsymname(HL(FREE(l))));
                imp_error(1, cmsg);
                ty = 5;
            }
            if (ty == 15)
                nv = nv + 1;
            if (np < 8)
                t0 = t0 | (ty << (4 * np));
            else if (np < 16)
                t1 = t1 | (ty << (4 * (np - 8)));
            np = np + 1;
        }
    if (np > 16) {
        imp_error(1, "MORE THAN 16 PARAMETERS IN A C FUNCTION.");
        np = 16;
    }
    if (nv > 0) {
        if (nv == 1 && np == 1 + (v & 4) / 4 && (v & 1) == 0) {
            np = (v & 4) / 4;
            t0 = (v & 4) != 0 ? 12 : 0;
        } else
            imp_error(1, "VOID IS ONLY THE WHOLE LIST OF PARAMETERS, AS IN (VOID).");
    }
    h = r + (16 * np);
    if ((v & 1) != 0)
        h = h + 512;
    if ((v & 2) != 0)
        h = h + 1024;
    if ((v & 4) != 0)
        h = h + 2048;
    e = g_cpfind(d);
    if (e < 0)
        g_cpadd(d, h, t0, t1);
    else if (FREE((cpr + e) + 1) == 0) {
        snprintf(cmsg, 300, "%s IS DECLARED AFTER A CALL OF IT THAT HAD NO DECLARATION.", g_irsymname(d));
        imp_error(1, cmsg);
    } else if (FREE((cpr + e) + 1) != h || FREE((cpr + e) + 2) != t0 || FREE((cpr + e) + 3) != t1) {
        snprintf(cmsg, 300, "%s IS DECLARED TWICE WITH DIFFERENT TYPES.", g_irsymname(d));
        imp_error(1, cmsg);
    }
    if ((v & 2) != 0)
        return enstack(0);
    if (r == 13 || r == 14)
        dprops(DP_ATYPE, d, 2);
    return enstack(0);
}

/* FCON(A,B,E): the floating constant A.B"E, whose parts are the names of stack entries A and B (digits) and the constant E (or 0), as the bits
 * of the IEEE 754 double nearest to it (the original builds a PDP-10 floating point word; the C library's sscanf does the conversion here) */
word
g_fcon(word a, word b, word e)
{
    char fbuf[256], nm[1024], dgw[48];
    int q = 0, fn, p, exn, i;
    word ex, bits = 0;
    double dv;

    fn = 0;
    dname_str(HL(FREE(a)), nm, sizeof nm);
    for (p = 0; nm[p] != '\0'; p++) {
        int fc = (unsigned char)nm[p];

        if (fc < 060 || fc > 071)
            goto fbad;
        if (fn < 100) {
            fbuf[q++] = (char)fc;
            fn = fn + 1;
        }
    }
    if (fn == 0)
        fbuf[q++] = (char)060;
    fbuf[q++] = (char)056;
    fn = 0;
    dname_str(HL(FREE(b)), nm, sizeof nm);
    for (p = 0; nm[p] != '\0'; p++) {
        int fc = (unsigned char)nm[p];

        if (fc < 060 || fc > 071)
            goto fbad;
        if (fn < 100) {
            fbuf[q++] = (char)fc;
            fn = fn + 1;
        }
    }
    if (fn == 0)
        fbuf[q++] = (char)060;
    fbuf[q++] = (char)0145;
    ex = 0;
    if (e != 0) {
        if ((FREE(e) & 077) != 4)
            goto fbad;
        ex = FREE(e + 1);
        if (ex > 308 || ex < -330) {
            imp_error(1, "EXPONENT OUT OF RANGE.");
            ex = 0;
        }
    }
    if (ex < 0) {
        ex = -ex;
        fbuf[q++] = (char)055;
    }
    exn = 0;                                            /* the digits of the exponent, the last first */
    do {
        dgw[exn] = (char)(060 + (ex - 10 * (ex / 10)));
        exn = exn + 1;
        ex = ex / 10;
    } while (ex > 0);
    for (i = exn - 1; i >= 0; i--)
        fbuf[q++] = dgw[i];
    fbuf[q] = '\0';
    dv = 0;
    sscanf(fbuf, "%lf", &dv);
    memcpy(&bits, &dv, 8);
    goto fdone;
fbad:
    imp_error(1, "ILLEGAL CHARACTER IN FLOATING CONSTANT.");
    bits = 0;
fdone:
    FREES(a, 0204);
    FREES(a + 1, bits);
    return a;
}

/* ---- the table of semantic routines: the DSEMs of IMPSEM ------------------------------------------------------------------ */

static word w_name(const word *p) { return g_name(p[0]); }
static word w_areg1(const word *p) { return g_areg1(p[0], p[1]); }
static word w_areg(const word *p) { return g_areg(p[0]); }
static word w_reg2(const word *p) { return g_reg2(p[0]); }
static word w_val(const word *p) { return g_val(p[0]); }
static word w_sval(const word *p) { return g_sval(p[0]); }
static word w_newlist(const word *p) { return g_newlist(p[0]); }
static word w_enlist(const word *p) { return g_enlist(p[0]); }
static word w_decl(const word *p) { return g_decl(p[0], p[1]); }
static word w_declare(const word *p) { return g_declare(p[0]); }
static word w_subbeg(const word *p) { return g_subbeg(p[0]); }
static word w_subend(const word *p) { (void)p; return g_subend(); }
static word w_subpr0(const word *p) { return g_subpr0(p[0]); }
static word w_remot(const word *p) { return g_remot(p[0]); }
static word w_assemc(const word *p) { return g_assemc(p[0]); }
static word w_callme(const word *p) { return g_callme(p[0]); }
static word w_switch(const word *p) { return g_switch(p[0]); }
static word w_stackup(const word *p) { return g_stackup(p[0]); }
static word w_reg2s(const word *p) { return g_reg2s(p[0]); }
static word w_error(const word *p) { return g_errsrt(p[0], p[1]); }
static word w_letsyn(const word *p) { return g_letsyn(p[0]); }
static word w_cval(const word *p) { return g_cval(p[0]); }
static word w_cdecl(const word *p) { return g_cdecl(p[0], p[1], p[2], p[3]); }
static word w_fcon(const word *p) { return g_fcon(p[0], p[1], p[2]); }

/* IMPSEM(NIL): the DSEMs of this file, in the order of impsem.imp */
void
gen_impsem_dsems(void)
{
    g_iinit();
    dsem("NAME", w_name);
    dsem("AREG1", w_areg1);
    dsem("AREG", w_areg);
    dsem("REG2", w_reg2);
    dsem("VAL", w_val);
    dsem("SVAL", w_sval);
    dsem("NEWLIST", w_newlist);
    dsem("ENLIST", w_enlist);
    dsem("DECL", w_decl);
    dsem("DECLARE", w_declare);
    dsem("SUBBEG", w_subbeg);
    dsem("SUBEND", w_subend);
    dsem("SUBPR0", w_subpr0);
    dsem("REMOT", w_remot);
    dsem("ASSEMC", w_assemc);
    dsem("CALLME", w_callme);
    dsem("SWITCH", w_switch);
    dsem("STACKUP", w_stackup);
    dsem("REG2S", w_reg2s);
    dsem("ERROR", w_error);
    dsem("LETSYN", w_letsyn);
    dsem("CVAL", w_cval);
    dsem("CDECL", w_cdecl);
    dsem("FCON", w_fcon);
}

/* gen_init: IMPSEM, CODEI (which enters CODE2I, which enters CODE3I) of the IMP, in that order: every semantic routine of the generator under the
 * name the syntax calls it.  The driver calls it once, after the kernel's own routines are entered and before the syntax is read. */
void
gen_init(void)
{
    gen_impsem_dsems();
    gen_code_dsems();
    gen_code2_dsems();
    gen_code3_dsems();
}
