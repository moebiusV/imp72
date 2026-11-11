/*
 * debug.c -- diagnostic print-outs.  Ports src/debug.imp (decus/debug.imc): HELPD, GPRIN1,
 * PARTR1, TPRIN1, SEMPR1, PSTAC1.  (PCODE1 lists PDP-10 code and has no counterpart.)
 *
 * The original is linked in only if wanted, and a request for a dump without
 * it is trapped with an advisory.  Here it is always linked.
 *
 * The dumps are those of the stage compiler (src/debug.imp): the 36 bit formats
 * of the tape are gone.  The raw words of the tables are printed in octal as
 * the 64 bit numbers they are (OCT 17 for a word of SEMS, OCT 22 for the stuff
 * word of the code tree), and the bit matrix NBITM is printed as the masks the
 * BIT macro gives with words of 64 bits: bit j of a word is 1 LS (j mod 64).
 */
#include "imp.h"

/* OCT w of the library (lib/impio.imp): the 64 bits as unsigned octal, zero filled to w columns; in a field of more than 12 columns
 * a number of 12 digits or fewer is zero filled to 12 and the rest of the field is blanks (as the field of a 36 bit word was) */
void
pr_oct64(int w, word v)
{
    FILE *o = imp_out();
    unsigned long long u = (unsigned long long)(uword)v;
    char buf[32];
    int len = snprintf(buf, sizeof buf, "%llo", u);

    if (w > 12 && len < 13)
        fprintf(o, "%*s%012llo", w - 12, "", u);
    else if (w > len)
        fprintf(o, "%0*llo", w, u);
    else
        fputs(buf, o);
}

void
helpd(void)
{
    imp_msg(" ");
    imp_msg("COMPILER DEBUGGING SWITCHES:");
    imp_msg("B = ASSEMBLER DEBUGGING PRINTOUT");
    imp_msg("D = TYPE THIS LIST OF DEBUGGING SWITCHES");
    imp_msg("F = PRINT FREE STORAGE MAP WHENEVER ALTERRED");
    imp_msg("G = PRINT GORY DETAILS OF PARSE");
    imp_msg("I = PRINT HASH-CODED DIRECTORY");
    imp_msg("M = PRINT CODE TREE MATCH DETAILS");
    imp_msg("N = PRINT CONNECTIVITY MATRICES WITH GRAPH");
    imp_msg("P = PRINT PARSE OUTPUT AND SEMANTICS EXECUTION");
    imp_msg("S = PRINT SEMANTICS ARRAYS");
    imp_msg("T = PRINT CODE TREE");
    imp_msg("X = PRINT SYNTAX GRAPH");
    imp_msg("Z = GATHER AND PRINT COMPILATION STATISTICS");
}

/* The value debug.imp prints for bit j of NBITM: 1 LS (j mod 64). */
static word
bitmask(const word a[3], word i, word j)
{
    return bit(a, i, j) ? (word)((uword)1 << (j % 64)) : 0;
}

/* One row of a connectivity matrix, as GPRIN1 prints them side by side. */
static void
matrix_row(const word a[3], word i, word ncols, int raw)
{
    word j;

    for (j = 1;; j++) {
        pr_igr(1, raw ? bitmask(a, i, j) : (bit(a, i, j) ? 1 : 0));
        if (5 * (j / 5) == j) {
            pr_stg(1, " ");
        }
        if (j >= ncols)
            break;
    }
    pr_stg(5, " ");
}

