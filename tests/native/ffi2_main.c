#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "imp72words.h"
extern long v, arr[8];
word len(word *), sort(word *), show(word *);
int main(void) {
    word n = 5, p = 0;
    char s[16] = "hello!";
    long q = (long)s;
    arr[0] = 5; arr[1] = 3; arr[2] = 9; arr[3] = 1; arr[4] = 7;
    printf("len=%lld\n", W(len((word *)&q)));
    printf("sort=%lld\n", W(sort(&n)));
    printf("arr=%ld %ld %ld %ld %ld\n", arr[0], arr[1], arr[2], arr[3], arr[4]);
    show(&n); printf("\n");
    (void)p;
    return 0;
}
