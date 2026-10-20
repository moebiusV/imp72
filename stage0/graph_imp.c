/*
 * graph_imp.c -- graph.c's interface, over src/graph.imp compiled by imp72stage0.
 * The IMP module has GRAPH, GMATR, GGOAL, GNEXT, GALT, GCONN, GMADE, GNT2T, GNT2TX, GLEAP,
 * GENTRY, GRREQ, GNT2B, NEWNO, MAKENO, GMARK, GPOUT1, GPOUT2 and GRAPHF of decus/graph.imc;
 * what is here is the by-value C interface to them, the node accessors SCR, ALT, GTYPE, GVAL,
 * GNB (which impsyn.imc expands in line), GPRINT and IMP_MSEC.
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
#include <stdint.h>
#include <time.h>
#include "imp.h"

#define TY_SHIFT 48
#define NB_SHIFT 32
#define NB_MASK  0xFFFFL
#define ID_MASK  0xFFFFFFFFL

extern word impl_graph(word *, word *, word *), impl_gmatr(word *), impl_ggoal(word *),
    impl_gentry(void), impl_gconn(word *, word *), impl_gmade(word *, word *),
    impl_galt(word *, word *), impl_gnext(word *, word *), impl_gleap(word *, word *),
    impl_gnt2t(word *, word *, word *, word *), impl_gnt2tx(word *, word *, word *, word *),
    impl_gpout1(word *, word *, word *), impl_gpout2(word *, word *, word *),
    impl_graphf(void);

word fb;                            /* the base of free storage, for IMP (stack_imp.c has it too) */

static void
sync(void)
{
    fb = (word)(intptr_t)fmem;
}

#define NODEWORD(n) FREE(ga + (n) + 1)

word scr(word n) { return HR(FREE(ga + n)); }
word alt(word n) { return HL(FREE(ga + n)); }
word gtype(word n) { return (word)(((uword)NODEWORD(n) >> TY_SHIFT) & 7); }
word gval(word n) { return NODEWORD(n) & ID_MASK; }
word gnb(word n) { return (NODEWORD(n) >> NB_SHIFT) & NB_MASK; }

long
imp_msec(void)
{
    return (long)(clock() * 1000 / CLOCKS_PER_SEC);
}

/* GPRINT: dump the graph (GPRIN1 of debug.c). */
void
gprint(void)
{
    gprin1();
}

word graph(word synt, word nsynt, word pnq) { sync(); return impl_graph(&synt, &nsynt, &pnq); }
word gmatr(word tfl) { sync(); return impl_gmatr(&tfl); }
word ggoal(word nt) { sync(); return impl_ggoal(&nt); }
word gentry(void) { return impl_gentry(); }
word gconn(word q, word qgoal) { sync(); return impl_gconn(&q, &qgoal); }
word gmade(word q, word qgoal) { sync(); return impl_gmade(&q, &qgoal); }
word galt(word q, word qgoal) { sync(); return impl_galt(&q, &qgoal); }
word gnext(word q, word qgoal) { sync(); return impl_gnext(&q, &qgoal); }
word gleap(word nt, word no) { sync(); return impl_gleap(&nt, &no); }

word
gnt2t(word nt, word *ni, word gl)
{
    word no = 0;

    sync();
    return impl_gnt2t(&nt, ni, &gl, &no);
}

word
gnt2tx(word nt, word *ni, word gl)
{
    word no = 0;

    sync();
    return impl_gnt2tx(&nt, ni, &gl, &no);
}

void gpout1(word cnode, word nnode, word put) { sync(); impl_gpout1(&cnode, &nnode, &put); }
void gpout2(word cnode, word goal, word put) { sync(); impl_gpout2(&cnode, &goal, &put); }
void graphf(void) { sync(); impl_graphf(); }
