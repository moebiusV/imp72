#include <stdio.h>
#include "imp72words.h"
word fill(word *), total(word *), fact(word *), gcd(word *, word *), twice(word *), caller(word *), bits(word *), rem(word *, word *);
int main(void) {
    word n = 10, a = 5, b = 84, c = 36, d = 12345, e = 17, f = 5;
    fill(&n);
    printf("total=%lld fact=%lld gcd=%lld twice=%lld caller=%lld bits=%lld rem=%lld\n", W(total(&n)), W(fact(&a)), W(gcd(&b, &c)), W(twice(&e)), W(caller(&f)), W(bits(&d)), W(rem(&e, &f)));
    return 0;
}
