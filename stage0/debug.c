/*
 * debug.c -- diagnostic print-outs.  Ports decus/debug.imc: HELPD, GPRIN1,
 * PARTR1.  (TPRIN1, SEMPR1 and PSTAC1 are added with the semantic modules
 * whose tables they print; PCODE1 lists PDP-10 code and has no counterpart.)
 *
 * The original is linked in only if wanted, and a request for a dump without
 * it is trapped with an advisory.  Here it is always linked.
 *
 * The dumps are meant to be compared with the output of the original
 * compiler.  Where the original prints the raw 36 bit contents of a table
 * word, so does this, provided the table keeps the original layout; the
 * tables that do not (the graph's) are printed field by field, as GPRIN1 does.
 * The one place the 64 bit word shows is the bit matrix NBITM, which GPRIN1
 * prints as the masks the BIT macro returns: bit j of a word, 1 LS (j mod 36)
 * there, and so here, to keep the numbers the same.
 */
#include "imp.h"

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

/* The value the BIT macro gives for bit j: the mask in a 36 bit word. */
static word
bitmask(const word a[3], word i, word j)
{
    return bit(a, i, j) ? (word)1 << (j % 36) : 0;
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
            pr_oct(0, id);
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
        if ((dname(n, 0) >> 29) == '#')
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
        pr_oct(0, cval);
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
                pr_oct(0, k);
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
