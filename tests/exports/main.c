#include <stdio.h>
#include "sample.h"

int main(void) {
    int64_t a = 2, b = 3, i = 2, n = 10, t = 100, x = 9;

    TAB[2] = 7;
    FOO = 5;
    printf("%ld %ld %ld %ld %ld\n", (long)ADD2(&a, &b), (long)NOARGS(), (long)GETTAB(&i), (long)FOOPLUS(&n, &t), (long)Mixed(&x));
    return 0;
}
