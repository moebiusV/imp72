/* IMP72 words are 64 bits on every machine, whatever `long` is */
#include <stdint.h>
typedef int64_t word;
#define W(x) ((long long)(x))
