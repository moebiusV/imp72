/* the routines and variables of the IMP72 sources, for C (written by exports) */
#ifndef SAMPLE_H
#define SAMPLE_H
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern int64_t FOO;
extern int64_t TAB[8];
int64_t ADD2(int64_t *A, int64_t *B);
int64_t NOARGS(void);
int64_t GETTAB(int64_t *I);
int64_t FOOPLUS(int64_t *N, int64_t *type);
int64_t Mixed(int64_t *X);

#ifdef __cplusplus
}
#endif

#endif
