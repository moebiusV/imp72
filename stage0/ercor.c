/*
 * ercor.c -- the parser's error corrector and disambiguator.
 * Ports decus/ercor.imc: ERCOR, FSUB, ISNAME, AMBIG.
 *
 * ERCOR(OP,NP,S) is called when every parse has died at symbol S; OP is the
 * array of the NP parses that were alive before S.  It searches the syntax
 * graph for the cheapest sequence of terminal symbols to insert before S that
 * lets some parse go on: it tries paths of up to MAXPATH terminals (then
 * skips S and tries the next input symbol), taking at each length the path
 * with the shortest goal list.  It returns the pointer to a free array of the
 * symbols to insert, S last, and a 0.
 *
 * The goal list CGL holds (node, goal) pairs.  The stack ST holds one word per
 * step of the path (original: type at bits 33-35 over the node; here
 * type << 56 | node):
 *   type 0-3   the graph node walked
 *   type 4     the node that called for a goal that has been reached; the word
 *              below it is HL width, HR goal, below that the type 2 node
 *   type 5     a node on the chain of terminal entry points (walked as type 3)
 *
 * The step-by-step trace the original prints with /G is reduced here to its
 * summary lines.
 */
#include <string.h>
#include "imp.h"

#define ERTY(w)      ((w) >> 56)
#define ENT(ty, nd)  (((word)(ty) << 56) | (nd))
#define MAXTERMS 50

static word erst, cgl, terms;           /* ST (ER/ST), CGL, TERMS */
static int init;
static word maxpath, pct, nam, zer;
static word nm;                         /* NM: see isname */

/* FSUB(Q,QQ): pop the stack. */
static word
fsub(word q, word *qq)
{
    if (--*qq < 0)
        imp_error(0, "** STACK UNDERFLOW IN FSUB **");
    return FREE(q + *qq);
}

/*
 * ISNAME(S): 1 if symbol s may be used as a symbol in syntax extensions, 2 if
 * it may be used as an identifier, 0 otherwise.  (The marks ; : % < are not
 * symbols.)  Anything of two or more characters, a digit or a letter is an
 * identifier.
 *
 * The original starts with `NM=R'''' => RETURN 2`, comparing NM left over
 * from the previous call with the apostrophe; NM never keeps that value, so
 * the test never succeeds.  It is kept, to keep NM's life the same.
 */
word
isname(word s)
{
    static int isnit;
    static word sem, col, pctd, lbr;
    word nz;

    if (!isnit) {
        sem = dir(";");
        col = dir(":");
        pctd = dir("%");
        lbr = dir("<");
        isnit = 1;
    }
    if (nm == 047)
        return 2;
    nm = dname(s, 0);
    if (nm & 0x1FFFFFFFL)               /* NM LS 7: more than one character */
        return 2;
    nm >>= 29;
    if (nm >= 060 && nm <= 071)
        return 2;
    nm &= 0137;
    if (nm >= 0101 && nm <= 0132)
        return 2;
    nz = 1;
    if (s == sem || s == col || s == pctd || s == lbr)
        nz = 0;
    return nz;
}

