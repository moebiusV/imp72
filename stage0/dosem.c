/*
 * dosem.c -- the semantics array and its interpreter.  Ports decus/dosem.imc:
 * DOSEM, SEMFIX, SEMFX1, DOCASE, ENPROD, MATCHP, NOTEPROD, PLIES, SNODEN,
 * ENSTACK, RESETSEM, SETSEM, PAR, SEMPER, SNARG, SEMGET, SEMED, SEMOR, CKSVAL.
 *
 * The semantics of a production is a run of words in free array SEMS (36 bit
 * words; V = bits 17-0, N = bits 29-18, TYPE = bits 35-30):
 *
 *   0  start of semantics with N arguments; bits 29-27 are a priority for
 *      immediate execution; V is the production's entry in PROSEM
 *   1  end of semantics (so is a second type 0)
 *   2  push the stack pointer of parameter V on PSTAK
 *   3  call semantic routine V with the bottom N entries of PSTAK (at most
 *      10) and push the result
 *   4  push the value -V as the next parameter
 *   5  enter a node with value V at the next position of the code stack
 *   6  enter parameter V of the stack in the next position
 *   7  copy parameter V of the stack to the next position
 *   10 load a constant on the stack of parameters for 'CASE'
 *   11 add the last two stack elements
 *   12 put a type 0 node for [V], value N, on the stack
 *
 * PROSEM records, for each production, where it can match in the code tree;
 * see ENPROD below.  Its words, like those of SEMS, keep the original
 * layout.
 *
 * Semantic routines are C functions called through the table of sem-pdp10.c; the
 * original jumps to the routine's address.
 */
#include <string.h>
#include "imp.h"

word sems, nsems, prosem, nprosem, scratch;
static word pstak, npstak, casep, ncasep;
static word many = 31, nsnodes;
static word pars[MARGS];
static int snit;

#define SW_SEMTRACE 0100000L            /* /P  trace semantics execution */

word
par(word i)
{
    return FREE(casep + i);
}

static word
w_par(const word *p)
{
    return par(p[0]);
}

static word
w_stack(const word *p)
{
    return stack(p[0]);
}

/* SETSEM(TY,VAL): add a word to SEMS; returns its index (never 0). */
word
setsem(word ty, word val)
{
    word v;

    if (!snit) {
        snit = 1;
        frelot(&sems, "SEMS", 2200, NULL);
        nsems = 2;
        many = 31;
        FREES(sems + 1, 010000000001L);
        frelot(&pstak, "SPSTK", 20, NULL);
        npstak = 0;
        frelot(&prosem, "PROSM", 1200, NULL);
        nprosem = 1;
        frelot(&casep, "CASEP", 10, NULL);
        dsem("PAR", w_par);
        dsem("STACK", w_stack);
        nsnodes = 0;
    }
    v = nsems;
    fadd(&sems, &nsems, sx36(val | (ty << 30)));
    return v;
}

void
resetsem(word val, word a)
{
    word j = FREE(sems + a);

    FREES(sems + a, sx36(R18(j) | (val << 18)));
}

word snarg(word n) { return 0777 & (u36(FREE(sems + n)) >> 18); }
word semed(word a) { return 7 & (u36(FREE(sems + a)) >> 27); }

void
semor(word a, word n)
{
    word qq = FREE(sems + n);

    FREES(sems + n, sx36(qq | a));
}

/* SEMGET(N): the index in SEMS of the semantics of production N. */
word
semget(word n)
{
    word q = R18(FREE(sems + n));

    q = FREE(prosem + q + 1);
    return R18(FREE(prosem + q + 1));
}

/* PLIES(I): the bit vector of levels of the code tree at which production i
 * matches, or 0 if it matches at no terminal node. */
word
plies(word i)
{
    word j = R18(FREE(sems + i));

    return j ? FREE(prosem + j) : j;
}

word
snoden(word i)
{
    word j = R18(FREE(sems + i));

    return j ? FREE(prosem + j + 2) : j;
}

/* CKSVAL(V,I): is production v a value (type 12) case of i? */
word
cksval(word v, word i)
{
    word q = u36(FREE(sems + v + 1));

    return (q & 0770000777777L) == (u36(i | 0120000000000L)) ? 1 : 0;
}

