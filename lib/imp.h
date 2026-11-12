/* imp.h -- for C code that shares data with IMP72 programs (docs/FFI.md). */
#ifndef IMP_API_H
#define IMP_API_H
#include <stdint.h>
#include <string.h>

/*
 * The runtime of an IMP72 program, for a C program with a main of its own (docs/FFI.md): the library's own main is these two calls.
 * imp_init(argc, argv) first, so that the IMP routines find the runtime initialised (the command line, for the CCALL impargc(0) and
 * impargvp(0) of an IMP routine); imp_run() where the C program wants the main program of an IMP source (the statements before its
 * first SUBR), which returns the value of its last statement.  A source with no main program has no imp_run: it is not linked
 * unless it is called.  The word an IMP routine returns is 64 bits on every host, so the result is int64_t.
 */
int64_t imp_init(int argc, char **argv);
int64_t imp_run(void);

/*
 * A REAL variable of an IMP72 program holds an IEEE 754 binary64, in a word that is the double's own bits (imp and the seed, impstage0, alike).
 * These two convert between the word and the double, for C code that reads or writes the word of a REAL (a COMMON REAL, an element of a REAL array).
 */
static inline double
imp_real_to_double(int64_t word)
{
    double d;

    memcpy(&d, &word, sizeof d);
    return d;
}

static inline int64_t
imp_double_to_real(double d)
{
    int64_t w;

    memcpy(&w, &d, sizeof w);
    return w;
}

#endif
