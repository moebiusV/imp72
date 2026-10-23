/*
 * free_imp.c -- free.c's interface, over src/free.imp compiled by imp72stage0: the allocation routines (FINIT, FALLOT,
 * FRELOT, FADDEX, FADD, FINCSET, FGAP, FPACK, FTOP, FFIXER, FCLOSE of decus/free.imc) are the IMP module's; the
 * diagnostic print-outs (FMAP, FSTATS, the trace) and the debugging aid fcheck stay here.  See free.c for the layout
 * of the arrays: the arena is at the word fb (imp.h), `fmem`.
 */
#include <stdlib.h>
#include <string.h>
#include "imp.h"

extern word forgn, fend, nreloc, falln;     /* defined by the IMP module (src/shared.txt) */
extern word impl_ifnew(word *, word *), impl_ifallot(word *, word *), impl_ifrelot(word *, word *, word *, word *),
    impl_ifadd(word *, word *, word *), impl_ifaddex(word *, word *), impl_ifincset(word *, word *),
    impl_iftrace(word *), impl_ifinit(word *), impl_ifclose(word *);

#define BACK(x)      HL(fmem[x])
#define FWD(x)       HR(fmem[x])
#define LEN(x)       HR(fmem[(x) + 2])

/* Array names are up to 8 characters, first character in the top byte. */
word
fpackname(const char *name)
{
    uword w = 0;
    int i;

    for (i = 0; i < 8 && name[i] != '\0'; i++)
        w |= (uword)(unsigned char)name[i] << (56 - 8 * i);
    return (word)w;
}

static void
unpackname(word name, char *buf)
{
    int i, n = 0;

    for (i = 0; i < 8; i++) {
        char c = (char)((uword)name >> (56 - 8 * i));
        if (c != '\0')
            buf[n++] = c;
    }
    buf[n] = '\0';
}

/* the C routines the IMP module calls (CCALL, by value) */
long
imppackname(const char *s)
{
    return (long)fpackname(s);
}

long
impcallfix(long f)
{
    ((void (*)(void))(intptr_t)f)();
    return 0;
}

long
fallotnm(long nm, long n)
{
    char buf[16];

    unpackname((word)nm, buf);
    fprintf(stderr, "FALLOT %s %ld\n", buf, n);
    return 0;
}

long
falotr(long x, long n)
{
    char buf[16];

    unpackname(fmem[x + 1], buf);
    fprintf(stderr, "FALLOT %s %ld\n", buf, n);
    return 0;
}

word
flen(word a)
{
    return LEN(a - 4);
}

word
fname(word a)
{
    return fmem[a - 3];
}

void
fincset(word a, word n)
{
    impl_ifincset(&a, &n);
}

void
ftrace(word v)
{
    impl_iftrace(&v);
}

/* FADDEX(A,N): extend the array to N + its increment (called by FADD). */
void
faddex(word *a, word n)
{
    impl_ifaddex(a, &n);
}

/* FADD(A,N,V): V into word N of the array, then N = N+1. */
void
fadd(word *a, word *n, word v)
{
    impl_ifadd(a, n, &v);
}

void
finit(void)
{
    word z = 0;

    impl_ifinit(&z);
}

word
fallot_new(const char *name, word n)
{
    word nm = fpackname(name);

    return impl_ifnew(&nm, &n);
}

word
fallot(word a, word n)
{
    return impl_ifallot(&a, &n);
}

word
frelot(word *a, const char *name, word n, void (*fix)(void))
{
    word al = (word)(intptr_t)a, nm = fpackname(name), fx = (word)(intptr_t)fix;

    return impl_ifrelot(&al, &nm, &n, &fx);
}

void
fclose_(void)
{
    word z = 0;

    impl_ifclose(&z);
}

void
fstats(void)
{
    fprintf(stderr, "FREE STORAGE %ld WORDS: %lo - %lo\n",
        (long)(fend + 4 - 0), (long)forgn, (long)(fend + 3));
    if (falln) {
        fprintf(stderr, "%6ld REALLOTS\n", (long)falln);
        falln = 0;
    }
}

void
fmap(void)
{
    word k = forgn, k1, l;
    char buf[16];

    fprintf(stderr, " ARRAY  ORG  LENGTH GAP\n");
    while (k != fend) {
        unpackname(fmem[k + 1], buf);
        k1 = FWD(k);
        l = LEN(k);
        fprintf(stderr, "%-6s%c %5lo%6ld%4ld\n", buf,
            fmem[k + 3] != 0 ? ' ' : '.', (long)k, (long)l,
            (long)(k1 - (k + 4 + l)));
        k = k1;
    }
    fstats();
}

/* FCHECK(WHERE): walk the arrays and complain, on stderr, about a header that cannot be right: a debugging
 * aid for the IMP modules, which write into free storage themselves (set IMP_FCHECK to use it). */
int
fcheck(const char *where)
{
    word x = forgn, nx;
    int bad = 0;

    if (fb == 0)
        return 0;
    for (;;) {
        nx = FWD(x);
        if (nx == 0)
            break;
        if (nx <= x || nx > fend || BACK(nx) != x) {
            fprintf(stderr, "fcheck %s: bad link at %ld -> %ld\n", where, (long)x, (long)nx);
            bad = 1;
            if (getenv("IMP_FCHECK_ABORT"))
                abort();
            break;
        }
        if (nx != fend && (fmem[nx + 3] < -1 || fmem[nx + 3] > nreloc)) {
            fprintf(stderr, "fcheck %s: bad relocation slot %ld at array %ld\n", where,
                (long)fmem[nx + 3], (long)nx);
            bad = 1;
            if (getenv("IMP_FCHECK_ABORT"))
                abort();
        }
        x = nx;
    }
    return bad;
}
