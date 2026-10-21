/*
 * rsyn.c -- the bootstrap reader for the initial IMP syntax.  Ports
 * decus/rsyn.imc: RSYN and the routines that enter syntax statements into
 * the syntax graph and the semantics array (SYNT, SYTRM, SYTERM, ADDCHAR,
 * SYNTS, SEMANTICS, FORMARG, SYNARG, DEFCASE, CASE, QUOSEM, EQUOSE, SEMP,
 * GETARG, SEMR, SYNTAX, SEMBIT, REPVAL, VALU, RSERR).
 *
 * RSYN reads, with LEX alone, the part of the file SYNTAX up to the first
 * %%: the syntax statements
 *
 *      <CLASS> ::= elements ::= semantics ;
 *
 * of a tiny notation (a class, its elements which are <CLASS,ARG> or
 * symbols or NAM" and SYM", and the semantics, a call of one routine or an
 * N-ary routine with names for arguments, optionally with the 'ELSE'
 * extensions that syntax.imc itself then defines).  It makes the graph,
 * and for each production its semantics in SEMS and its place in the code
 * tree, exactly as the full compiler does once the syntax for syntax is in.
 *
 * The routines here are semantic routines too: the bootstrapped syntax
 * invokes them (as SYNT, SEMR, ...) from the productions that read the rest
 * of syntax.imc, so they are registered by name.
 *
 * Notes on the original, which this follows:
 *  - The production under construction is in free array SN, the argument
 *    names (0 for a symbol that has none, -1 for NAM#/SYM#) in NA.  A
 *    nonterminal in SN has a '#' prefixed to its name (ADDCHAR).
 *  - SEMK is the index in SEMS of the semantics being built; SEMN is the
 *    production number, which is SEMS's index of its first semantics;
 *    PRODN is the number in the tree, SEMN except for the cases of a class.
 *  - RST is a stack of calls whose arguments are still being read.
 */
#include <string.h>
#include "imp.h"

static word rst, nrst, nspars, na, sn, nsn, nna, snit, snitt, semn, semk, prodn,
    nsargs, cas, ty, cl, smit;
static word semi, lbr, rbr, pct, eqs, comma, quo, lpar, rpar, colon, nam, namm,
    sym, symm;

word
synarg(word i)
{
    return FREE(na + i);
}

/* Name of directory entry i, all of it (up to 50 characters + a '#'). */
static void
fullname(word i, char *buf, size_t n)
{
    word j, w;
    size_t k = 0;
    int c;

    for (j = 0;; j++) {
        w = dname(i, j);
        for (c = 0; c < 5; c++) {
            char ch = (char)((w >> (29 - 7 * c)) & 0x7F);

            if (ch == '\0' || k + 1 >= n)
                goto done;
            buf[k++] = ch;
        }
    }
done:
    buf[k] = '\0';
}

/* ADDCHAR(S,C): the directory index of the name of s with c put in front. */
static word
addchar(word s, char c)
{
    char buf[80];

    buf[0] = c;
    fullname(s, buf + 1, sizeof buf - 1);
    if (strlen(buf) > 50) {
        imp_error(1, "IN ADDCHAR - NAME > 50 CHARACTERS **");
        buf[50] = '\0';
    }
    return dir(buf);
}

/* RSERR(N): an error in the initial syntax: show where, and stop. */
static void
rserr(int n)
{
    (void)n;
    pline1(cl);
    imp_msg("?ERROR IN INITIAL SYNTAX");
    imp_error(0, NULL);
}

/*
 * SYTERM(S): if the symbol at stack entry s is a single quoted character
 * ('X), the quote is removed.
 */
static word
syterm(word s)
{
    word r = FREE(s), q = L18(r);
    char buf[16];

    fullname(q, buf, sizeof buf);
    if (buf[0] == '\'' && buf[1] != '\0' && buf[2] == '\0') {
        q = dir(buf + 1);
        FREES(s, W18(q, R18(r)));
    }
    return s;
}

/*
 * SYNT(A,B): add a syntax element to the definition being built: the
 * nonterminal named at stack entry a, with argument name at stack entry b (0
 * if none, -1 if a is a terminal).  The first of a production is the
 * nonterminal produced.
 */
