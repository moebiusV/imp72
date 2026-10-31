#include <stdio.h>
#include <stdint.h>
#include "imp72.h"
extern int64_t X, Y, Z, V[4];
int64_t SETUP(int64_t *), USEC(int64_t *), CMPR(int64_t *);
int main(void) {
    int64_t nil = 0;
    int i;

    SETUP(&nil);
    printf("%.17g %.17g %.17g\n", imp_real_to_double(X), imp_real_to_double(Y), imp_real_to_double(Z));
    for (i = 0; i < 4; i++)
        printf("v[%d]=%.17g\n", i, imp_real_to_double(V[i]));
    X = imp_double_to_real(0.5);                   /* C stores, IMP computes */
    Y = imp_double_to_real(-4.25);
    USEC(&nil);
    printf("x=%.17g (0.5+-4.25) v0=%.17g\n", imp_real_to_double(X), imp_real_to_double(V[0]));
    printf("y<x: %lld\n", (long long)CMPR(&nil));
    X = imp_double_to_real(-9.0);
    printf("y<x: %lld\n", (long long)CMPR(&nil));
    printf("order: %d %d\n", imp_real_to_double(imp_double_to_real(-2.0)) < imp_real_to_double(imp_double_to_real(-1.0)),
        imp_real_to_double(imp_double_to_real(1.0)) < imp_real_to_double(imp_double_to_real(2.0)));
    return 0;
}
