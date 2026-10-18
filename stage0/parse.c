/*
 * parse.c -- the Irons fast multi-track parser, with Bilofsky's error
 * correction and disambiguation.  Ports decus/parse.imc: PARSE, SYMBOL,
 * PFORCE, COPY, POUT, PSTATS (and PARSEF).  ERCOR, AMBIG and ISNAME are in
 * ercor.c.
 *
 * PARSE reads symbols with LEX and follows every parse that is still
 * possible at once.  A parse is a run of words in free array P (the current
 * symbol's parses) or PP (the next symbol's):
 *
 *   word 0    HL: head of this parse's output chain in O    HR: offset COFF
 *                                                            of its last pair
 *   word 1    the current graph node
 *   word 2..  pairs: HL the node that invoked a goal, HR the goal's
 *             nonterminal number -- the stack of goals being parsed
 *
 * Output (production numbers, and minus the directory index of a name or
 * symbol matched as NAM# or SYM#) is chained in free array O, newest first:
 *
 *   entry     HL: the value (a 32 bit two's complement number)
 *             HR: index of the next entry, 0 ends the chain
 *
 * (the original's comment has the halves the other way round; the code is as
 * here).  Whenever one parse remains its output is reversed with the
 * pushdown stack and fed to COTREE.
 *
 * Differences: COTREE is a hook, parse_cotree, until the semantic stack
 * reducer is ported; PARSE returns when the goal has been reached rather than
 * with a timing statistic; the timing counters count milliseconds of process
 * time.
 */
#include <string.h>
#include "imp.h"

void (*parse_cotree)(word k);

static int init;
static word p, pp, o;                   /* P, PP, O */
static word np, npp, no;                /* next free word of each */
static word pn, ppn, opn, nop;          /* parses in P, PP; last round's */
static word ip, pi, coff, nxip;         /* the parse being followed */
static word cgoal, cnode, cval, gole;
static int gotit;                       /* the goal has been reached */
static word nsymbs, symbs;              /* symbols forced into the input */
static int knit;
static word ol;                         /* column of the last token */
static word tr;                         /* trace on */
static word s[3];                       /* S: the symbol, SYM#, NAM# */
static word name;                       /* ISNAME of the symbol */

static long psts, pspn, pspnt, psps, pspst, psos, amtime, pstt, srtt;

#define STATSW 0243130160L              /* switches that ask for timing */

/* SYMBOL(SS): the next input symbol: a forced one, else from LEX. */
static void
symbol(word *ss)
{
    if (nsymbs) {
        nsymbs--;
        *ss = FREE(symbs + nsymbs);
    } else
        ol = lex(ss);
}

/* PFORCE(SY): push a symbol back, to be read before the rest of the input. */
void
pforce(word sy)
{
    if (!knit) {
        knit = 1;
        nsymbs = 0;
        frelot(&symbs, "P/SYM", 20, NULL);
    }
    fadd(&symbs, &nsymbs, sy);
}

/*
 * COPY(QQ,NQQ,QQN): copy the parse being followed (P+IP) to the end of array
 * qq, which has *nqq words and *qqn parses.  Returns the index of the copy.
 */
static word
copy(word *qq, word *nqq, word *qqn)
{
    word copyv;

    (*qqn)++;
    copyv = *nqq;
    *nqq = *nqq + coff + 1;
    if (flen(*qq) < *nqq)
        faddex(qq, *nqq);
    copyv += *qq;
    memmove(freep(copyv), freep(p + ip), (size_t)(coff + 1) * sizeof(word));
    return copyv;
}

/* POUT(QQ,VV): put output vv on the chain of the parse at index qq. */
word
pout(word qq, word vv)
{
    word q;

    if (tr) {
        FILE *f = imp_out();

        fprintf(f, "      ");
        if (vv < 0)
            pname(-vv);
        else
            fprintf(f, "[%lo]", (long)vv);
        fprintf(f, " ADDED TO OUTPUT.\n");
    }
    q = HL(FREE(qq));                           /* old chain head */
    FREES(qq, HW(no, HR(FREE(qq))));            /* HRL([KQ],NO) */
    fadd(&o, &no, HW(vv, q));                   /* Q OR VV LS 18 */
    return no;
}

/*
 * PARSE(GOAL): parse the input as the nonterminal named goal ('#PG').  The
 * outputs go to parse_cotree as they become certain.
 */
