/*
 * imp72real.h -- a REAL word and the bits of the IEEE 754 double it holds: the word is the double's own bits, in the programs of the seed
 * (imp72stage0) as in those of imp72.
 */
#ifndef IMP72REAL_H
#define IMP72REAL_H
#include <stdint.h>

static inline uint64_t real_bits(int64_t w) { return (uint64_t)w; }
static inline int64_t real_word(uint64_t bits) { return (int64_t)bits; }
#endif
