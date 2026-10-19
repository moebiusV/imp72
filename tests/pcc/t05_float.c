extern int printf(const char *, ...);
double da[6] = { 1.5, -2.25, 100.125, -0.5, 3.0, 1e10 };
float fa[4] = { 1.5f, -2.25f, 100.125f, 0.1f };
int main(void) {
    int i, j;
    for (i = 0; i < 6; i++)
        for (j = 0; j < 6; j++) {
            double a = da[i], b = da[j];
            printf("%g %g: %g %g %g %g %d %d %d %d %d %d\n", a, b, a + b, a - b, a * b, a / b,
                a < b, a <= b, a == b, a != b, a > b, a >= b);
        }
    for (i = 0; i < 4; i++)
        for (j = 0; j < 4; j++) {
            float a = fa[i], b = fa[j];
            printf("%g %g: %g %g %g %g %d %d\n", a, b, a + b, a - b, a * b, a / b, a < b, a >= b);
        }
    for (i = 0; i < 6; i++) {
        double a = da[i];
        printf("%g -> %d %ld %u %g %g\n", a, (int)a, (long)a, (unsigned)(a < 0 ? -a : a), (double)(float)a, -a);
    }
    {
        long l = -123456789L; int n = 77; unsigned u = 4000000000U; unsigned long ul = 18000000000000000000UL;
        printf("%g %g %g %g\n", (double)l, (double)n, (double)u, (double)ul);
        printf("%g %g\n", (float)l, (float)u);
    }
    return 0;
}
