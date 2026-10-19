extern int printf(const char *, ...);
int ia[8] = { 7, -7, 100, -100, 1, 0, 2147483647, -2147483647 };
long la[6] = { 7, -7, 1000000007L, -1000000007L, 9223372036854775807L, -3 };
unsigned ua[5] = { 7, 4000000000U, 1, 0, 255 };
unsigned long ula[4] = { 7, 18446744073709551615UL, 1, 100 };
int main(void) {
    int i, j;
    for (i = 0; i < 8; i++)
        for (j = 0; j < 8; j++) {
            int a = ia[i], b = ia[j];
            printf("%d %d: %d %d %d %d %d %d %d %d\n", a, b, a + b, a - b, a * b,
                b ? a / b : 0, b ? a % b : 0, a & b, a | b, a ^ b);
            printf("  %d %d %d %d %d %d\n", a < b, a <= b, a == b, a != b, a > b, a >= b);
        }
    for (i = 0; i < 6; i++)
        for (j = 0; j < 6; j++) {
            long a = la[i], b = la[j];
            printf("%ld %ld: %ld %ld %ld %ld %ld\n", a, b, a + b, a - b, a * b,
                b ? a / b : 0, b ? a % b : 0);
            printf("  %d %d %d %d\n", a < b, a == b, a > b, a >= b);
        }
    for (i = 0; i < 5; i++)
        for (j = 0; j < 5; j++) {
            unsigned a = ua[i], b = ua[j];
            printf("%u %u: %u %u %u %u %u %d %d\n", a, b, a + b, a - b, a * b,
                b ? a / b : 0, b ? a % b : 0, a < b, a >= b);
        }
    for (i = 0; i < 4; i++)
        for (j = 0; j < 4; j++) {
            unsigned long a = ula[i], b = ula[j];
            printf("%lu %lu: %lu %lu %lu %lu %lu %d %d\n", a, b, a + b, a - b, a * b,
                b ? a / b : 0, b ? a % b : 0, a < b, a >= b);
        }
    return 0;
}