word
ercor(word op, word np, word ss)
{
    word s = ss, mp, nchars = 0, pn, ip, tcur, tpath, bwidth, width, nst, ncgl,
        node, cgoal = 0, j, i = 0, k, ty, val, nterms;
    FILE *f;
    int tr = (finam5 & SW_PTRACE) != 0;

    if (erst == 0)
        erst = fallot_new("ER/ST", 20);
    if (init == 0) {
        frelot(&cgl, "ER/CG", 20, NULL);
        maxpath = 3;
        pct = dir("%");
        nam = dir("NAM#");
        zer = dir("0");
    }
    if (++init > 50)
        imp_error(0, "** TOO MANY SYNTAX ERRORS **");
    f = imp_out();
    if (tr)
        fprintf(f, "\n** ERROR CORRECTOR ENTERED.\n");
    terms = -1;
    nst = ncgl = 0;
    pn = ip = tcur = tpath = bwidth = width = 0;

nextterm:
    pname(s);
    fputc(' ', f);
    mp = 1;
nmp:
    bwidth = tcur = pn = ip = 0;
    tpath = mp + 1;
    if (tr) {
        pname(s);
        fprintf(f, " IS NEW TERMINAL.\n");
    }
nextparse:
    if (++pn > np) {                    /* tried every parse at this length */
        if (terms > 0)
            goto x;
        if (++nchars > 5) {
            fputc('\n', f);
            imp_error(0, "** ERROR CORRECTOR GIVES UP. **");
        }
        if (++mp <= maxpath)
            goto nmp;
        if (s == pct)
            imp_error(0, "** ERROR CORRECTOR GIVES UP AT % **");
        lex(&s);
        goto nextterm;
    }
    if (tr)
        fprintf(f, "PARSE #%ld\n", (long)pn);
    nst = ncgl = 0;
    /* make the goal list from the parse */
    node = FREE(op + ip + 1);
    j = HR(FREE(op + ip));
    k = ip + 2;
    do {
        fadd(&cgl, &ncgl, HL(FREE(op + k)));
        i = HR(FREE(op + k));
        fadd(&cgl, &ncgl, i);
        if (tr) {
            fprintf(f, "  GOAL ");
            pname(gval(HL(FREE(op + k))));
            fprintf(f, " AT NODE #%ld\n", (long)HL(FREE(op + k)));
        }
        k++;
    } while (k <= ip + j);
    cgoal = i;
    ip = ip + j + 1;
    width = ncgl;

nextnode:
    if (node == 0)
        goto backup;
    ty = gtype(node);
    val = gval(node);
    if (tr) {
        fprintf(f, "NODE #%ld, TYPE %ld, ", (long)node, (long)ty);
        if (ty == 3)
            fprintf(f, "[%ld]", (long)val);
        else
            pname(val);
        fprintf(f, ", %ld TERMS. SO FAR, STACK %ld, GOALS %ld\n",
            (long)tcur, (long)nst, (long)ncgl);
    }
    switch (ty) {
    case 0:
        goto type0;
    case 1:
        goto type1;
    case 2:
        goto type2;
    case 3:
        goto type3;
    case 4:
        /* the original GO TO's NEXTNODE here, looping on the same node;
         * the graph has no type 4 nodes */
        imp_error(0, "** ERROR CORRECTOR: BAD NODE TYPE **");
        goto altfwd;
    default:
        goto type5;
    }

type5:
    ty = 3;
pushit:
type3:
    fadd(&erst, &nst, ENT(ty, node));
scrfwd:
    node = scr(node);
    goto nextnode;

type1:
    if (val == s) {                     /* found the terminal symbol */
fts:    if (tpath == tcur && width >= bwidth)
            goto toomany;
        nterms = 0;
        if (terms < 0)
            frelot(&terms, "TSYMS", 20, NULL);
        if (tr)
            fprintf(f, "** VALID PATH FOUND WITH %ld TERMINALS, BRANCHING "
                "MIN.%ld\n", (long)tcur, (long)(width / 2));
        for (i = 0; i < nst; i++) {
            j = FREE(erst + i);
            if (ERTY(j) == 1) {
                j = gval(j & HW_MASK);
                if (j == nam)
                    j = zer;
                fadd(&terms, &nterms, j);
            }
        }
        fadd(&terms, &nterms, s);
        fadd(&terms, &nterms, 0);
        bwidth = width;
        tpath = tcur;
        goto altfwd;
    }
    if (isname(s) == 2 && val == nam)
        goto fts;
    if (++tcur < tpath)
        goto pushit;
    if (tcur == tpath && width < bwidth)
        goto pushit;
    tcur--;
toomany:
altfwd:
    node = gnext(alt(node), cgoal);
    goto nextnode;

type2:
    if (gmade(node, cgoal)) {
        if (ncgl <= 4)
            goto pushit;
        fadd(&erst, &nst, ENT(2, node));
        fadd(&erst, &nst, HW(width, cgoal));
        j = FREE(cgl + ncgl - 2);
        fadd(&erst, &nst, ENT(4, j));
        ncgl -= 2;
        if (ncgl < width)
            width = ncgl;
        cgoal = FREE(cgl + ncgl - 1);
        node = scr(FREE(cgl + ncgl));
        goto nextnode;
    }
    goto pushit;

type0:
    fadd(&cgl, &ncgl, node);
    cgoal = dprop(DP_NODE, val);
    fadd(&cgl, &ncgl, cgoal);
    fadd(&erst, &nst, node);
    node = gentry();
    if (gconn(node, cgoal))
        goto nextnode;
    goto altfwd;

backup:
    if (nst == 0)
        goto nextparse;
    j = fsub(erst, &nst);
    ty = ERTY(j);
    node = j & HW_MASK;
    switch (ty) {
    case 0:                             /* pop a goal */
        ncgl -= 2;
        cgoal = FREE(cgl + ncgl - 1);
        goto altfwd;
    case 1:
        tcur--;
        goto altfwd;
    case 4:                             /* back through a goal-reached node */
        fadd(&cgl, &ncgl, node);
        j = fsub(erst, &nst);
        cgoal = j & HW_MASK;
        fadd(&cgl, &ncgl, cgoal);
        width = HL(j);
        j = FREE(erst + nst - 1);
        node = j & HW_MASK;
        goto scrfwd;
    default:                            /* 2, 3 */
        goto altfwd;
    }
x:
    return terms;
}

