/*
 * graph.c -- the syntax graph and its connectivity matrices.
 * Ports decus/graph.imc: GRAPH, GMATR, GGOAL, GNEXT, GALT, GCONN, GMADE,
 * GNT2T, GNT2TX, GLEAP, GENTRY, GRREQ, GNT2B, NEWNO, MAKENO, GMARK, GPOUT1,
 * GPOUT2, GRAPHF and the node accessors SCR, ALT, GTYPE, GVAL, GNB (which
 * impsyn.imc expands in line).
 *
 * The graph is a free array, GA, of two word nodes; a node is named by the
 * index of its first word (1, 3, 5, ...) and "node number" n is the node at
 * index 2n-1, the number the bit matrices use for rows.
 *
 *   word 0   HL: ALT (alternate, the down link)   HR: SCR (successor)
 *   word 1   TYPE bits 48-50, NB bits 32-47, ID bits 0-31
 *
 * where ID is the directory index of the symbol (the production number for
 * boxed production nodes) and NB is the row of the node in NBITM.  The
 * original packs these into 36 bits: TYPE at 33-35, NB at 18-32, ID at 0-17.
 * Node types: 0 nonterminal, 1 terminal, 2 circled nonterminal, 3 boxed
 * production, 5 on the chain of terminal entry points.
 *
 * Nonterminal names carry a leading '#' (RSYN's SYNT puts it there), which
 * is how NEWNO tells them from terminals.
 *
 * BITM(n,nt):  node n can lead to nonterminal nt.
 * NBITM(r,nt): from the node whose NB is r, nt is reached through circled or
 *              boxed nodes only.
 */
#include <time.h>
#include "imp.h"

#define TY_SHIFT 48
#define NB_SHIFT 32
#define NB_MASK  0xFFFFL
#define ID_MASK  0xFFFFFFFFL

word ga, ng, nprods;
static word nodes, nnodes;          /* NODES, NNODES: terminal reference chains */
static word ntgent, nbitn;
static word bitm[3] = { 0, 625, 0 };
static word nbitm[3] = { 0, 150, 0 };
static word gept;                   /* GEPT: chain of terminal entry points */
static word ggoal1;
static word making, makent;         /* the circled node and nonterminal number
                                       of the production being entered */
static word col;                    /* COL: set by GMATR for GNT2B */
static int init;
void (*gprint_hook)(void);

#define NODEWORD(n) FREE(ga + (n) + 1)

static void
gorgfix(void)
{
}

word scr(word n) { return HR(FREE(ga + n)); }
word alt(word n) { return HL(FREE(ga + n)); }
word gtype(word n) { return (word)(((uword)NODEWORD(n) >> TY_SHIFT) & 7); }
word gval(word n) { return NODEWORD(n) & ID_MASK; }
word gnb(word n) { return (NODEWORD(n) >> NB_SHIFT) & NB_MASK; }
word gentry(void) { return gept; }

static void
setscr(word sv, word vv)
{
    FREES(ga + sv, HW(alt(sv), vv));
}

static void
setalt(word sv, word vv)
{
    FREES(ga + sv, HW(vv, scr(sv)));
}

long
imp_msec(void)
{
    return (long)(clock() * 1000 / CLOCKS_PER_SEC);
}

static word
nodeword(word ty, word nb, word id)
{
    return (word)(((uword)ty << TY_SHIFT) | ((uword)nb << NB_SHIFT)
        | ((uword)id & ID_MASK));
}

/* GMARK(NO): node no leads to the nonterminal being produced. */
static void
gmark(word no)
{
    word im = 1 + no / NS;

    bits(bitm, im, makent);
    bitor_(bitm, im, 1 + making / NS);
}

/* MAKENO: append a node; mw1 is its pointer word. */
static word
makeno(word mw1, word mty, word mno, word mid)
{
    word mw2 = nodeword(mty, mno, mid);
    word mg = ng;

    fadd(&ga, &ng, mw1);
    fadd(&ga, &ng, mw2);
    if (mty != 2)
        gmark(mg);
    else
        bits(bitm, 1 + mg / NS, dprop(DP_NODE, mid));
    return mg;
}

/*
 * NEWNO(DS,RTS,TYP,ID): a new node with down successor ds, right successor
 * rts, type typ and the name id.  For a circled node, the existing one if
 * there is one.  typ 4 means "decide from the name whether it is type 0 or
 * 1"; typ 6, "between 1 and 2".
 */
