extern int printf(const char *, ...);
int fib(int n) { return n < 2 ? n : fib(n - 1) + fib(n - 2); }
int gcd(int a, int b) { while (b) { int t = a % b; a = b; b = t; } return a; }
long fact(int n) { long r = 1; int i; for (i = 2; i <= n; i++) r *= i; return r; }
int sw(int x) {
    switch (x) {
    case 0: return 10;
    case 1: return 11;
    case 2: case 3: return 23;
    case 100: return 1000;
    case -5: return -50;
    default: return -1;
    }
}
int logic(int a, int b, int c) { return (a && b) || (!c && a) ? 1 : (a || b ? 2 : 3); }
int main(void) {
    int i, s = 0;
    printf("%d %d %ld\n", fib(20), gcd(1071, 462), fact(20));
    for (i = -7; i < 110; i += 3) printf("%d ", sw(i));
    printf("\n");
    for (i = 0; i < 8; i++) printf("%d ", logic(i & 1, i & 2, i & 4));
    printf("\n");
    i = 0;
    while (1) { i++; if (i == 3) continue; if (i > 8) break; s += i; }
    do { s += 100; i--; } while (i > 5);
    printf("%d %d\n", s, i);
    { int a[10], j; for (j = 0; j < 10; j++) a[j] = j * j; s = 0; for (j = 9; j >= 0; j--) s = s * 3 + a[j] % 7; printf("%d\n", s); }
    goto L;
    s = 999;
L:  printf("%d\n", s);
    return 0;
}
