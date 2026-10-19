#include <stdio.h>
#include "imp72words.h"
word fill(word *), copy(word *), sumq(word *);
int main(void) {
    word n = 10;
    fill(&n);
    copy(&n);
    printf("sumq=%lld\n", W(sumq(&n)));
    return 0;
}
