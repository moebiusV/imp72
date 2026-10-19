extern int printf(const char *, ...);
long long la[8] = { 7, -7, 1000000007LL, -1000000007LL, 9223372036854775807LL, -3, 4294967296LL, -299850 };
unsigned long long ua[6] = { 7, 18446744073709551615ULL, 1, 4294967295ULL, 4294967296ULL, 100 };
int main(void) {
    int i, j;
    for (i = 0; i < 8; i++)
        for (j = 0; j < 8; j++) {
            long long a = la[i], b = la[j];
            printf("%lld %lld: %lld %lld %lld %lld %lld\n", a, b, a + b, a - b, a * b,
                b ? a / b : 0, b ? a % b : 0);
            printf("  %d %d %d %d %lld %lld %lld\n", a < b, a == b, a > b, a >= b, a & b, a | b, a ^ b);
        }
    for (i = 0; i < 6; i++)
        for (j = 0; j < 6; j++) {
            unsigned long long a = ua[i], b = ua[j];
            printf("%llu %llu: %llu %llu %llu %llu %llu %d %d\n", a, b, a + b, a - b, a * b,
                b ? a / b : 0, b ? a % b : 0, a < b, a >= b);
        }
    {
        long long x = -299850LL; int k;
        printf("%lld %lld %lld\n", x * 10, 10 * x, x * 1000);
        for (k = 0; k < 64; k += 9) printf("%lld %lld %llu\n", x << k, x >> k, (unsigned long long)x >> k);
        printf("%lld %lld\n", (long long)(int)x, (long long)(unsigned)x);
        printf("%g %g\n", (double)x, (double)(unsigned long long)x);
        printf("%lld %llu\n", (long long)-2.9e9, (unsigned long long)3.0e19);
    }
    return 0;
}
