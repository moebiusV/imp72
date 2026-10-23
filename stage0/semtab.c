/*
 * semtab.c -- the table of semantic routines (a table of C function pointers: dsem registers a routine by name, semcall calls it).
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
    if (getenv("IMP_SLOG")) {
        word r = rtn[id - 1].fn(pars);

        fprintf(stderr, "semcall %s %ld %ld %ld %ld %ld -> %ld\n", rtn[id - 1].name, (long)pars[0],
            (long)pars[1], (long)pars[2], (long)pars[3], (long)pars[4], (long)r);
        return r;
    }
    return rtn[id - 1].fn(pars);
}

