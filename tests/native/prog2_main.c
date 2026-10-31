#include <stdio.h>
#include "imp72words.h"
word FILL(word *), TOTAL(word *), FACT(word *), GCD(word *, word *), TWICE(word *), CALLER(word *), BITS(word *), REM(word *, word *);
int main(void) {
    word n = 10, a = 5, b = 84, c = 36, d = 12345, e = 17, f = 5;
    FILL(&n);
    printf("total=%lld fact=%lld gcd=%lld twice=%lld caller=%lld bits=%lld rem=%lld\n", W(TOTAL(&n)), W(FACT(&a)), W(GCD(&b, &c)), W(TWICE(&e)), W(CALLER(&f)), W(BITS(&d)), W(REM(&e, &f)));
    return 0;
}