/* NOTEPROD(PN,N): record that production pn matches at ply n of the tree. */
void
noteprod(word pn, word n)
{
    word j = FREE(sems + pn), k = R18(j);

    if (k == 0) {
        k = nprosem;
        fadd(&prosem, &nprosem, 0);
        fadd(&prosem, &nprosem, 0);
        fadd(&prosem, &nprosem, ++nsnodes);
        FREES(sems + pn, sx36(j | k));
    }
    j = FREE(prosem + k);
    if (j == 0)
        j = -ls36(1, n);
    else {
        if (j < 0)
            j = -j;
        j |= ls36(1, n);
    }
    FREES(prosem + k, j);
}

/*
 * ENPROD(PN,NO,SEM): record that production pn matches at terminal node no
 * of the tree, with semantics sem.  PROSEM, for production pn, holds
 *   word k    the plies vector (negative while pn has been entered once only)
 *   word k+1  the head of a list of terminal nodes
 *   word k+2  the production's row in the matrix SBITM
 * and each list node is
 *   word 0    HL: the tree node  HR: the next list node (0 ends)
 *   word 1    HL: if nonzero, this is a case of a class and the semantics at
 *             the low half sets up the parameters; this is where to start
 *             executing the class's semantics  HR: the index in SEMS of the
 *             semantics to invoke
 */
void
enprod(word pn, word no, word sem)
{
    word j = FREE(sems + pn), k = R18(j);

    if (k == 0) {
        k = nprosem;
        fadd(&prosem, &nprosem, 0);
        fadd(&prosem, &nprosem, 0);
        FREES(sems + pn, sx36(j | k));
    }
    j = FREE(prosem + k + 1);
    FREES(prosem + k + 1, nprosem);
    fadd(&prosem, &nprosem, sx36(j | (no << 18)));
    fadd(&prosem, &nprosem, sem);
}

/* MATCHP(PN,N): the semantics entry if production pn matches at node n. */
word
matchp(word pn, word n)
{
    word j = R18(FREE(sems + pn)), k, jx;

    if (j == 0)
        return j;
    j = FREE(prosem + j + 1);
    while (j) {
        jx = prosem + j;
        k = FREE(jx);
        if (n == L18(k))
            return FREE(jx + 1);
        j = R18(k);
    }
    return j;
}

/* DOCASE(ID,SEMN,PRODN): make PROSEM entries for all the cases of class id
 * under production prodn, whose setup semantics is semn. */
void
docase(word id, word semn, word prodn)
{
    word jd = dprop(DP_SEM, id), je, kd, ke;

    if (jd == 0) {
        imp_error(1, "CASE UNDEFINED ** ");
        pname(id);
        fputc('\n', imp_out());
        return;
    }
    jd = R18(FREE(sems + jd));
    je = FREE(prosem + jd);                     /* levels of the class */
    if (je < 0)
        je = -je;
    noteprod(prodn, -1);
    kd = prosem + R18(FREE(sems + prodn));
    ke = FREE(kd);
    if (ke < 0)
        ke = -ke;
    FREES(kd, sx36(ke | je));
    jd = FREE(prosem + jd + 1);
    while (jd) {
        je = FREE(prosem + jd + 1);
        jd = FREE(prosem + jd);
        enprod(prodn, L18(jd), sx36(semn | (je << 18)));
        jd = R18(jd);
    }
}

/*
 * ENSTACK(V): a scratch node on the semantics stack for the directory index
 * v (a positive or negative one).
 */
word
enstack(word v)
{
    word vv = v, w;

    if (scratch > many)
        imp_error(0, " SEMANTICS STACK OVERFLOW **");
    if (v < 0)
        vv = -v;
    w = setstk(scratch, 1 | (vv << 18), 0, 0);
    scratch++;
    return w;
}

/* SEMFIX(I,A): fix up the semantics at i, invoked by a quoted conditional;
 * a is a free array of the indices of the local names of the arguments,
 * ended by -1.  (Changes to the types must be made here and in DOSEM.) */
