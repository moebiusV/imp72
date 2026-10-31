#include <stdio.h>
#include "imp72words.h"
word ADD2(word *, word *);
word MUL3(word *);
word MAXV(word *, word *);
word SUMTO(word *);
int main(void) {
    word a = 20, b = 22, n = 10, x = 14, p = 7, q = 9;
    printf("add2=%lld mul3=%lld maxv=%lld,%lld sumto=%lld\n", W(ADD2(&a, &b)), W(MUL3(&x)),
        W(MAXV(&p, &q)), W(MAXV(&q, &p)), W(SUMTO(&n)));
    return 0;
}
