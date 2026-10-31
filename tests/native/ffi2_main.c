#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "imp72words.h"
extern int64_t V, ARR[8];
word LEN(word *), SORT(word *), SHOW(word *);
int main(void) {
    word n = 5, p = 0;
    char s[16] = "hello!";
    long q = (long)s;
    ARR[0] = 5; ARR[1] = 3; ARR[2] = 9; ARR[3] = 1; ARR[4] = 7;
    printf("len=%lld\n", W(LEN((word *)&q)));
    printf("sort=%lld\n", W(SORT(&n)));
    printf("arr=%lld %lld %lld %lld %lld\n", W(ARR[0]), W(ARR[1]), W(ARR[2]), W(ARR[3]), W(ARR[4]));
    SHOW(&n); printf("\n");
    (void)p;
    return 0;
}