word
parse(const char *goal)
{
    word gol, i, j, k, l, ll, ngoal, ent, ntn, tnode, nnode;
    int flush_timing;

    tr = (finam5 & SW_PTRACE) != 0;
    psts = pspn = pspnt = psps = pspst = psos = amtime = 0;
    pstt = -imp_msec();
    srtt = 0;
    if (!init) {
        init = 1;
        p = fallot_new("P/P", 400);
        pp = fallot_new("P/PP", 150);
        frelot(&o, "P/OUT", 100, NULL);
    }
    no = 1;
    pn = opn = 1;
    gotit = 0;
    gol = dir(goal);
    gole = dprop(DP_NODE, gol);
    j = ggoal(gol);
    FREES(p, 2);
    FREES(p + 1, j);
    FREES(p + 2, HW(j, gole));
    np = 3;
    s[1] = dir("SYM#");
    s[2] = dir("NAM#");

nexts:
    symbol(&s[0]);
    psts++;
    if (dprop(DP_ISNM, s[0]) - 1 < 0)
        dprops(DP_ISNM, s[0], isname(s[0]) + 1);
    name = isname(s[0]);
    if (tr) {
        FILE *f = imp_out();

        fprintf(f, "\nNEXT INPUT SYMBOL: ");
        pname(s[0]);
        fprintf(f, ", %ld POSSIBLE PARSES.\n", (long)pn);
    }
    ppn = ip = npp = pi = 0;
nextp:
    l = p + ip;
    coff = HR(FREE(l));
    nxip = ip + coff + 1;
    if (tr)
        fprintf(imp_out(), "  PARSE #%ld", (long)(pi + 1));
    cgoal = HR(FREE(l + coff));
    cnode = FREE(l + 1);
nextn:
    cval = gval(cnode);
    if (tr) {
        fprintf(imp_out(), "\n  NODE #%ld TYPE %ld ", (long)cnode,
            (long)gtype(cnode));
        pname(cval);
        fputc('\n', imp_out());
    }
    switch (gtype(cnode)) {
    case 0:
        goto ntwanted;
    case 2:
        goto ntmade;
    case 3:
        goto output;
    default:
        break;                          /* 1; the original falls through too */
    }

    /* a terminal is wanted here */
    for (i = name; i >= 0; i--) {
        if (cval == s[i]) {
            j = copy(&pp, &npp, &ppn);
            k = gnext(scr(cnode), cgoal);
            FREES(j + 1, k);
            if (tr) {
                fprintf(imp_out(), "      MATCH FOR ");
                pname(cval);
                fprintf(imp_out(), " SO NEW PARSE #%ld IS MADE.\n", (long)ppn);
            }
            if (i)
                pout(j, -s[0]);
            goto takealt;
        }
    }
    goto takealt;

    /* a boxed production node: put out its number, and spawn a parse for
     * each alternative that leads to the goal */
output:
    j = cnode;
l1: j = gnext(alt(j), cgoal);
    if (j) {
        k = copy(&p, &np, &pn);
        if (tr)
            fprintf(imp_out(), "      PARSE #%ld SPAWNED FOR ALT NODE.\n",
                (long)pn);
        FREES(k + 1, j);
        goto l1;
    }
    pout(p + ip, cval);
    cnode = scr(cnode);
    goto nextn;

    /* a nonterminal is wanted: start a parse of it for each way the symbol
     * can begin one */
ntwanted:
    ngoal = dprop(DP_NODE, cval);
    for (i = name; i >= 0; i--) {
        ent = dprop(DP_ENTRY, s[i]);
        j = gnext(scr(ent), ngoal);
        if (j) {
            k = copy(&pp, &npp, &ppn);
            if (tr) {
                fprintf(imp_out(), "      ");
                pname(s[i]);
                fprintf(imp_out(), " -> ");
                pname(cval);
                fprintf(imp_out(), " AT NODE #%ld SO NEW PARSE #%ld IS "
                    "FORMED.\n", (long)ent, (long)ppn);
            }
            FREES(k, FREE(k) + 1);
            FREES(k + 1, j);
            if (i)
                pout(k, -s[0]);
            fadd(&pp, &npp, HW(cnode, ngoal));
        }
    }
    goto takealt;

    /* a circled node: the goal has been made */
ntmade:
    ntn = dprop(DP_NODE, cval);
    for (i = name; i >= 0; i--) {
        tnode = dprop(DP_NODE, s[i]);
l2:     if (tnode && (nnode = gnt2tx(ntn, &tnode, cgoal)) != 0) {
            k = copy(&p, &np, &pn);
            if (tr) {
                fprintf(imp_out(), "      \"");
                pname(s[i]);
                fprintf(imp_out(), "\" MAY CONTINUE SO PARSE #%ld IS "
                    "SPAWNED.\n", (long)pn);
            }
            FREES(k + 1, nnode);
            gpout1(cnode, nnode, k);
            goto l2;
        }
    }
    if (ntn == cgoal)
        goto l3;
    if (gmade(cnode, cgoal)) {
        gpout2(cnode, cgoal, p + ip);
l3:     if (cgoal == gole) {
            copy(&pp, &npp, &ppn);
            gotit = 1;
            goto takealt;
        }
        k = copy(&p, &np, &pn);
        l = k;
        FREES(l, FREE(l) - 1);
        np--;
        ll = p + np;
        j = HL(FREE(ll));
        ngoal = HR(FREE(ll - 1));
        j = gnext(scr(j), ngoal);
        FREES(l + 1, j);
        if (tr)
            fprintf(imp_out(), "      GOAL REACHED SO PARSE #%ld IS SPAWNED.\n",
                (long)pn);
    }

takealt:
    cnode = gnext(alt(cnode), cgoal);
    if (cnode) {
        if (tr)
            fprintf(imp_out(), "   ALT NODE TAKEN.");
        goto nextn;
    }
    ip = nxip;
    if (++pi < pn)
        goto nextp;

    /* all the parses for this symbol are followed; disambiguate */
    if (ppn > 1) {
        if (finam5 & SW_STATS)
            amtime -= 1 + imp_msec();
        ambig(pp, &npp, &ppn, o, tr);
        if (finam5 & SW_STATS)
            amtime += imp_msec();
    }
    if (ppn > 1) {
        if (gotit)
            imp_error(1, "SYNTACTIC AMBIGUITY; UNRESOLVED AT % **");
        goto end1sym;
    }
    if (ppn == 0) {                     /* the parse died: correct the error */
        FILE *f = imp_out();

        finam5 |= SW_LIST;
        pline1(ol);
        fputs("** SYNTAX ERROR ", stderr);
        fprintf(f, "%*s\n** SYNTAX ERROR - \"", (int)(ol + 4), "^");
        l = j = ercor(p, opn, s[0]);
        imp_msg("**");
        imp_error(1, NULL);
        fprintf(f, "\" IS REPLACED BY \"");
        while ((k = FREE(j)) != 0) {
            pname(k);
            j++;
            fprintf(f, " ");
        }
        fprintf(f, "\"\n");
        for (i = j - 1; i >= l; i--)
            pforce(FREE(i));
        fallot(l, 0);
        np = nop;
        pn = opn;
        goto nexts;
    }

    /* just one parse: feed out its output */
    j = HL(FREE(pp));
    if (j) {
        pushmi(0);
        while (j) {
            j = FREE(o + j);
            k = (int32_t)HL(j);
            pushmi(k);
            j = HR(j);
        }
        flush_timing = (finam5 & STATSW) != 0;
        if (flush_timing) {
            word t = imp_msec();

            pstt += t;
            srtt -= 1 + t;
        }
        while ((k = pullyu(0)) != 0) {
            if (parse_cotree != NULL)
                parse_cotree(k);
        }
        if (flush_timing) {
            word t = imp_msec();

            pstt -= 1 + t;
            srtt += t;
        }
    }
    if (no > psos)
        psos = no;
    no = 1;
    FREES(pp, HW(0, HR(FREE(pp))));     /* HRL([FORG+PP],0) */

end1sym:
    if (npp > psps)
        psps = npp;
    if (ppn > pspn)
        pspn = ppn;
    if (np > pspst)
        pspst = np;
    if (pn > pspnt)
        pspnt = pn;
    if (!gotit) {                       /* PP becomes P for the next symbol */
        j = p;
        p = pp;
        pp = j;
        pn = opn = ppn;
        nop = np = npp;
        goto nexts;
    }
    pstt += imp_msec();
    return pstt;
}

void
pstats(void)
{
    FILE *f = imp_out();

    fprintf(f, "PARSE TIME %ld MSEC., %ld INPUT SYMBOLS.\n"
        "   MAX. PARSE BRANCHING %ld/%ld\n"
        "   MAX. PARSE SPACE %ld+%ld, MAX. OUTPUT SPACE %ld\n",
        pstt, psts, pspnt, pspn, pspst, psps, psos);
    if (amtime)
        fprintf(f, "AMBIG %ld MS.\n", amtime);
    if (srtt > 0)
        fprintf(f, "SEMANTIC ROUTINES TIME %ld MS.\n", srtt);
}

/* PARSEF: release the parser's arrays. */
void
parsef(void)
{
    fallot(pp, 0);
    fallot(p, 0);
    fallot(o, 0);
    init = 0;
}