static word
newno(word ds, word rts, word typ, word id)
{
    word ty = typ, ptrs, n1, nr;

    if ((ty & 5) == 4) {
        nr = dname(id, 0);
        ty &= 3;
        if ((nr & 0774000000000L) != ((word)'#' << 29))
            ty = 1;
    }
    ptrs = HW(ds, rts);
    if ((ty & 1) == 0) {                /* a nonterminal: has a circled node? */
        n1 = dprop(DP_ENTRY, id);
        if (n1 == 0) {
            dprops(DP_NODE, id, ntgent);
            ntgent++;
            n1 = makeno(ty == 0 ? 0 : ptrs, 2, nbitn, id);
            nbitn++;
            dprops(DP_ENTRY, id, n1);
        }
        if (ty == 0)
            n1 = makeno(ptrs, ty, 0, id);
        return n1;
    }
    return makeno(ptrs, ty, 0, id);
}

/*
 * GRAPH(SYNT,NSYNT,PN): enter a production.  synt is a free array of nsynt+1
 * directory indices: synt[0] the nonterminal produced, synt[1..nsynt] the
 * symbols of the production.  pn is its number, or 0.  Returns the number of
 * the production it is now known by.
 */
word
graph(word synt, word nsynt, word pnq)
{
    word pnr = pnq, j, jj, k, kk, sq, ent, hook, hookn, nhook, nn, row, nodeno;

    nprods = pnr;
    if (!init) {
        init = 1;
        frelot(&ga, "GRAPH", 1300, gorgfix);
        fincset(ga, 50);
        FREES(ga + 2, (word)((uword)7 << TY_SHIFT));
        ng = ntgent = nnodes = nbitn = 1;
        frelot(&nodes, "NODES", 100, NULL);
        gept = 0;
    }
    making = newno(0, 0, 2, FREE(synt));
    makent = dprop(DP_NODE, FREE(synt));

    /* look for an entry point (circled node) for the first symbol */
    ent = 0;
    hook = dprop(DP_ENTRY, FREE(synt + 1));
    hookn = 1;
    if (hook) {
        j = gtype(hook);
        if (j == 2)
            ent = hook;
    }
    if (hook == 0) {
        /* none: the whole production is entered, and the new node becomes
         * the entry point and the place to hook the production on */
        hook = newno(0, 0, 6, FREE(synt + 1));
        j = gtype(hook);
        if (j == 1)
            gept = newno(gept, hook, 5, 0);
        else {
            gmark(hook);
            ent = hook;
        }
        dprops(DP_ENTRY, FREE(synt + 1), hook);
        goto makeit;
    }
    /* can the hook-on point be moved further down the production? */
chook:
    gmark(hook);
    if (hookn == nsynt)
        goto checkit;
    nhook = scr(hook);
lhook:
    if (nhook == 0)
        goto makeit;
    sq = gval(nhook);
    j = FREE(synt + hookn + 1);
    if (j != sq) {
l3:     nhook = alt(nhook);
        goto lhook;
    }
    j = gtype(nhook);
    if (j > 1)
        goto l3;
    hook = nhook;
    hookn++;
    goto chook;

    /* is this production a duplicate? */
checkit:
    j = scr(hook);
ck1:
    if (j == 0)
        goto makeit;
    jj = gtype(j);
    if (jj == 2)
        kk = gval(j);
    else if (jj == 3)
        kk = gval(scr(j));
    else
        goto ck2;
    k = FREE(synt);
    if (kk == k) {
        imp_error(2, "PRODUCTION DUPLICATES ONE ALREADY IN SYNTAX **");
        if (jj == 2) {                  /* it needs a production number */
            setscr(hook, newno(j, 0, 3, pnr));
            goto graphex;
        }
        nprods = gval(j);
        if (nprods == 1) {
            FREES(ga + j + 1, nodeword(3, 0, pnr));
            nprods = pnr;
        }
        goto graphex;
    }
ck2:
    j = alt(j);
    goto ck1;

    /* make a string of nodes: synt[0], the production number, and synt[hookn+1
     * ..nsynt], and hook it on as the successor of the hook */
makeit:
    if (hookn == nsynt && pnr == 0 && scr(hook))    /* no alt to a type 2 node */
        pnr = 1;
    j = making;
    if (pnr)
        j = newno(0, j, 3, pnr);
    if (hookn < nsynt) {
        for (k = nsynt; k >= hookn + 1; k--)
            j = newno(0, j, 4, FREE(synt + k));
    }
    setalt(j, scr(hook));
    setscr(hook, j);
    /* if the first node of the production was circled, every node that
     * reaches it now reaches what it reaches */
    if (ent) {
        word c = dprop(DP_NODE, gval(ent));

        row = 1 + ent / NS;
        nodeno = 1 + ng / NS;
        for (nn = 1; nn <= nodeno; nn++) {
            if (bit(bitm, nn, c)) {
                (void)bit(bitm, nn, makent);
                bitor_(bitm, nn, row);
            }
        }
    }
graphex:
    if (finam5 & SW_GTRACE) {
        FILE *o = imp_out();
        word i;

        fprintf(o, "PRODUCTION ENTERED IN GRAPH: ");
        pname(FREE(synt));
        fprintf(o, " [%lo] ", (long)nprods);
        for (i = 1; i <= nsynt; i++)
            pname(FREE(synt + i));
        fputc('\n', o);
    }
    return nprods;
}

