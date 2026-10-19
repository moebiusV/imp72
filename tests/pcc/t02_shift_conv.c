extern int printf(const char *, ...);
int main(void) {
    long x = -123456789012345L;
    unsigned long ux = 0xfedcba9876543210UL;
    int i = -1000, k;
    unsigned u = 0x87654321U;
    for (k = 0; k < 64; k += 7) {
        printf("%d: %ld %ld %lu %lu\n", k, x << k, x >> k, ux << k, ux >> k);
    }
    for (k = 0; k < 32; k += 5) {
        printf("%d: %d %d %u %u\n", k, i << k, i >> k, u << k, u >> k);
    }
    printf("%ld %ld\n", x << 3, x >> 3);
    printf("%d %d %u\n", i << 4, i >> 4, u >> 4);
    {
        char c = -5; unsigned char uc = 250; short s = -300; unsigned short us = 60000;
        printf("%d %d %d %d\n", c, uc, s, us);
        printf("%ld %ld %ld %ld\n", (long)c, (long)uc, (long)s, (long)us);
        printf("%d %d %d %d\n", (char)i, (unsigned char)i, (short)i, (unsigned short)i);
        printf("%d %d\n", (char)x, (unsigned char)x);
        printf("%ld %ld %ld\n", (long)i, (long)u, (long)(int)u);
        printf("%lu %lu\n", (unsigned long)i, (unsigned long)(unsigned)i);
        c++; uc++; s--; us++;
        printf("%d %d %d %d\n", c, uc, s, us);
        c = 127; c++; uc = 255; uc++;
        printf("%d %d\n", c, uc);
    }
    return 0;
}