/*
 * AMBIG(P,NP,PN,O,TR): compare all pairs of the *pn parses in array p (with
 * *np words); where two are identical keep the one whose output is better:
 * fewer names matched as NAM#/SYM#, else shorter output.  Equal ones are an
 * unresolvable ambiguity.  Removes the losers, adjusting *np and *pn.
 */
void
ambig(word p, word *np, word *pn, word o, word tr)
{
    word i, j, il, jl, ni, nj, k, io, jo, it, jt, same, longer;
    FILE *f = imp_out();

a1: i = p;
    ni = 0;
    il = -1;
a4: ni++;
    i += il + 1;
    if (ni == *pn)
        return;
    il = HR(FREE(i));
    j = i;
    nj = ni;
    jl = il;
a3: if (++nj > *pn)
        goto a4;
    j += jl + 1;
    jl = HR(FREE(j));
    if (il != jl)
        goto a3;
    for (k = 1; k <= il; k++)
        if (FREE(i + k) != FREE(j + k))
            goto a3;
    if (tr)
        fprintf(f, "AMBIGUITY FOUND - IDENTICAL PARSES # %3ld,%ld\n",
            (long)ni, (long)nj);
    io = HL(FREE(i));
    jo = HL(FREE(j));
    it = jt = same = 0;
    while (io && jo) {
        io = FREE(o + io);
        jo = FREE(o + jo);
        if (HL(io) != HL(jo))
            same = 1;
        if (io < 0)
            it++;
        if (jo < 0)
            jt++;
        io = HR(io);
        jo = HR(jo);
    }
    if ((io | jo | same) == 0) {
        if (tr)
            fprintf(f, "OUTPUTS IDENTICAL");
        goto offj;
    }
    longer = io - jo;
    while (io) {
        io = FREE(o + io);
        if (io < 0)
            it++;
        io = HR(io);
    }
    while (jo) {
        jo = FREE(o + jo);
        if (jo < 0)
            jt++;
        jo = HR(jo);
    }
    if (it != jt) {
        if (tr)
            fprintf(f, "MORE TERMINALS");
        if (it > jt)
            goto offi;
        goto offj;
    }
    if (longer == 0) {
        imp_error(1, "SYNTACTIC AMBIGUITY.");
        finam5 |= 040000L;              /* turn on the listing */
        io = HL(FREE(i));
        if (io) {
            fprintf(f, " * IDENTIFIERS IN AMBIGUOUS SEGMENT (IN REVERSE "
                "ORDER): \n ");
            while (io) {
                io = FREE(o + io);
                if (io < 0) {
                    pname(-(word)(int32_t)HL(io));
                    fputc(' ', f);
                }
                io = HR(io);
            }
            fputc('\n', f);
        }
        goto offj;
    }
    if (tr)
        fprintf(f, "LONGER");
    if (longer > 0)
        goto offi;
offj:
    i = j;
    ni = nj;
    il = jl;
offi:
    if (tr)
        fprintf(f, ", #%ld DISCARDED.\n", (long)ni);
    /* move everything above parse i down over it */
    il++;
    j = i + il;
    jl = p + *np - 1;
    if (j <= jl)
        memmove(freep(i), freep(j), (size_t)(jl - j + 1) * sizeof(word));
    *np -= il;
    if (--*pn > 1)
        goto a1;
}
