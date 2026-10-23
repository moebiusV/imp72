/*
 * cotree_imp.c -- cotree.c's interface, over src/cotree.imp compiled by imp72stage0: COTREE is IMP; CFUDGE, CODENT,
 * CKDONE, MAXWELL, MAXEND, SETPRI, SAMEAS and PSTACK stay here (the semantic routines are entered in the
 * table of DSEM by address).  The state they share with COTREE is in common variables of the IMP module.
 * Ports decus/cotree.imc: COTREE, CFUDGE, CODENT, CKDONE, MAXWELL, MAXEND, SETPRI, SAMEAS, PSTACK.
 *
 * COTREE(S) adds one item of parser output to the code stack CS: S is a
 * production number, or minus the directory index of a terminal symbol.  For
 * a production, a match is tried between a path through the code tree and
 * the part of the stack from each position on.  A match copies that part of
 * the stack to the semantic routines' stack and calls DOSEM; the result
 * replaces what matched, and it is repeated until nothing matches.
 * COTREE(0) clears the stack.
 *
 * A stack entry is three words (36 bit):
 *
 *   word 0   HL: DI, usually a directory index  bits 7-6: AR, 1 integer,
 *            2 real, 3 byte  bits 5-0: TY
 *   word 1   VAL
 *   word 2   HL: CB, the start of its code  HR: CE, the end
 *
 * TY: 0 production (DI = its index in SEMS), 1 raw identifier, 2 register,
 * 4 constant, 8 identifier, 16 subscripted variable.  For 2, 8 and 16, VAL is
 * bit 35 the indirect bit, bits 28-18 the register, bits 17-0 the offset.
 *
 * Quoted semantics are collected, by Maxwell's demon (MAXWELL), in a free
 * array of their own, MAXIE, one for each, and MAXEND turns the demon off.
 *
 * The stack's items are kept in 36 bit words; where code is attached to an
 * item (CB, CE) it is the PDP-10 code of the original, which is not ported:
 * the routines that copy or compare code stop with an error.
 */
#include <stdint.h>
#include <string.h>
#include "imp.h"

extern word cs, ncs, entries, prio, demon, maxie, nmaxie, nc;
extern word impl_cotree(word *), impl_ckdone(word *);


/* COTREE(SS): see src/cotree.imp */
word
cotree(word ss)
{
    word r;

    r = impl_cotree(&ss);
    return r;
}


void
ckdone(void)
{
    word z = 0;

    impl_ckdone(&z);
}