void
gprint(void)
{
    if (gprint_hook != NULL)
        gprint_hook();
    else
        imp_error(2, "GPRIN1 DEBUGGING PGM NOT PRESENT");
}

/* GCONN(Q,QGOAL): does node q lead to nonterminal qgoal? */
word
gconn(word q, word qgoal)
{
    word qv = 0;

    if (q)
        qv = bit(bitm, 1 + q / NS, qgoal);
    return qv;
}

/* GMADE(C,GO): at circled node c, have we made nonterminal go? */
word
gmade(word q, word qgoal)
{
    if (nbitm[2] == 0)
        return 1 & (word)((uword)FREE(nbitm[0] + gnb(q)) >> qgoal);
    return bit(nbitm, gnb(q), qgoal);
}

/* GGOAL(NT): a node for the goal nonterminal, pointing at itself. */
word
ggoal(word nt)
{
    ggoal1 = newno(0, 0, 0, 0);
    setscr(ggoal1, ggoal1);
    bits(bitm, 1 + ggoal1 / NS, dprop(DP_NODE, nt));
    FREES(ga + ggoal1 + 1, nt);
    return ggoal1;
}

/* GALT(Q,QGOAL): the next alternate of q that can reach qgoal. */
word
galt(word q, word qgoal)
{
    word qq = q;

    for (;;) {
        qq = alt(qq);
        if (qq == 0 || bit(bitm, 1 + qq / NS, qgoal) != 0)
            return qq;
    }
}

/* GNT2T(NT,NI,GL,NO): the next node where a terminal is required right
 * after the circled node of nt, working down chain *ni in NODES. */
word
gnt2t(word nt, word *ni, word gl)
{
    word j, no;

    for (;;) {
        if (*ni == 0)
            return 0;
        j = FREE(nodes + *ni);
        no = HR(j);
        *ni = HL(j);
        if (bit(bitm, 1 + no / NS, gl) == 0)
            continue;
        if (gleap(nt, no) == 0)
            continue;
        return no;
    }
}

/* GNT2TX: GNT2T, speeded up when both nonterminals fit a word. */
word
gnt2tx(word nt, word *ni, word gl)
{
    uword msk1 = gl < 64 ? (uword)1 << gl : 0;
    uword msk2 = nt < 64 ? (uword)1 << nt : 0;
    word j, no;

    if (msk1 == 0 || msk2 == 0)
        return gnt2t(nt, ni, gl);
    for (;;) {
        if (*ni == 0)
            return 0;
        j = FREE(nodes + *ni);
        no = HR(j);
        *ni = HL(j);
        if ((msk1 & (uword)FREE((bitm[0] + (no >> 1)) + 1)) == 0)
            continue;
        if ((msk2 & (uword)FREE(nbitm[0] + gnb(no))) == 0)
            continue;
        return no;
    }
}

/* GLEAP(NT,NO): does terminal node no immediately follow nt's circled node? */
word
gleap(word nt, word no)
{
    return bit(nbitm, gnb(no), nt);
}

/* GNEXT(Q,QGOAL): q, or its first alternate, that leads to qgoal. */
word
gnext(word q, word qgoal)
{
    word q0 = q;

    if (q0 == 0)
        return 0;
    if (bitm[2] == 0) {
        uword mask = qgoal < 64 ? (uword)1 << qgoal : 0;

        while (q0 && (mask & (uword)FREE((bitm[0] + (q0 >> 1)) + 1)) == 0)
            q0 = alt(q0);
    } else if (gconn(q0, qgoal) == 0)
        q0 = galt(q0, qgoal);
    return q0;
}

/* GNT2B(ND): give node nd a row in NBITM if it has none; set bit
 * (row, COL). */
static void
gnt2b(word nd)
{
    word nda = FREE(ga + nd + 1), non = (nda >> NB_SHIFT) & NB_MASK;

    if (non == 0) {
        non = nbitn;
        nbitn++;
        FREES(ga + nd + 1, nda | (non << NB_SHIFT));
    }
    bits(nbitm, non, col);
}

/* GRREQ(TN,NNR,ND): record that terminal tn is required at node nnr. */
static void
grreq(word tn, word nnr, word nd)
{
    word s, ss;

    gnt2b(nd);
    s = ss = dprop(DP_NODE, tn);
    while (s) {
        s = FREE(nodes + s);
        if (HR(s) == nnr)
            return;
        s = HL(s);
    }
    dprops(DP_NODE, tn, nnodes);
    fadd(&nodes, &nnodes, HW(ss, nnr));
}

