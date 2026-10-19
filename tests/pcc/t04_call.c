extern int printf(const char *, ...);
long f8(long a, long b, long c, long d, long e, long f, long g, long h) { return a + 2*b + 3*c + 4*d + 5*e + 6*f + 7*g + 8*h; }
long f12(long a, long b, long c, long d, long e, long f, long g, long h, long i, long j, long k, long l) {
    return a - b + c - d + e - f + g - h + i - j + k - l + 1000 * (i + j + k + l); }
double fd(double a, double b, double c, double d, double e, double f, double g, double h, double i, double j) {
    return a + 2*b + 3*c + 4*d + 5*e + 6*f + 7*g + 8*h + 9*i + 10*j; }
double mix(int a, double b, long c, double d, char e, float f, unsigned g, double h, short s, double z) {
    return a + b + c + d + e + f + g + h + s + z; }
int ptr(int *p, long *q, char *c) { return *p + (int)*q + *c; }
int (*fp)(int *, long *, char *) = ptr;
int apply(int (*f)(int *, long *, char *), int *p, long *q, char *c) { return f(p, q, c) * 2; }
int main(void) {
    int i = 5; long l = 70; char c = 3;
    printf("%ld %ld\n", f8(1,2,3,4,5,6,7,8), f12(1,2,3,4,5,6,7,8,9,10,11,12));
    printf("%g\n", fd(1,2,3,4,5,6,7,8,9,10));
    printf("%g\n", mix(1, 2.5, 3, 4.5, 5, 6.5f, 7, 8.5, 9, 10.5));
    printf("%d %d %d\n", ptr(&i, &l, &c), fp(&i, &l, &c), apply(ptr, &i, &l, &c));
    return 0;
}