word
synt(word a, word b)
{
    word q = b, r;

    if (!snit) {
        frelot(&sn, "SYNTX", 25, NULL);
        snit = 1;
        snitt = 0;
    }
    if (q > 0) {
        q = L18(FREE(syterm(b)));
    }
    r = L18(FREE(a));
    if (b >= 0) {
        if (snitt == 0) {                       /* start a production */
            nsn = semn = smit = 0;
            nna = -1;
            snitt = 1;
        }
        r = addchar(r, '#');
        if (nna >= 0)
            fadd(&na, &nna, q);
        else
            nna = 0;
    }
    fadd(&sn, &nsn, r);
    return a;
}

word
sytrm(word a)
{
    return synt(syterm(a), -1);
}

/* SYNTS(N): add the terminal NAM# or SYM# (n is minus the directory index of
 * NAM or SYM). */
word
synts(word n)
{
    char buf[16];

    fullname(-n, buf, sizeof buf - 1);
    strcat(buf, "#");
    fadd(&sn, &nsn, dir(buf));
    fadd(&na, &nna, -1);
    return 0;
}

/* SEMANTICS(): the semantics being read is complete: enter it in the tree. */
word
semantics(void)
{
    word i;

    semfx1(semk, na, nna);
    if (flen(rst) <= nna + 1)
        faddex(&rst, nna + 1);
    for (i = 0; nna && i <= nna - 1; i++)
        FREES(rst + i, -FREE(na + i));
    FREES(rst + nna, semn);
    FREES(rst + nna + 1, 0);
    entree(rst, semn, semk, prodn);
    semk = setsem(0, nna << 18);
    return 0;
}

word
defcase(word n)
{
    if (cas)
        imp_error(1, "NESTED CASE DEFINITIONS ILLEGAL **");
    cas = L18(FREE(n));
    if (semk == semn)
        semk = setsem(0, nna << 18);
    prodn = semk;
    if (dprop(DP_SEM, cas))
        imp_error(1, "CASE DEFINED TWICE OR SAME NAME AS SEMANTIC ROUTINE **");
    dprops(DP_SEM, cas, prodn);
    return 0;
}

/* GETARG(A): a SEMS entry for the identifier, constant or argument at stack
 * entry a: ty is its type (4 a constant, 2 an argument), the value returned
 * its value (the argument number, or minus the constant in 30 bits). */
static word
getarg(word a)
{
    word q = L18(FREE(a)), j, r;

    ty = 4;
    j = convc(q);
    if (j) {
        q = 007777777777L & -convc_const;
        return q;
    }
    for (r = 0; nna && r <= nna - 1; r++) {
        j = FREE(na + r);
        if (j == q) {
            q = r;
            ty = 2;
            break;
        }
    }
    return q;
}

/*
 * CASE(A,F): the semantics of a `CASE (list) OF name` production: the class
 * name has the numbers of the list; with F, a new production is started
 * for the class's own semantics.
 */
word
docase_(word a, word f)
{
    word s, k, j;

    if (f) {
        cas = 0;
        prodn = semn;
        semk = setsem(0, nna << 18);
    }
    s = enstack(0);
    while (getlist(s)) {
        k = getarg(s);
        if (ty == 2)
            imp_error(1, "CANT USE NONTERMINALS IN CASE ARG LIST");
        setsem(010, k);
    }
    j = FREE(a);
    docase(L18(j), semk, semn);
    entrca(semn);
    semk = setsem(0, nna << 18);
    return a;
}

/* QUOSEM(A): enter a conditional quoted production in the code tree, then
 * start the next production of the same semantics. */
word
quosem(word a)
{
    word tb = entree(FREE(a + 1), semn, semk, prodn);

    semfix(semk, tb);
    semk = setsem(0, nna << 18);
    fallot(FREE(a + 1), 0);
    return a;
}

/* EQUOSE(A): enter the semantics for quoted semantics (a's parser output)
 * in SEMS, turning the local names into numbered local symbols. */
word
equose(word a)
{
    word t = enstack(0), i = 0, n, j, k, nn, jj;

    while (getlist(t)) {
        i++;
        n = FREE(t);
        n = -L18(n);
        j = FREE(a + 1);
        nn = 0;
        while ((k = FREE(j))) {
            if (k == n) {
                if (nn == 0) {
                    nn = 1;
                    FREES(j, -((-n) | (i << 18)));
                } else
                    FREES(j, -(i << 18));
            }
            j++;
        }
    }
    j = FREE(a + 1) - 1;
    for (;;) {
        j++;
        k = FREE(j);
        if (k == 0)
            break;
        if (k < 0 && nna) {
            for (i = 0; i <= nna - 1; i++) {
                jj = FREE(na + i);
                if (jj == k) {
                    setsem(7, i);
                    goto l18;
                }
                if (jj == -k) {
                    setsem(6, i);
                    FREES(na + i, k);
                    goto l18;
                }
            }
        }
        setsem(5, 007777777777L & -k);
l18:
        ;
    }
    for (i = 0; nna && i <= nna - 1; i++) {
        jj = FREE(na + i);
        if (jj < 0)
            FREES(na + i, -jj);
    }
    fallot(FREE(a + 1), 0);
    return a;
}

