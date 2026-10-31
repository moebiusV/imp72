#include <stdio.h>
#include "imp72words.h"
word T(word *), U(word *), NEG(word *), REL(word *);
int main(void) {
    word n = 10;
    printf("t=%lld u=%lld neg=%lld\n", W(T(&n)), W(U(&n)), W(NEG(&n)));
    printf("rel=%lld\n", W(REL(&n)));
    return 0;
}
