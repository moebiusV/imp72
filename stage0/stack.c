/*
 * stack.c -- the semantic stack, a pushdown stack, and bit matrices.
 * Ports decus/stack.imc (STACK, STPOP, STPUSH, SETSTK, STINIT; PUSHMI,
 * PULLYU; BITS, BIT, BITZ, BITOR, BSTATS).
 *
 * The semantic stack has three word entries, in free array 'STACK'; its
 * routines return the free storage index of the entry's first word.
 *
 * A bit matrix is a descriptor of three words (see imp.h) and one relocatable
 * free array of (rows * (a[2]+1)) words, stored column of words by column of
 * words: bit (i,j) is bit j%64 of word a[0] + i + a[1]*(j/64).  The original
 * packs 36 bits per word.
 */
#include "imp.h"

word st;                    /* ST: the STACK array */
word stp;                   /* STP: offset of the top entry (-3 when empty) */
static int init;            /* INIT */

word
stack(word n)
{
    return st + stp + 3 * n;
}

word
stinit(void)
{
    if (!init) {
        init = 1;
        st = fallot_new("STACK", 110);
    }
    stp = -3;
    return st;
}

word
stpop(word n)
{
    stp = stp - 3 * n;
    return stp;
}

word
stpush(word a, word b, word c)
{
    word stn, stx;

    stp = stp + 3;
    stn = stp + 3;
    if (flen(st) <= stn)
        faddex(&st, stn);
    stx = st + stp;
    FREES(stx, a);
    FREES(stx + 1, b);
    FREES(stx + 2, c);
    return st + stp;
}

word
setstk(word n, word a, word b, word c)
{
    word stn = stp + 3 * n;
    word s = stn;

    fadd(&st, &stn, a);
    fadd(&st, &stn, b);
    fadd(&st, &stn, c);
    return st + s;
}

/* ---- PUSHMI-PULLYU ----------------------------------------------------- */

static int pnit;
static word pdstack, stackn;

void
pushmi(word v)
{
    if (!pnit) {
        pnit = 1;
        stackn = 0;
        pdstack = fallot_new("PSHMI", 20);
    }
    fadd(&pdstack, &stackn, v);
}

word
pullyu(word sw)
{
    word val;

    if (!pnit) {
        imp_error(0, "** PUSHDOWN STACK UNDERFLOW ERROR");
        return 0;
    }
    val = FREE(pdstack + stackn - 1);
    if (sw == 0) {
        stackn = stackn - 1;
        if (stackn < 0)
            imp_error(0, "** PUSHDOWN STACK UNDERFLOW ERROR");
    }
    return val;
}

/* ---- BITS: the bit matrix package ------------------------------------- */

#define WORDBITS 64

static long qbits, qbit, qbo;

/* BITS(A,I,J): set bit (i,j), growing the matrix to hold it. */
void
bits(word a[3], word i, word j)
{
    word col, s, e, k, l, nw, q, ds, row;

    qbits++;
    col = j / WORDBITS;
    if (a[0] == 0) {
        if (i >= a[1]) {
            a[1] = 10;
            if (a[1] <= i)
                a[1] = i + 1;
        }
        if (col > a[2])
            a[2] = col;
        frelot(&a[0], "BITS", a[1] * (a[2] + 1), NULL);
    }
    if (col > a[2]) {
        a[0] = fallot(a[0], a[1] * (col + 1));
        s = a[0] + a[1] * (a[2] + 1);
        e = a[0] + a[1] * (col + 1) - 1;
        for (k = s; k <= e; k++)
            FREES(k, 0);
        a[2] = col;
    }
    if (i >= a[1]) {
        nw = i + 5;
        a[0] = fallot(a[0], nw * (a[2] + 1));
        for (k = a[2]; k >= 0; k--) {
            row = a[0] + a[1] * k;
            ds = a[0] + nw * k;
            for (l = nw - 1; l >= a[1]; l--)
                FREES(ds + l, 0);
            for (l = a[1] - 1; l >= 0; l--) {
                q = FREE(row + l);
                FREES(ds + l, q);
            }
        }
        a[1] = nw;
    }
    s = a[0] + i + a[1] * col;
    FREES(s, (word)((uword)FREE(s) | ((uword)1 << (j - WORDBITS * col))));
}

/* BIT(A,I,J): nonzero iff bit (i,j) is set.  The BIT of impsyn.imc, which is
 * what the compiler uses, does not check its indices and reads whatever lies
 * beyond the matrix; the checking version in stack.imc is commented out.
 * Here out-of-range bits read as zero, as in that version. */
word
bit(const word a[3], word i, word j)
{
    word col = j / WORDBITS;

    qbit++;
    if (i < 0 || i >= a[1] || col > a[2])
        return 0;
    return (word)((uword)FREE(a[0] + i + a[1] * col)
        & ((uword)1 << (j - WORDBITS * col)));
}

/* BITZ(A): zero the matrix. */
void
bitz(word a[3])
{
    word k, l;

    if (a[0] != 0) {
        l = a[1] * (a[2] + 1) - 1;
        for (k = 0; k <= l; k++)
            FREES(a[0] + k, 0);
    }
}

/* BITOR(A,I,J): OR row j into row i. */
void
bitor_(word a[3], word i, word j)
{
    word s = a[0] + j, d = a[0] + i, e;

    qbo++;
    for (e = 0; e <= a[2]; e++) {
        FREES(d, (word)((uword)FREE(s) | (uword)FREE(d)));
        d += a[1];
        s += a[1];
    }
}

/* BSTATS(I): print the call counts; `reset` clears them. */
void
bstats(int reset)
{
    if (reset)
        fprintf(stderr, "%ld CALLS TO BITS, %ld CALLS TO BIT, "
            "%ld CALLS TO BITOR.\n", qbits, qbit, qbo);
    qbo = qbit = qbits = 0;
}
