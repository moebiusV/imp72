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
    semifn ifn;                         /* or a routine of IMP */
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
    rtn[nrtn].ifn = NULL;
    nrtn++;
    dprops(DP_SEM, d, nrtn);
}

void
dsemi(const char *name, semifn fn)
{
    dsem(name, NULL);
    rtn[dprop(DP_SEM, dir(name)) - 1].ifn = fn;
}

word
semcall(word id, const word *pars)
{
    if (id < 1 || id > nrtn) {
        imp_error(0, "ATTEMPT TO EXECUTE UNDEFINED SEMANTIC ROUTINE.");
        return 0;
    }
    if (rtn[id - 1].ifn != NULL) {              /* IMP passes by reference: a copy of the parameters, for the routine may change them */
        word a[5], r;

        memcpy(a, pars, sizeof a);
        r = rtn[id - 1].ifn(&a[0], &a[1], &a[2], &a[3], &a[4]);
        if (getenv("IMP_SLOG"))
            fprintf(stderr, "semcall %s %ld %ld %ld %ld %ld -> %ld\n", rtn[id - 1].name, (long)pars[0], (long)pars[1],
                (long)pars[2], (long)pars[3], (long)pars[4], (long)r);
        return r;
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


/* DOSEM calls the semantic routine by its id, with the parameters by reference (the original jumps to an address it patches into a JSA) */
word
impsemcall(word *id, word *pars)
{
    return semcall(*id, pars);
}

/* for the image: the names of the routines in the order of their ids, and the table made again in the order of an image's (the ids are in its tables) */
word dsem_count(void) { return nrtn; }
const char *dsem_name(word i) { return rtn[i].name; }

int
dsem_remap(char **names, word n)
{
    struct semrtn *t = calloc((size_t)n + 1, sizeof *t);
    word i, j;

    for (i = 0; i < n; i++) {
        for (j = 0; j < nrtn; j++)
            if (rtn[j].name != NULL && strcmp(rtn[j].name, names[i]) == 0)
                break;
        if (j == nrtn) {
            fprintf(stderr, "imp72: the image uses the routine %s, which this run has not entered\n", names[i]);
            free(t);
            return 0;
        }
        t[i] = rtn[j];
    }
    free(rtn);
    rtn = t;
    nrtn = crtn = n;
    return 1;
}