/*
 * GMATR(TFL): rebuild NBITM, the NODES chains, and the NB row numbers, by
 * walking the graph from each circled node.  A negative TFL only collects the
 * total time spent here so far, in ms, and restarts the count; it is what the
 * driver prints under /T-style statistics.
 */
word
gmatr(word tfl)
{
    static long tim, tin;
    word nn, j, no, noda, noty, noid, ntnr, k, row, nd;
    long now;

    tin = imp_msec();
    if (tfl < 0)
        goto gmtrx;
    bitz(nbitm);
    nnodes = 1;
    for (nn = 1; nn <= ng; nn += NS) {
        j = FREE(ga + nn + 1);
        if (((j >> TY_SHIFT) & 7) == 1)
            dprops(DP_NODE, j & ID_MASK, 0);
    }
    for (nn = 1; nn <= ng; nn += NS) {
        if (nn == ggoal1)
            continue;
        j = FREE(ga + nn + 1);
        if (((j >> TY_SHIFT) & 7) != 2)
            continue;
        pushmi(-1);
        col = dprop(DP_NODE, j & ID_MASK);
        row = (j >> NB_SHIFT) & NB_MASK;
        bits(nbitm, row, col);
        bits(bitm, 1 + nn / NS, col);
        no = scr(nn);
        for (;;) {
            if (no == 0) {
                no = pullyu(0);
                if (no == -1)
                    break;
alterm:
                no = alt(no);
                continue;
            }
            noda = FREE(ga + no + 1);
            noty = (noda >> TY_SHIFT) & 7;
            noid = noda & ID_MASK;
            if (noty == 2) {
                bits(nbitm, row, dprop(DP_NODE, noid));
                if (no == nn)
                    goto alterm;
            }
            if (noty == 1) {
                grreq(noid, no, no);
                goto alterm;
            }
            if (noty == 0) {
                ntnr = dprop(DP_NODE, noid);
                gnt2b(no);
                for (k = gept; k; k = alt(k)) {
                    nd = scr(k);
                    if (bit(bitm, 1 + nd / NS, ntnr))
                        grreq(gval(nd), no, nd);
                }
                goto alterm;
            }
            pushmi(no);
            no = scr(no);
        }
    }
gmtrx:
    now = imp_msec();
    tim += now - tin;
    tin = tim;
    if (tfl < 0)
        tim = 0;
    return tin;
}

/* GRAPHF: release the graph's arrays. */
void
graphf(void)
{
    fallot(ga, 0);
    fallot(nodes, 0);
}

/*
 * GPOUT1(CNODE,NNODE,PUT): put out the production numbers along the path from
 * circled node cnode to nnode, which consists only of type 2 and 3 nodes, onto
 * parse put.  nnode is a node requiring a terminal that follows a circled node.
 */
void
gpout1(word cnode, word nnode, word put)
{
    uword bitrow = (uword)FREE(nbitm[0] + gnb(nnode));
    word k = scr(cnode), val, l, ntn;
    uword i;

    while (k != nnode) {
        switch (gtype(k)) {
        case 2:
            val = 0;
            l = k;
            break;
        case 3:
            val = gval(k);
            l = scr(k);
            break;
        default:
            goto next;
        }
        ntn = dprop(DP_NODE, gval(l));
        i = ntn < 64 ? (uword)1 << ntn : 0;
        if (i == 0) {
            if (!gleap(ntn, nnode))
                goto next;
        } else if ((bitrow & i) == 0)
            goto next;
        k = scr(l);
        if (val)
            pout(put, val);
        continue;
next:
        k = alt(k);
        if (k == 0)
            return;
    }
}

/*
 * GPOUT2(CNODE,GOAL,PUT): likewise from cnode to the circled node of
 * nonterminal number goal.
 */
void
gpout2(word cnode, word goal, word put)
{
    word ntn = dprop(DP_NODE, gval(cnode)), k, l, vl;
    uword mask;

    if (ntn == goal)
        return;
    mask = goal < 64 ? (uword)1 << goal : 0;
    k = scr(cnode);
    for (;;) {
        switch (gtype(k)) {
        case 2:
            vl = 0;
            l = k;
            break;
        case 3:
            vl = gval(k);
            l = scr(k);
            break;
        default:
            goto next;
        }
        if (nbitm[2]) {
            if (!gmade(l, goal))
                goto next;
        } else if ((mask & (uword)FREE(nbitm[0] + gnb(l))) == 0)
            goto next;
        k = scr(l);
        if (vl)
            pout(put, vl);
        ntn = dprop(DP_NODE, gval(l));
        if (ntn == goal)
            return;
        continue;
next:
        k = alt(k);
        if (k == 0)
            return;
    }
}
