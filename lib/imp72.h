/* imp72.h -- for C code that shares data with IMP72 programs (docs/FFI.md). */
#ifndef IMP72_H
#define IMP72_H
#include <stdint.h>
#include <string.h>

/*
 * A REAL variable of an IMP72 program holds an IEEE 754 binary64, in a word that is the double's own bits (imp72 and the seed, imp72stage0, alike).
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
