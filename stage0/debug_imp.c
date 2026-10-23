/*
 * debug_imp.c -- debug.c's interface, over src/debug.imp (TPRIN1, SEMPR1, GPRIN1, PSTAC1, PARTR1) compiled by imp72stage0:
 * they print with PRINT, through the same line buffer as the other IMP modules.  HELPD stays here.  The tables the
 * print-outs read are shared COMMON variables (src/shared.txt); the arguments of the original's calls that are
 * those tables are therefore not passed.
 */
#include "imp.h"

extern word impl_tprin1(word *), impl_sempr1(word *), impl_gprin1(word *), impl_pstac1(word *, word *),
    impl_partr1(word *, word *, word *, word *, word *, word *, word *);

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

void
gprin1(void)
{
    word z = 0;

    impl_gprin1(&z);
}

void
tprin1(void)
{
    word z = 0;

    impl_tprin1(&z);
}

void
sempr1(word sems_, word prosem_, word nsems_)
{
    word z = 0;

    (void)sems_;
    (void)prosem_;
    (void)nsems_;
    impl_sempr1(&z);
}

void
pstac1(word s, word n)
{
    impl_pstac1(&s, &n);
}

void
partr1(word cnode, word ctyp, word cval, word p, word ip, word o, word cw)
{
    impl_partr1(&cnode, &ctyp, &cval, &p, &ip, &o, &cw);
}
