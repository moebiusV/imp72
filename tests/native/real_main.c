#include <stdio.h>
#include "imp72words.h"
word t(word *), u(word *), neg(word *), rel(word *);
int main(void) {
    word n = 10;
    printf("t=%lld u=%lld neg=%lld\n", W(t(&n)), W(u(&n)), W(neg(&n)));
    printf("rel=%lld\n", W(rel(&n)));
    return 0;
}