/* GPRIN1: the graph following entry of production nprods. */
void
gprin1(void)
{
    word i, n, j, w1, w2, ty, id, nno, k, r;

    pr_stg(0, "HERE IS THE GRAPH FOLLOWING ENTRY OF PRODUCTION NR. ");
    pr_igr(0, nprods);
    pr_nl();
    pr_stg(8, "INDEX");
    pr_stg(8, "SCR");
    pr_stg(8, "ALT");
    pr_stg(8, "TYPE");
    pr_stg(8, "NB");
    pr_stg(8, "NAME");
    pr_stg(8, "NT#");
    pr_stg(8, "ENTRY");
    pr_nl();
    for (i = 1; i <= ng; i += NS) {     /* includes the unused node at ng */
        w1 = FREE(ga + i);
        w2 = FREE(ga + i + 1);
        ty = gtype(i);
        id = gval(i);
        pr_igr(8, i);
        pr_igr(8, HR(w1));
        pr_igr(8, HL(w1));
        pr_igr(8, ty);
        pr_igr(8, gnb(i) & 07777);
        if (ty == 3) {
            pr_stg(2, "[");
            pr_oct64(0, id);
            pr_stg(1, "]");
        } else {
            pr_stg(4, " ");
            pname(id);
            pr_igr(5, dprop(DP_NODE, id));
            pr_igr(5, dprop(DP_ENTRY, id));
        }
        pr_nl();
        (void)w2;
    }
    pr_nl();
    /* the chains of nodes that require each terminal */
    for (n = dseq(-1); n >= 0; n = dseq(n)) {
        if ((dname(n, 0) & 0377) == '#')
            continue;
        j = dprop(DP_NODE, n);
        if (j == 0)
            continue;
        pr_stg(0, "TERMINAL ");
        pname(n);
        do {
            j = FREE(gr_nodes + j);
            pr_igr(4, HR(j));
            j = HL(j);
        } while (j);
        pr_nl();
    }
    if (finam5 & SW_MATRICES) {
        pr_stg(0, "CONNECTIVITY MATRICES BITM AND NBITM.");
        pr_nl();
        pr_nl();
        nno = (2 + ng / NS) / 2;
        for (k = 1; k <= nno; k++) {
            for (r = k;; r = k + nno) {
                pr_stg(0, "NODE#");
                pr_igr(4, r + r - 1);
                pr_stg(2, " ");
                matrix_row(gr_bitm, r, gr_ntgent - 1, 0);
                if (r == k + nno)
                    break;
            }
            pr_nl();
        }
        pr_nl();
        nno = gr_nbitn / 2;
        for (k = 1; k <= nno; k++) {
            for (r = k;; r = k + nno) {
                pr_stg(0, "NBIT#");
                pr_igr(4, r);
                pr_stg(2, " ");
                matrix_row(gr_nbitm, r, gr_ntgent - 1, 1);
                if (r == k + nno)
                    break;
            }
            pr_nl();
        }
    }
    pr_nl();
}

/*
 * PARTR1: trace one step of the parser.  The original lists the output of
 * the parse by indexing free storage with the output entry (`K_FREE(J) ARS
 * 18`, where J has just been loaded with the entry); the entry itself is
 * meant, and is what is printed.
 */
void
partr1(word cnode, word ctyp, word cval, word p, word ip, word o, word cw)
{
    word j, k, n;

    pr_stg(0, "   CURRENT NODE #");
    pr_igr(0, cnode);
    pr_stg(0, ", TYPE ");
    pr_igr(0, ctyp);
    pr_stg(0, ", ");
    if (ctyp == 3) {
        pr_stg(0, "[");
        pr_oct64(0, cval);
        pr_stg(1, "]");
    } else
        pname(cval);
    j = HL(FREE(p + ip));
    if (j) {
        pr_stg(0, ", OUTPUT: ");
        while (j) {
            j = FREE(o + j);
            k = (int32_t)HL(j);
            if (k > 0)
                pr_oct64(0, k);
            else
                pname(-k);
            pr_stg(0, " ");
            j = HR(j);
        }
    }
    pr_nl();
    if (cw > ip + 3) {
        for (k = cw; k >= ip + 4; k--) {
            n = HR(FREE(p + k));
            j = HL(FREE(p + k));
            pr_stg(0, "    ");
            pr_stg(0, k == cw ? "CURRENT " : "        ");
            pr_stg(0, "GOAL: ");
            pname(gval(j));
            pr_stg(0, "=NT NR. ");
            pr_igr(0, n);
            pr_stg(0, " AT NODE #");
            pr_igr(0, j);
            pr_nl();
        }
    }
}

