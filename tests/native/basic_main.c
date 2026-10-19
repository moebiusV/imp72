#include <stdio.h>
#include "imp72words.h"
word add2(word *, word *);
word mul3(word *);
word maxv(word *, word *);
word sumto(word *);
int main(void) {
    word a = 20, b = 22, n = 10, x = 14, p = 7, q = 9;
    printf("add2=%lld mul3=%lld maxv=%lld,%lld sumto=%lld\n", W(add2(&a, &b)), W(mul3(&x)),
        W(maxv(&p, &q)), W(maxv(&q, &p)), W(sumto(&n)));
    return 0;
}
