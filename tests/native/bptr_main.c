#include <stdio.h>
#include "imp72words.h"
word getp(word *, word *), walk(word *);
int main(void) {
    word a = 0x1234, n = 0x77, w[2] = { 0x0807060504030201LL, 0x1817161514131211LL };
    word r = getp(&a, &n);
    printf("getp=%lld a=%llx\n", W(r), W(a));
    printf("walk=%llx\n", W(walk(w)));
    return 0;
}
