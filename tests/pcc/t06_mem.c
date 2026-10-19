extern int printf(const char *, ...);
extern int puts(const char *);
struct pt { int x, y; };
int g1 = 5; long g2 = -6; char g3 = 'a'; short g4 = -7; unsigned char g5 = 200; double g6 = 2.5; float g7 = 1.25f;
char buf[40] = "hello, world";
int arr[6] = { 1, 2, 3, 4, 5, 6 };
char *msgs[3] = { "alpha", "beta", "gamma" };
static int counter;
int next(void) { static int n = 10; return n++ + counter++; }
int sum(int *a, int n) { int s = 0; while (n--) s += *a++; return s; }
void swap(int *a, int *b) { int t = *a; *a = *b; *b = t; }
int slen(const char *s) { int n = 0; while (*s++) n++; return n; }
int main(void) {
    int i, x = 3, y = 4;
    printf("%d %ld %c %d %d %g %g\n", g1, g2, g3, g4, g5, g6, g7);
    g1 += 3; g2 *= 7; g3++; g4 -= 100; g5 += 100; g6 *= 3; g7 /= 5;
    printf("%d %ld %c %d %d %g %g\n", g1, g2, g3, g4, g5, g6, g7);
    puts(buf); buf[0] = 'J'; buf[5] = 0; puts(buf);
    printf("%d %d %d\n", sum(arr, 6), sum(arr + 2, 3), slen(msgs[2]));
    swap(&x, &y); printf("%d %d\n", x, y);
    for (i = 0; i < 3; i++) printf("%s %d\n", msgs[i], next());
    {
        int local[100]; char lc[300]; long ll[40];
        for (i = 0; i < 100; i++) local[i] = i * 3;
        for (i = 0; i < 300; i++) lc[i] = (char)i;
        for (i = 0; i < 40; i++) ll[i] = (long)i << 33;
        printf("%d %d %d %ld %d\n", local[99], lc[299], lc[1], ll[39], sum(local, 100));
    }
    {
        struct pt p; struct pt *pp = &p; p.x = 3; pp->y = 9;
        printf("%d %d\n", p.x, pp->y);
    }
    return 0;
}
