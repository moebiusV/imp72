#include <stdio.h>
#include "imp72words.h"
word GETP(word *, word *), WALK(word *);
int main(void) {
    word a = 0x1234, n = 0x77, w[2] = { 0x0807060504030201LL, 0x1817161514131211LL };
    word r = GETP(&a, &n);
    printf("getp=%lld a=%llx\n", W(r), W(a));
    printf("walk=%llx\n", W(WALK(w)));
    return 0;
}
