/* imp72.h -- for C code that shares data with IMP72 programs (docs/FFI.md). */
#ifndef IMP72_H
#define IMP72_H
#include <stdint.h>

/*
 * A REAL variable of an IMP72 program holds an IEEE 754 double, in a word that is not the double's own bits: the sign
 * and magnitude of the double with the magnitude negated for a negative number (the PDP-10's way, which makes the
 * integer compares and negation the code generator applies to any value right for reals).  C code that reads or writes
 * the word of a REAL (a COMMON REAL, an element of a REAL array) converts with these.
 */
double imp_real_to_double(int64_t word);
int64_t imp_double_to_real(double d);

#endif