word
semfix(word i, word a)
{
    word na = 0, i0 = 0, k, j, jt, kk, kt;

    while ((k = FREE(a + na)) >= 0)
        na++;
    j = FREE(sems + i);
    FREES(sems + i, sx36((na << 18) | (u36(j) & 0776000777777L)));
    na--;
    j = i;
l3: j++;
    k = FREE(sems + j);
    jt = u36(k) >> 30;
    switch (jt) {
    case 0:
        if (i0)
            return 0;
        i0 = 1;
        goto l3;
    case 1:
        return 0;
    case 2:
    case 6:
    case 7:
        jt = 4 | (2 & (jt >> 1));
        k = u36(synarg(R18(k))) | 040000000000L;
        k = sx36(k);
        FREES(sems + j, k);
        goto l14;
    case 4:
    case 5:
        goto l14;
    default:
        goto l3;
    }
l14:
    if ((u36(k) & 004000000000L) == 0 && na >= 0 && (k = R18(k)) != 0) {
        for (kk = 0; kk <= na; kk++) {
            kt = FREE(a + kk);
            if (kt == -k) {
                FREES(sems + j, sx36(070000000000L | kk));
                goto l3;
            }
            if (kt == k) {
                if (jt == 4)
                    FREES(sems + j, sx36(020000000000L | kk));
                else {
                    FREES(sems + j, sx36(060000000000L | kk));
                    FREES(a + kk, -kt);
                }
                goto l3;
            }
        }
    }
    /* check for VAL/ arguments */
    if (u36(k) & 004000000000L) {
        kt = -sx36(k | 0770000000000L);
        if (kt < 01000000)
            goto l3;
        if (na >= 0) {
            for (kk = 0; kk <= na; kk++) {
                if (FREE(a + kk) == kt) {
                    FREES(sems + j, sx36(0600000000000L | kk));
                    goto l3;
                }
            }
        }
    }
    goto l3;
}

/* SEMFX1(I,A,NA): patch up a possible VAL/"STUFF" default case.  Type 5
 * references to productions which are cases of VAL become type 6 if VAL is
 * in the parameter list a (of na names); the parameter list entry becomes
 * the production number for VAL. */
word
semfx1(word i, word a, word na)
{
    word j, k, jt, kk, jj;

    if (na == 0)
        return 0;
    j = i + 1;
    for (;;) {
        k = FREE(sems + j);
        jt = u36(k) >> 30;
        if (jt == 5 && (u36(k) & 004000000000L)) {
            k = -sx36(0770000000000L | k);
            if (k >= 01000000) {
                k = R18(k);
                for (kk = na - 1; kk >= 0; kk--) {
                    jj = -FREE(a + kk);
                    if (jj < 0)
                        jj = dprop(DP_SEM, -jj);
                    if (jj == k) {
                        FREES(a + kk, -(jj | (jj << 18)));
                        FREES(sems + j, sx36(060000000000L | kk));
                    }
                }
            }
        }
        if (jt > 1) {
            j++;
            continue;
        }
        return 0;
    }
}

/*
 * DOSEM(PPO): interpret the semantics of a production.  ppo has in its low
 * half the index in SEMS to start at; its high half, if not 0, is where to
 * go on after the first end of semantics (a case of a class).  The result is
 * left in the first stack entry (stack(0)).
 */
