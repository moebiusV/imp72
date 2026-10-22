/*
 * dosem_imp.c -- dosem.c's interface, over src/dosem.imp compiled by imp72stage0: all of decus/dosem.imc but PAR, the
 * entry for STACK, and SEMPER, which stay here, with the by-value entry points.  Ports decus/dosem.imc:
 * DOSEM, SEMFIX, SEMFX1, DOCASE, ENPROD, MATCHP, NOTEPROD, PLIES, SNODEN,
 * ENSTACK, RESETSEM, SETSEM, PAR, SEMPER, SNARG, SEMGET, SEMED, SEMOR, CKSVAL.
 *
 * The semantics of a production is a run of words in free array SEMS (36 bit
 * words; V = bits 17-0, N = bits 29-18, TYPE = bits 35-30):
 *
 *   0  start of semantics with N arguments; bits 29-27 are a priority for
 *      immediate execution; V is the production's entry in PROSEM
 *   1  end of semantics (so is a second type 0)
 *   2  push the stack pointer of parameter V on PSTAK
 *   3  call semantic routine V with the bottom N entries of PSTAK (at most
 *      10) and push the result
 *   4  push the value -V as the next parameter
 *   5  enter a node with value V at the next position of the code stack
 *   6  enter parameter V of the stack in the next position
 *   7  copy parameter V of the stack to the next position
 *   10 load a constant on the stack of parameters for 'CASE'
 *   11 add the last two stack elements
 *   12 put a type 0 node for [V], value N, on the stack
 *
 * PROSEM records, for each production, where it can match in the code tree;
 * see ENPROD below.  Its words, like those of SEMS, keep the original
 * layout.
 *
 * Semantic routines are C functions called through the table of sem.c; the
 * original jumps to the routine's address.
 */
#include <stdint.h>
#include <string.h>
#include "imp.h"

extern word fb;                         /* the base of free storage, for IMP */
extern word casep;
extern word impl_dosem(word *), impl_setsem(word *, word *), impl_resetsem(word *, word *),
    impl_semfix(word *, word *), impl_semfx1(word *, word *, word *),
    impl_docase(word *, word *, word *), impl_enprod(word *, word *, word *),
    impl_matchp(word *, word *), impl_noteprod(word *, word *), impl_plies(word *),
    impl_snoden(word *), impl_enstack(word *), impl_snarg(word *), impl_semget(word *),
    impl_semed(word *), impl_semor(word *, word *), impl_cksval(word *, word *);

static void
sync(void)
{
    fb = (word)(intptr_t)fmem;
}

word
par(word i)
{
    return FREE(casep + i);
}

static word
w_par(const word *p)
{
    return par(p[0]);
}

static word
w_stack(const word *p)
{
    return stack(p[0]);
}

/* the semantic routines that SETSEM enters in the table of DSEM (called through impglue.c) */
void
dosem_dsems(void)
{
    dsem("PAR", w_par);
    dsem("STACK", w_stack);
}

word dosem(word ppo) { word r; sync(); r = impl_dosem(&ppo); sync(); return r; }
word setsem(word ty, word val) { word r; sync(); r = impl_setsem(&ty, &val); sync(); return r; }
void resetsem(word val, word a) { sync(); impl_resetsem(&val, &a); }
word semfix(word i, word a) { word r; sync(); r = impl_semfix(&i, &a); sync(); return r; }
word semfx1(word i, word a, word na) { word r; sync(); r = impl_semfx1(&i, &a, &na); sync(); return r; }
void docase(word id, word semn, word prodn) { sync(); impl_docase(&id, &semn, &prodn); sync(); }
void enprod(word pn, word no, word sem) { sync(); impl_enprod(&pn, &no, &sem); sync(); }
word matchp(word pn, word n) { sync(); return impl_matchp(&pn, &n); }
void noteprod(word pn, word n) { sync(); impl_noteprod(&pn, &n); sync(); }
word plies(word i) { sync(); return impl_plies(&i); }
word snoden(word i) { sync(); return impl_snoden(&i); }
word enstack(word v) { word r; sync(); r = impl_enstack(&v); sync(); return r; }
word snarg(word n) { sync(); return impl_snarg(&n); }
word semget(word n) { sync(); return impl_semget(&n); }
word semed(word a) { sync(); return impl_semed(&a); }
void semor(word a, word n) { sync(); impl_semor(&a, &n); sync(); }
word cksval(word v, word i) { sync(); return impl_cksval(&v, &i); }

void
semper(void)
{
    sempr1(sems, prosem, nsems);
}
