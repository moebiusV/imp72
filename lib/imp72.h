/* imp72.h -- for C code that shares data with IMP72 programs (docs/FFI.md). */
#ifndef IMP72_H
#define IMP72_H
#include <stdint.h>

/*
 * A REAL variable of an IMP72 program holds an IEEE 754 binary64.  In a program compiled by imp72 (the compiler written in IMP) the word is the double's own
 * bits, and these two are the identity (libimp72raw.a, built with IMP_REAL_RAW).  In a program compiled by imp72stage0, the seed, the word is the double's sign
 * and magnitude with the magnitude negated for a negative number (libimp72.a), because the seed translates the original code generator, which compares and
 * negates a real with the integer instructions.  C code that reads or writes the word of a REAL (a COMMON REAL, an element of a REAL array) converts with
 * these, and so is right for both.
 */
double imp_real_to_double(int64_t word);
int64_t imp_double_to_real(double d);

#endif
