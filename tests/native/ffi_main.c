#include <stdio.h>
#include <stdlib.h>
#include "imp72words.h"
word F(word *, word *), G(word *);
/* C functions called with CCALL take the values as C longs */
long cadd3(long a, long b, long c) { return a * 10000 + b * 100 + c; }
long cnone(void) { return 42; }
int main(void) {
    word a = 3, b = 4;
    printf("f=%lld g=%lld\n", W(F(&a, &b)), W(G(&a)));
    return 0;
}