/* SEMP(A,B): a parameter (b = 0) or the end of a parameter list (b = 1). */
word
semp(word a, word b)
{
    word j, q;

    if (b) {
        nrst--;
        j = FREE(rst + nrst);
        setsem(3, (R18(j)) | (nsargs << 18));
        nsargs = 1 + L18(j);
        return 0;
    }
    nsargs++;
    q = getarg(syterm(a));
    setsem(ty, q);
    return 0;
}

/* SEMR(A,B): a semantic routine call: with parameters to follow (b = 1), or a
 * plain call; or (a = 0) the operation b. */
word
semr(word a, word b)
{
    word q;

    if (a == 0) {
        setsem(b, 0);
        nsargs--;
        return a;
    }
    q = dprop(DP_SEM, L18(FREE(a)));
    if (q == 0) {
        pname(L18(FREE(a)));
        fputc(' ', imp_out());
        imp_error(1, "UNDEFINED SEMANTIC ROUTINE");
    }
    if (b) {
        fadd(&rst, &nrst, q | (nsargs << 18));
        nsargs = 0;
    } else {
        setsem(3, q);
        nsargs++;
    }
    return a;
}

/*
 * SYNTAX(N): the syntax of a statement is complete.  n = 0: there is no
 * semantics; 1: semantics follow; 2: called by SQUOTE for quoted semantics.
 * Enters the production in the graph.
 */
word
syntax(word n)
{
    if (n == 2) {
        if (semk == semn)
            semk = setsem(0, nna << 18);
        return 0;
    }
    semn = 0;
    if (n | (nna - 1)) {
        semn = prodn = semk;
        resetsem(nna, semk);
        if (n == 0)
            semantics();
    }
    prodn = semn = graph(sn, nsn - 1, semn);
    gmatr(0);
    snitt = 0;
    return 0;
}

/* SEMBIT(A): set the priority bits of a production. */
word
sembit(word a)
{
    word j = FREE(a);

    convc(L18(j));
    semor(ls36(convc_const, 27), semn);
    return 0;
}

/* REPVAL(S,A): the VAL/ modifier of quoted semantics. */
word
repval(word s, word a)
{
    word j = L18(FREE(s)), k, i;

    j = dprop(DP_SEM, j);
    k = FREE(a + 1);
    do {
        i = FREE(k);
        if (i > 0 && cksval(R18(i), j))
            FREES(k, sx36(j | (i << 18)));
        k++;
    } while (i);
    return a;
}

/* VALU(N,S): a VALUE semantics: production s gets value n. */
word
valu(word n, word s)
{
    word k = L18(FREE(s)), jj, j;

    jj = j = dprop(DP_SEM, k);
    if (jj == 0) {
        j = semk + 2;
        dprops(DP_SEM, k, j);
    }
    k = L18(FREE(n));
    if (convc(k) == 0)
        imp_error(1, "VALUE NOT A CONSTANT **");
    setsem(012, 07777777777L & (j | (convc_const << 18)));
    if (jj == 0)
        setsem(0, nna << 18);
    return 0;
}

static word
stx(word *j)
{
    cl = lex(j);
    return setstk(1, sx36(1 | (*j << 18)), 0, 0);
}

static word w_defcase(const word *p) { return defcase(p[0]); }
static word w_quosem(const word *p) { return quosem(p[0]); }
static word w_equose(const word *p) { return equose(p[0]); }
static word w_sembit(const word *p) { return sembit(p[0]); }
static word w_sytrm(const word *p) { return sytrm(p[0]); }
static word w_synts(const word *p) { return synts(p[0]); }
static word w_semantics(const word *p) { (void)p; return semantics(); }
static word w_syntax(const word *p) { return syntax(p[0]); }
static word w_semr(const word *p) { return semr(p[0], p[1]); }
static word w_semp(const word *p) { return semp(p[0], p[1]); }
static word w_synt(const word *p) { return synt(p[0], p[1]); }
static word w_enstack(const word *p) { return enstack(p[0]); }
static word w_valu(const word *p) { return valu(p[0], p[1]); }
static word w_repval(const word *p) { return repval(p[0], p[1]); }
static word w_case(const word *p) { return docase_(p[0], p[1]); }