/*
 * TPRIN1: print the code tree and its bit matrix.  Each node: its index, SCR,
 * ALT, the special field, the type of the stuff word, the node number, the
 * stuff word in octal, and a gloss of it.
 *
 * The matrix is printed 75 columns at a time, as src/debug.imp prints it: the
 * number of columns is 36 * ((MXC//36)+1), at least 5 * 36, MXC being the
 * highest column set (and // the remainder in IMP, which makes it a bug of the
 * port that the dumps keep until it is mended there).
 */
void
tprin1(void)
{
    FILE *o = imp_out();
    word j = 0, kvv, kt, kv, n, t, i, k, cols;

    pr_stg(0, "** CODE GENERATION TREE:");
    pr_nl();
    pr_stg(0, "NODE SCR ALT PDN TY NR");
    pr_nl();
    for (;;) {
        j++;
        if (j >= nct)
            break;
        kvv = FREE(ct + j + 1);
        pr_igr(4, j);
        pr_igr(4, tscr(j));
        pr_igr(4, talt(j));
        pr_igr(4, FREE(ct + j - 1));
        pr_igr(3, rs64(kvv, 61));
        pr_igr(3, tnoden(j));
        pr_oct64(22, kvv);
        kt = rs64(kvv, 61);
        j += 2;
        if (kt == 1) {
            if ((kvv & ls64(077, 53)) == 0) {
                if ((kvv & NTRM) == 0) {
                    pr_stg(0, "  [");
                    pr_oct64(0, HR(kvv));
                    pr_stg(0, "]");
                } else {
                    pr_stg(0, "  <");
                    pr_oct64(0, HR(kvv));
                    pr_stg(0, ">");
                }
                pr_nl();
                continue;
            }
            if ((kvv & ls64(1, 59)) == 0)
                pr_stg(0, " FLT ");
            if ((kvv & ls64(1, 60)) == 0)
                pr_stg(0, " IGR ");
            if ((3 & rs64(kvv, 59)) == 3)
                pr_stg(0, "     ");
            pr_stg(0, (kvv & ls64(1, 58)) ? "W" : " ");
            pr_stg(0, (kvv & ls64(1, 57)) ? "S" : " ");
            pr_stg(0, (kvv & ls64(1, 56)) ? "V" : " ");
            pr_stg(0, (kvv & ls64(1, 55)) ? "C" : " ");
            pr_stg(0, (kvv & ls64(1, 54)) ? "R" : " ");
            pr_stg(0, (kvv & ls64(1, 53)) ? "N" : " ");
            pr_stg(0, "  ");
            pr_oct64(2, 037 & rs64(kvv, 24));
            pr_stg(1, " ");
            pr_oct64(2, 037 & rs64(kvv, 16));
            pr_stg(1, " ");
            pr_oct64(3, 0177 & rs64(kvv, 8));
            pr_stg(1, " ");
            pr_oct64(3, 0377 & kvv);
            pr_nl();
            continue;
        }
        if (kt == 3) {
            kv = HR(kvv);
            pr_stg(1, " ");
            pname(kv);
            if (kvv & NCLS) {
                kv = FREE(ct + j);
                j++;
                pr_oct64(22, kv);
            }
            pr_nl();
            continue;
        }
        pr_stg(2, " ");
        pr_oct64(5, HR(kvv));
        pr_nl();
    }
    pr_stg(0, "CODE TREE BIT MATRIX:");
    pr_nl();
    pr_nl();
    {                                   /* the columns the original's matrix would have: 5 words of 36 bits to begin with, more when a bit beyond them is set */
        word mc = 0;

        for (i = 0; i <= noden; i++)
            for (k = 0; k < 64 * (sbitm[2] + 1); k++)
                if (bit(sbitm, i, k) != 0 && k > mc)
                    mc = k;
        cols = mc / 36 + 1;             /* as src/debug.imp: COLS:=(MXC/36)+1 */
    }
    if (cols < 5)
        cols = 5;
    n = 36 * cols;
    j = 0;
    do {
        t = j + 74;
        if (t > n)
            t = n;
        pr_stg(5, " ");                 /* (the tape's IGR 6 has no value and prints nothing) */
        k = j + 4;                      /* (a FOR loop of IMP runs its body once before it tests the end: the last block's header has a number, 829, past its columns) */
        do {
            fprintf(o, "%6ld", (long)k);
            k += 5;
        } while (k <= t);
        pr_nl();
        for (i = 0; i <= noden; i++) {
            pr_igr(3, i);
            pr_stg(2, " ");
            for (k = j; k <= t; k++) {
                if (k % 5 == 0)
                    pr_stg(1, " ");
                pr_igr(1, k < 64 * (sbitm[2] + 1) && bit(sbitm, i, k) != 0 ? 1 : 0);   /* not past the row; the tape's loop ends at N itself, one column past the matrix, and prints what lies after it in free storage (0 here) */
            }
            pr_nl();
        }
        pr_nl();
        j += 75;
    } while (t != n);
}

