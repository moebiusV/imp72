/*
 * entree_imp.c -- entree.c's interface, over src/entree.imp compiled by imp72stage0.
 * The IMP module has ENTREE, ENTMARK, INSNO, NUNO, COMPNO, CODEID, MATCH, ENTRCA and ENTRC1 of
 * decus/entree.imc; here are the by-value entry points, and the tree accessors TSCR, TALT, TNODEN
 * that the C side uses, and TPRINT.  See entree.c for the node formats.  Ports decus/entree.imc: ENTREE, ENTMARK, INSNO,
 * NUNO, COMPNO, CODEID, MATCH, ENTRCA, ENTRC1, TPRINT, and the tree accessors
 * TSCR, TALT, TNODEN that impsyn.imc expands in line.
 *
 * The code tree is kept in free array CT, in nodes of two or three words
 * (the node is named by the index of its links word):
 *
 *   word -1   the "special" field: the first production node among this
 *             node's alternates (set by INSNO)
 *   word 0    links: SCR (successor), ALT (alternate), NODEN (node number)
 *   word 1    the stuff word, with the node type in bits 35-33:
 *               2  match the kth item of the pattern (k in the low 18 bits)
 *               3  match identifier id (dir index in the low 18 bits; if bit
 *                  18 is set the id is a one word constant whose value is in
 *                  word 2 of the node, and matching by value is also tried)
 *               1  match a stack item of a kind given by the other bits:
 *
 *      3   2 111 111  5  5  7   8
 *     TTT XI WSV CRN PP QQ KKK LLL
 *
 *     X, I   floating point / integer may match
 *     W      only expressions of one word of code (IMP10)
 *     S, V   any subscripted / simple variable may match
 *     C, R, N  a constant / register / raw identifier might match, the
 *            constant being limited by KKK, LLL, the register by PP, QQ
 *     (as in entree.imc's header comment, where the rest is explained)
 *     With WSVCRN = 0 the node matches productions: bit 19 of the stuff word
 *     says it is a terminal node (MATCHP finds the semantics), bit 18 that it
 *     is a class of VAL/ productions.
 *
 * The stuff words keep their 36 bit format.  The links word does not: the
 * original packs three 12 bit fields in 36 bits (its header comment says 18
 * and 18, which is older); here they are three 20 bit fields (SCR 0-19, ALT
 * 20-39, NODEN 40-59), so the tree is not limited to 4095 words.  Nothing
 * prints that word raw.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "imp.h"

extern word fb;                         /* the base of free storage, for IMP */
extern word impl_entree(word *, word *, word *, word *), impl_match(word *, word *, word *),
    impl_entrca(word *);

static void
sync(void)
{
    fb = (word)(intptr_t)fmem;
}

#define LK_MASK 0xFFFFFL
word tscr(word n) { return FREE(ct + n) & LK_MASK; }
word talt(word n) { return (FREE(ct + n) >> 20) & LK_MASK; }
word tnoden(word n) { return (FREE(ct + n) >> 40) & LK_MASK; }

/* IMP_ELOG: the calls of ENTREE and what they return, on stderr (to compare with entree.c's) */
static int
elogging(void)
{
    static int on = -1;

    if (on < 0)
        on = getenv("IMP_ELOG") != NULL;
    return on;
}

static void
elog_in(word s, word cs_, word sr, word pn)
{
    word i;

    fprintf(stderr, "entree cs=%ld sr=%ld pn=%ld s:", (long)cs_, (long)sr, (long)pn);
    for (i = s; FREE(i); i++)
        fprintf(stderr, " %ld", (long)FREE(i));
}

static void
elog_out(word r)
{
    word i;

    fprintf(stderr, " ->");
    for (i = r; FREE(i) >= 0; i++)
        fprintf(stderr, " %ld", (long)FREE(i));
    fprintf(stderr, " nct=%ld noden=%ld\n", (long)nct, (long)noden);
    if (getenv("IMP_ETREE"))
        for (i = 1; i <= nct; i++)
            fprintf(stderr, "   %ld: %lx\n", (long)i, (long)FREE(ct + i));
}

word
entree(word s, word cs_, word sr, word pn)
{
    word r;

    sync();
    if (elogging())
        elog_in(s, cs_, sr, pn);
    r = impl_entree(&s, &cs_, &sr, &pn);
    sync();
    if (elogging())
        elog_out(r);
    return r;
}

word
match(word *tnp, word sn, word snonr)
{
    word r;

    word tn0 = *tnp;

    sync();
    r = impl_match(tnp, &sn, &snonr);
    if (elogging())
        fprintf(stderr, "match tn=%ld sn=%ld snonr=%ld -> %ld tn=%ld\n", (long)tn0, (long)sn,
            (long)snonr, (long)r, (long)*tnp);
    return r;
}

void
entrca(word semn)
{
    sync();
    impl_entrca(&semn);
    sync();
}

void
tprint(void)
{
    tprin1();
}
