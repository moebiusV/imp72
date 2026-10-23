/*
 * sem.c -- the semantic routines of the seed (the compiler whose code generator is codegen.c): the lists of decus/impsem.imc (NEWLIST,
 * ENLIST, GETLIST), ASSEMC, and the registration of the routines.  The table itself is semtab.c.  (The compiler with the code generator of
 * src/pccir has these in src/pccir/impsem.imp.)
 */
#include <stdlib.h>
#include <string.h>
#include "imp.h"

/* ---- lists (impsem.imc) ------------------------------------------------- */

static word lists, liste, listb, blist;
static int linit;

static void
listinit(void)
{
    if (!linit) {
        linit = 1;
        frelot(&lists, "LISTS", 50, NULL);
        listb = liste = 0;
    }
}

/* NEWLIST(S): open a new list, with the stack entry at s on it if s. */
word
newlist(word s)
{
    listinit();
    if (flen(lists) <= liste + 3)
        faddex(&lists, liste + 3);
    FREES(lists + liste, listb);
    FREES(lists + liste + 1, liste);
    FREES(lists + liste + 2, blist);
    blist = listb = liste = liste + 3;
    if (s)
        enlist(s);
    return s;
}

word
enlist(word s)
{
    listinit();
    fadd(&lists, &liste, FREE(s));
    fadd(&lists, &liste, FREE(s + 1));
    fadd(&lists, &liste, FREE(s + 2));
    return 0;
}

/* GETLIST(S): the next entry of the current list into the stack at s; 0 when
 * it is empty, which also closes it and reopens the previous. */
word
getlist(word s)
{
    word j;

    listinit();
    if ((j = liste - blist) != 0) {
        FREES(s, FREE(lists + blist));
        FREES(s + 1, FREE(lists + blist + 1));
        FREES(s + 2, FREE(lists + blist + 2));
        blist += 3;
    } else {
        blist = FREE(lists + listb - 1);
        liste = FREE(lists + listb - 2);
        listb = FREE(lists + listb - 3);
    }
    return j / 3;
}

/*
 * ASSEMC(A): the end of a compilation unit; a is the stack entry whose code
 * is the whole program.  The original assembles it (assemb) after freeing the
 * parser's and the graph's arrays; here the code is handed to the translator
 * (trans.c), and listed first if /A.
 */
word
assemc(word a)
{
    if (anyco()) {
        subtmpf();
        if (finam5 & SW_ASSEMBLY) {
            fprintf(imp_out(), "\n** CODE PRODUCED BY PROGRAM\n");
            pcode(a);
        }
        trans_unit(a);
    }
    return 0;
}

static word w_newlist(const word *p) { return newlist(p[0]); }
static word w_enlist(const word *p) { return enlist(p[0]); }
static word w_assemc(const word *p) { return assemc(p[0]); }

/* sem_init: register the routines. */
void
sem_init(void)
{
    static const char *const unported[] = {
        /* impsem.imc */
        "ERROR",
        /* code2.imc, code3.imc */

        NULL
    };
    int i;

    for (i = 0; unported[i] != NULL; i++)
        dsem(unported[i], NULL);
    dsem("NEWLIST", w_newlist);
    dsem("ENLIST", w_enlist);
    dsem("ASSEMC", w_assemc);
    impsem_init();
    codegen_init();
}