/*
 * SEMPR1: print the semantics array: each production's header with its
 * plies, snode number and the list of terminal nodes it matches, then the
 * words of SEMS in octal, five to a line.
 */
void
sempr1(word sems_, word prosem_, word nsems_)
{
    word k, j, l, nn = 0, lln, k1, n;

    pr_nl();
    pr_stg(0, "*** SEMANTICS ***");
    pr_nl();
    for (k = 1; k <= nsems_ - 1 || k == 1; k++) {
        j = FREE(sems_ + k);
        if (SEMTYPE(j) == 0) {
            if (nn)
                pr_nl();
            nn = 0;
            pr_stg(1, "[");
            pr_oct64(4, k);
            pr_stg(1, "]  ");
            if ((l = HR(j))) {
                pr_stg(0, " PLIES:");
                pr_oct64(0, FREE(prosem_ + l));
                pr_stg(0, ",#");
                pr_igr(0, FREE(prosem_ + l + 2));
                l = FREE(prosem_ + l + 1);
                lln = 0;
                while (l) {
                    if (++lln > 5) {
                        lln = 1;
                        pr_nl();
                        pr_stg(12, " ");
                    }
                    k1 = FREE(prosem_ + l);
                    pr_stg(0, " <");
                    pr_oct64(0, HL(k1));
                    pr_stg(0, "/");
                    n = FREE(prosem_ + l + 1);
                    if (HL(n)) {
                        pr_oct64(0, HL(n));
                        pr_stg(0, "_");
                    }
                    pr_oct64(0, HR(n));
                    pr_stg(0, ">");
                    l = HR(k1);
                }
                nn = 5;
            }
        }
        if (++nn == 6) {
            nn = 1;
            pr_nl();
            pr_stg(6, " ");
        }
        pr_stg(2, " ");
        pr_oct64(17, j);
    }
    pr_nl();
}

/* PSTAC1(S,N): print the stack entry at offset n of the stack at s. */
void
pstac1(word s, word n)
{
    word ss = FREE(s + n), t;

    pr_stg(0, "STACK NODE ");
    pr_igr(0, n);
    pr_stg(0, ", TYPE ");
    pr_oct64(2, 077 & ss);
    if ((077 & ss) == 0) {
        pr_stg(0, " [");
        pr_oct64(0, HL(ss));
        pr_stg(0, "]");
    } else {
        if ((t = HL(ss))) {
            pr_stg(0, ", NAME ");
            pname(t);
        }
        if (ss & 0100)
            pr_stg(0, ", IGR");
        if (ss & 0200)
            pr_stg(0, ", REAL");
    }
    if ((t = FREE(s + n + 1))) {
        pr_stg(0, ", VALUE ");
        pr_oct64(22, t);
    }
    pr_nl();
}
