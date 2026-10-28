#include <stdio.h>
#include <stdint.h>
#include "imp72.h"
extern int64_t x, y, z, v[4];
int64_t setup(int64_t *), usec(int64_t *), cmpr(int64_t *);
int main(void) {
    int64_t nil = 0;
    int i;

    setup(&nil);
    printf("%.17g %.17g %.17g\n", imp_real_to_double(x), imp_real_to_double(y), imp_real_to_double(z));
    for (i = 0; i < 4; i++)
        printf("v[%d]=%.17g\n", i, imp_real_to_double(v[i]));
    x = imp_double_to_real(0.5);                   /* C stores, IMP computes */
    y = imp_double_to_real(-4.25);
    usec(&nil);
    printf("x=%.17g (0.5+-4.25) v0=%.17g\n", imp_real_to_double(x), imp_real_to_double(v[0]));
    printf("y<x: %lld\n", (long long)cmpr(&nil));
    x = imp_double_to_real(-9.0);
    printf("y<x: %lld\n", (long long)cmpr(&nil));
    #ifdef IMP_REAL_RAW             /* the code generator of src/pccir: a REAL word is the double's own bits */
    printf("order: %d %d\n", imp_real_to_double(imp_double_to_real(-2.0)) < imp_real_to_double(imp_double_to_real(-1.0)),
        imp_real_to_double(imp_double_to_real(1.0)) < imp_real_to_double(imp_double_to_real(2.0)));
#else                           /* the original compiler's order preserving words: integer order is real order */
    printf("order: %d %d\n", imp_double_to_real(-2.0) < imp_double_to_real(-1.0), imp_double_to_real(1.0) < imp_double_to_real(2.0));
#endif
    return 0;
}
