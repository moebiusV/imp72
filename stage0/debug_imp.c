/*
 * debug_imp.c -- debug.c's interface, over src/debug.imp (TPRIN1, SEMPR1, GPRIN1, PSTAC1, PARTR1) compiled by imp72stage0:
 * they print with PRINT, through the same line buffer as the other IMP modules.   The tables the
 * print-outs read are shared COMMON variables (src/shared.txt); the arguments of the original's calls that are
 * those tables are therefore not passed.
 */
#include "imp.h"

extern word impl_helpd(word *), impl_tprin1(word *), impl_sempr1(word *), impl_gprin1(word *), impl_pstac1(word *, word *),
    impl_partr1(word *, word *, word *, word *, word *, word *, word *);

void
helpd(void)
{
    word z = 0;

    impl_helpd(&z);
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
