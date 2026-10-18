/*
 * sem.c -- the table of semantic routines, and the few that syntax
 * statements use when they are read.
 *
 * Ports decus/impsem.imc (NEWLIST, ENLIST, GETLIST) and decus/code2.imc
 * (HOOK) and ASSEMC, for the case of stack entries with no code attached,
 * which is all that reading syntax statements needs.  Every other semantic
 * routine of the PDP-10 compiler (code.imc, code2.imc, code3.imc, impsem.imc)
 * is registered by name so that `SEMR` accepts it, but is not ported: calling
 * it stops with an error.  They are replaced when the compiler switches from
 * PDP-10 code to pcc IR.
 *
 * The name of a routine has as its 'SEM' property its id: 1 + its slot here.
 */
#include <stdlib.h>
#include <string.h>
#include "imp.h"

static struct semrtn {
    const char *name;
    semfn fn;
} *rtn;
static word nrtn, crtn;

void
dsem(const char *name, semfn fn)
{
    word d = dir(name), s;

    s = dprop(DP_SEM, d);
    if (s > 0 && s <= nrtn && rtn[s - 1].name != NULL
        && strcmp(rtn[s - 1].name, name) == 0) {
        rtn[s - 1].fn = fn;             /* registered again: replace */
        return;
    }
    if (nrtn == crtn) {
        struct semrtn *t = realloc(rtn, (size_t)(crtn += 64) * sizeof *t);

        if (t == NULL) {
            imp_error(0, "OUT OF MEMORY IN DSEM. ");
            return;
        }
        rtn = t;
    }
    rtn[nrtn].name = name;
    rtn[nrtn].fn = fn;
    nrtn++;
    dprops(DP_SEM, d, nrtn);
}

word
semcall(word id, const word *pars)
{
    if (id < 1 || id > nrtn) {
        imp_error(0, "ATTEMPT TO EXECUTE UNDEFINED SEMANTIC ROUTINE.");
        return 0;
    }
    if (rtn[id - 1].fn == NULL) {
        char msg[96];

        snprintf(msg, sizeof msg, "SEMANTIC ROUTINE %s IS NOT YET PORTED",
            rtn[id - 1].name);
        imp_error(0, msg);
        return 0;
    }
    return rtn[id - 1].fn(pars);
}

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
        "LETSYN", "FCON", "ERROR",
        /* code2.imc, code3.imc */
        "PRINCAL", "PRINPAR", "BYTEP",
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
