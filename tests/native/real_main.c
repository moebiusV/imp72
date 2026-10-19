#include <stdio.h>
#include "imp72words.h"
word t(word *), u(word *), neg(word *);
int main(void) {
    word n = 10;
    printf("t=%lld u=%lld neg=%lld\n", W(t(&n)), W(u(&n)), W(neg(&n)));
    return 0;
}
