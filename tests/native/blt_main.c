#include <stdio.h>
#include "imp72words.h"
word FILL(word *), COPY(word *), SUMQ(word *);
int main(void) {
    word n = 10;
    FILL(&n);
    COPY(&n);
    printf("sumq=%lld\n", W(SUMQ(&n)));
    return 0;
}