word
dosem(word ppo)
{
    word npo = L18(ppo), po = R18(ppo), s, se, sty, sval, sn, jx, i, j, k, tr,
        nt0 = 0, xname;

    if (po == 1)
        return 0;
    tr = (finam5 & SW_SEMTRACE) != 0;
    s = po;
    ncasep = 0;
    FREES(pstak, 0);
nextsem:
    se = u36(FREE(sems + s));
    sty = se >> 30;
    sval = se & MASK18;
    if (tr) {
        fprintf(imp_out(), " [%lo] = ", (long)s);
        pr_oct(12, se);
    }
    switch (sty) {
    case 0:
        if (nt0)
            goto type1;                 /* the second type 0 is a type 1 */
        nt0 = 1;
        if (npo)
            goto nxsem;
        scratch = snarg(s);
        /* try to make sure the stack does not move while DOSEM runs */
        setstk(many, 0, 0, 0);
        stpop(scratch - 1);
        goto nxsem;
    case 1:
    type1:
        if (npo == 0) {
            j = FREE(pstak);
            k = stack(0);
            if (j == k)
                goto typex;
            if (j > k && j <= k + 3 * scratch) {
                FREES(k, FREE(j));
                FREES(k + 1, FREE(j + 1));
                FREES(k + 2, FREE(j + 2));
                goto typex;
            }
            if (j == 0) {
                FREES(k + 2, 0);
                goto typex;
            }
            fprintf(imp_out(), "\n%lo=%lo, ", (long)ppo, (long)j);
            imp_error(2, "BAD RESULT OF SEMANTICS; IGNORED **");
            goto typex;
        }
        s = npo;
        npo = nt0 = 0;
        goto nextsem;
    case 2:
        fadd(&pstak, &npstak, stack(sval));
        goto nxsem;
    case 3:
        if (sval == 0)
            imp_error(0, "ATTEMPT TO EXECUTE UNDEFINED SEMANTIC ROUTINE.");
        sn = 07777 & (se >> 18);
        npstak -= sn;
        if (!(sn <= MARGS && npstak >= 0))
            imp_error(0, "** TOO MANY ARGS TO SEMANTIC RTN OR PARAM STACK "
                "UNDERFLOW **");
        if (sn) {
            jx = pstak + npstak + sn;
            for (i = sn - 1; i >= 0; i--)
                pars[i] = FREE(--jx);
        }
        j = semcall(sval, pars);
        fadd(&pstak, &npstak, j);
        goto nxsem;
    case 4:
    case 5:
    case 010:
        sval = se & 007777777777L;
        if (sval & 004000000000L)
            sval = sx36(sval | 0770000000000L);
        if (sty == 4) {
            fadd(&pstak, &npstak, -sval);
            goto nxsem;
        }
        if (sty == 010) {
            fadd(&casep, &ncasep, -sval);
            goto nxsem;
        }
        j = 1;
        if (sval > 0 && sval > 0777777) {
            /* a local symbol: use PARS, as semantic routines and "STUFF"
             * are never mixed */
            i = (sval >> 18) - 1;
            if (i > MARGS) {
                i = 1;
                imp_error(1, " TOO MANY LOCAL SYMBOLS IN REFERENCED SEMANTICS");
            }
            if ((sval &= 0777777) != 0) {
                xname = sval;
                sval = newname(gname(xname));
                pars[i] = sval;
            }
            sval = pars[i];
        }
        if (sval < 0) {
            j = 0;
            sval = -sval;
        }
        codent(sx36(j | (sval << 18)), 0, 0, 0);
        goto nxsem;
    case 6:
    case 7:
        j = stack(sval);
        i = FREE(j);
        if ((i & 077) == 0 && FREE(j + 1)) {
            /* value semantics: replace the original production number */
            i = FREE(j + 1);
            FREES(j, sx36(0777777000000L & i));
            FREES(j + 1, 0);
        }
        codent(FREE(j), FREE(j + 1), FREE(j + 2), sty - 6);
        goto nxsem;
    case 011:
        npstak--;
        j = FREE(pstak + npstak);
        k = FREE(pstak + npstak - 1);
        FREES(pstak + npstak - 1, sx36(j + k));
        goto nxsem;
    case 012:
        j = stack(0);
        FREES(j, sx36(sval << 18));
        FREES(j + 1, sx36((po << 18) | (07777 & (se >> 18))));
        FREES(j + 2, 0);
        FREES(pstak, j);
        cfudge();
        goto nxsem;
    default:
        imp_error(0, "BAD TYPE IN SEMANTICS");
        goto typex;
    }
nxsem:
    if (tr) {
        fprintf(imp_out(), "=>STK[%ld]=", (long)npstak);
        j = FREE(pstak + npstak);
        if (j >= stack(0) && j <= stack(0) + 3 * scratch)
            pstack(stack(0), j - stack(0));
        else
            fprintf(imp_out(), "%ld\n", (long)j);
    }
    s++;
    goto nextsem;
typex:
    if (tr)
        fputc('\n', imp_out());
    npstak = 0;
    return 0;
}

void
semper(void)
{
    sempr1(sems, prosem, nsems);
}

/* for impglue.c: the C setsem registers PAR and STACK itself (dosem_imp.c has the IMP module's) */
void
dosem_dsems(void)
{
}
