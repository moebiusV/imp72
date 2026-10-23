/*
 * imp72real.h -- a REAL word and the bits of the IEEE 754 double it holds.  The code of the original compiler (decus/syntax.imc and the
 * translator trans.c that reads its chains) applies integer comparison and negation to reals, so a REAL word is the sign and
 * magnitude of the double with the magnitude negated for a negative number (order preserving); the code generator of src/ir keeps
 * the double's own bits, and its library (libimp72raw.a) is built with IMP_REAL_RAW.
 */
#ifndef IMP72REAL_H
#define IMP72REAL_H
#include <stdint.h>

#ifdef IMP_REAL_RAW
static inline uint64_t real_bits(int64_t w) { return (uint64_t)w; }
static inline int64_t real_word(uint64_t bits) { return (int64_t)bits; }
#else
static inline uint64_t
real_bits(int64_t w)
{
    int64_t s = w >> 63;                                /* -1 for a negative number */

    return (uint64_t)((w ^ s) - s) | ((uint64_t)s & 0x8000000000000000ULL);
}

static inline int64_t
real_word(uint64_t bits)
{
    int64_t s = (int64_t)bits >> 63;

    return (int64_t)(((bits & 0x7FFFFFFFFFFFFFFFULL) ^ (uint64_t)s) - (uint64_t)s);
}
#endif
#endif