/*
 * RSYN: read the initial syntax from the file opened with LEXIN: up to the
 * first %%.
 */
void
rsyn(void)
{
    word j = 0, jj = 0, k;

    stinit();
    frelot(&rst, "RSYN", 23, NULL);
    nrst = nspars = 0;
    frelot(&na, "RSYN2", 12, NULL);
    semi = dir(";");
    lbr = dir("<");
    rbr = dir(">");
    pct = dir("%");
    eqs = dir("=");
    comma = dir(",");
    quo = dir("\"");
    lpar = dir("(");
    rpar = dir(")");
    colon = dir(":");
    semk = setsem(0, 0);
    nam = dir("NAM");
    namm = dir("NAM#");
    sym = dir("SYM");
    symm = dir("SYM#");
    dsem("CASE", w_case);
    dsem("DEFCASE", w_defcase);
    dsem("QUOSEM", w_quosem);
    dsem("EQUOSE", w_equose);
    dsem("SEMBIT", w_sembit);
    dsem("SYTRM", w_sytrm);
    dsem("SYNTS", w_synts);
    dsem("SEMANTICS", w_semantics);
    dsem("SYNTAX", w_syntax);
    dsem("SEMR", w_semr);
    dsem("SEMP", w_semp);
    dsem("SYNT", w_synt);
    dsem("ENSTACK", w_enstack);
    dsem("VALU", w_valu);
    dsem("REPVAL", w_repval);
    (void)namm;
    (void)symm;
    (void)smit;
    (void)nspars;

next:
    stinit();
    cl = lex(&j);
    if (j != lbr)
        rserr(1);
    synt(stx(&j), 0);
    cl = lex(&j);
    if (j != rbr)
        rserr(2);
    cl = lex(&j);
    if (j != colon)
        rserr(3);
    cl = lex(&j);
    if (j != colon)
        rserr(4);
    cl = lex(&j);
    if (j != eqs)
        rserr(5);
nsymb:
    cl = lex(&j);
gnsymb:
    if (j == lbr) {                             /* <CLASS,ARG> */
        cl = lex(&j);
        k = setstk(2, sx36(1 | (j << 18)), 0, 0);
        cl = lex(&j);
        if (j != comma)
            rserr(6);
        synt(k, stx(&j));
        cl = lex(&j);
        if (j == rbr)
            goto nsymb;
        rserr(7);
    }
    if (j == semi) {
        syntax(0);
        goto next;
    }
    if (j == pct) {
        syntax(0);
        goto quit;
    }
    if (j != colon) {
        cl = lex(&jj);
        if (jj == quo) {                        /* NAM" or SYM" */
            if (j == nam || j == sym) {
                synts(-j);
                goto nsymb;
            }
        }
        sytrm(setstk(1, sx36(1 | (j << 18)), 0, 0));
        j = jj;
        goto gnsymb;
    }
    /* have the syntax: do the semantics */
    syntax(1);
    cl = lex(&j);
    if (j != colon)
        rserr(8);
    cl = lex(&j);
    if (j != eqs)
        rserr(9);
    cl = lex(&j);
    jj = setstk(1, sx36(1 | (j << 18)), 0, 0);
    cl = lex(&j);
    if (j != lpar)
        rserr(10);
    cl = lex(&j);
    if (j == rpar) {
        semr(jj, 0);
        goto l10;
    }
    semr(jj, 1);
l14:
    semp(setstk(1, sx36(1 | (j << 18)), 0, 0), 0);
    cl = lex(&j);
    if (j == comma) {
        cl = lex(&j);
        goto l14;
    }
    if (j != rpar)
        rserr(11);
    semp(0, 1);
l10:
    cl = lex(&j);
    if (j != semi && j != pct)
        rserr(12);
    semantics();
    if (j == semi)
        goto next;
quit:
    cl = lex(&j);
    if (j != pct)
        rserr(13);
}

/* for impglue.c: the C rsyn registers its routines itself (rsyn_imp.c has the IMP module's) */
void
rsyn_dsems(void)
{
}
