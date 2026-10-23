/*
 * free.c -- free storage with relocatable arrays.  Ports decus/free.imc.
 *
 * All the compiler's tables live in one growable array of words, `fmem`,
 * as doubly linked "arrays", each with a four word header:
 *
 *     word 0   back link (HL), forward link (HR)   -- header indices
 *     word 1   name of the array
 *     word 2   increment for FADD (HL), current length (HR)
 *     word 3   0: fixed array; -1: being packed; else: 1 + a slot in
 *              `reloc`, naming the C variable that holds this array's pointer
 *              and a routine to call after it moves  (original: LOC(A),,S)
 *
 * An array is named by the index of its first data word, header + 4.  The
 * list runs from a dummy header at `forgn` to a dummy header at `fend`.
 * Arrays move, so a pointer held anywhere else goes stale; FRELOT arrays have
 * their registered variable updated whenever they move (FFIXER).
 *
 * Deviations from free.imc:
 *  - `fmem` is a C heap block grown with realloc, not the TOPS-10 low
 *    segment grown with CORE; sizes round to 1024 words as before.
 *  - the pointer location of a relocatable array is a registry slot, since a
 *    C address does not fit in a halfword.
 *  - FGAP has no 64000-word ceiling on the gap it will use.
 *  - FALLOT with a name (a "new array") is a separate function, fallot_new.
 *  - FADD and FADDEX take the address of the pointer variable (the original
 *    passes parameters by reference).
 */
#include <stdlib.h>
#include <string.h>
#include "imp.h"

word fb;                    /* the address of fmem (imp.h): the base of the arena */

static word fsize;          /* words allocated to fmem */
static word forgn = 8;      /* FORGN: dummy header at the bottom.  Not 0:
                               header index 0 means "no array" to FGAP. */
static word fend;           /* FEND:  dummy header at the top */
static int  init;           /* INIT */
static word incnom = 8;     /* INCNOM: nominal FADD increment */
static word trace;          /* TRACE */
static word tr;             /* TR: 4 = array stayed put, 6 = list reordered */
static word xfix;           /* XFIX: where FPACK left the array being grown */
static long falln;          /* FALN: calls on FALLOT that moved/resized */

#define BACK(x)      HL(fmem[x])
#define FWD(x)       HR(fmem[x])
#define SETBACK(x,v) (fmem[x] = HW((v), FWD(x)))
#define SETFWD(x,v)  (fmem[x] = HW(BACK(x), (v)))
#define LEN(x)       HR(fmem[(x) + 2])
#define SETLEN(x,n)  (fmem[(x) + 2] = HW(HL(fmem[(x) + 2]), (n)))

static struct reloc {
    word *loc;                  /* the variable holding the array's pointer */
    void (*fix)(void);          /* called after the pointer is updated */
} *reloc;
static word nreloc;

static word ftop(word n);
static word fgap(word n);
static void fpack(void);
static void ffixer(word x);
static void falotr(word x, word n);

/* CORE(N): make the low segment N+1 words long (indices 0..N). */
static int
core(word k)
{
    word *p;

    if (k + 1 <= fsize)
        return 1;
    p = realloc(fmem, (size_t)(k + 1) * sizeof *p);
    if (p == NULL)
        return 0;
    memset(p + fsize, 0, (size_t)(k + 1 - fsize) * sizeof *p);
    fb = (word)(intptr_t)p;
    fsize = k + 1;
    return 1;
}

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
    if (a == 0)
        incnom = n;
    else if (n >= 0)
        fmem[a - 2] = HW(n, LEN(a - 4));
}

void
ftrace(word v)
{
    trace = v;
}

/* FADDEX(A,N): extend the array to N + its increment (called by FADD). */
void
faddex(word *a, word n)
{
    word k = HL(fmem[*a - 2]);

    if (k == 0)
        k = incnom;
    if (k <= 0)
        k = 1;
    *a = fallot(*a, n + k);
}

/* FADD(A,N,V): V into word N of the array, then N = N+1. */
void
fadd(word *a, word *n, word v)
{
    if (flen(*a) <= *n)
        faddex(a, *n);
    FREES(*a + *n, v);
    *n += 1;
}

void
finit(void)
{
    word k = 1023 | (forgn + 7);        /* 1777B OR FORGN+7 */

    if (!core(k)) {
        imp_error(0, "CANNOT GET CORE TO INITIALIZE FREE STORAGE. ");
        return;
    }
    fend = k - 3;
    fmem[forgn] = HW(0, fend);
    fmem[fend] = HW(forgn, 0);
    fmem[forgn + 1] = fpackname("FORGN");
    fmem[forgn + 2] = fmem[forgn + 3] = 0;
    fmem[fend + 1] = fmem[fend + 2] = fmem[fend + 3] = 0;
    incnom = 8;
    init = 1;
}

/* Header x was found out of place or gone: release its registry slot. */
static void
release(word x)
{
    word s = fmem[x + 3];

    if (s > 0 && s <= nreloc)
        reloc[s - 1].loc = NULL;
}

/*
 * Place a new, zeroed array of n words after the array whose header is v.
 * Returns its header index.
 */
static word
place_after(word v, word name, word n)
{
    word w = FWD(v);
    word x = v + 4 + LEN(v);
    word i;

    fmem[x] = HW(v, w);
    fmem[x + 1] = name;
    fmem[x + 2] = HW(0, n);
    fmem[x + 3] = 0;
    for (i = 0; i < n; i++)
        fmem[x + 4 + i] = 0;
    SETFWD(v, x);
    SETBACK(w, x);
    return x;
}

/* FALLOT(name,N): allocate a new array.  Returns the pointer to its data. */
word
fallot_new(const char *name, word n)
{
    word nm = fpackname(name);
    word y, x;
    int packed = 0;

    if (!init)
        finit();
    if (trace || (finam5 & SW_STATS))
        falln++;
    tr = 6;
    if (n <= 0)
        return 0;
    for (;;) {
        y = fgap(n + 4);
        if (y == 0 && ftop(n + 4))
            y = BACK(fend);
        if (y != 0)
            break;
        if (packed) {                   /* packed once already: no room */
            char buf[16];
            unpackname(nm, buf);
            fprintf(stderr, "FALLOT %s %ld\n", buf, (long)n);
            fmap();
            imp_error(0, "FREE STORAGE EXHAUSTED. ");
            return 0;
        }
        fpack();
        packed = 1;
    }
    x = place_after(y, nm, n);
    if (trace) {
        char buf[16];
        unpackname(nm, buf);
        fprintf(stderr, "FALLOT %s %ld\n", buf, (long)n);
        if (trace & 4 || trace & 2)
            fmap();
    }
    return x + 4;
}

/*
 * FALLOT(A,N): set the length of array A to N, moving it if need be, and
 * return its (possibly new) pointer.  N <= 0 deletes it and returns 0.
 */
word
fallot(word a, word n)
{
    word x, v, w, lx, y, k, mm, x3sv, i;

    if (!init)
        finit();
    if (trace || (finam5 & SW_STATS))
        falln++;
    tr = 4;
    x = a - 4;
    v = BACK(x);
    w = FWD(x);
    lx = LEN(x);
    if (n <= 0) {
        SETFWD(v, w);
        SETBACK(w, v);
        release(x);
        if (trace)
            falotr(x, n);
        return 0;
    }
    if (n + 4 <= w - x)
        goto fa1;
    tr = 6;
    y = v + 4 + LEN(v);
    if (n + 4 <= w - y)
        goto fa2;
    y = fgap(n + 4);
    if (y)
        goto fa4;
    mm = n + 4;
    if (FWD(x) == fend)
        mm = n - lx;
    if (ftop(mm)) {
        y = BACK(fend);
        if (y == x)
            goto fa1;
        goto fa4;
    }
    x3sv = fmem[x + 3];             /* pack, remembering where x ends up */
    fmem[x + 3] = -1;
    xfix = x;
    fpack();
    x = xfix;
    fmem[x + 3] = x3sv;
    y = fgap(n + 4);
    if (y) {
        if (y == x)
            goto fa1;
        goto fa4;
    }
    mm = n + 4;
    if (FWD(x) == fend)
        mm = n - lx;
    if (ftop(mm)) {
        y = BACK(fend);
        if (y == x)
            goto fa1;
        goto fa4;
    }
    falotr(x, n);
    fmap();
    imp_error(0, "FREE STORAGE EXHAUSTED. ");
    return 0;

fa4:                                /* move x to just after array y */
    v = BACK(x);
    w = FWD(x);
    SETFWD(v, w);
    SETBACK(w, v);
    v = y;
    w = FWD(y);
    fmem[x] = HW(v, w);
    y = v + 4 + LEN(v);
fa2:                                /* move x down to y */
    memmove(&fmem[y], &fmem[x], (size_t)(lx + 4) * sizeof *fmem);
    SETFWD(v, y);
    SETBACK(w, y);
    x = y;
fa1:                                /* x has room: set its length */
    SETLEN(x, n);
    y = x + 4 + lx;
    k = (n - 1) - lx;
    for (i = 0; i <= k; i++)
        fmem[y + i] = 0;
    ffixer(x);
    if (trace) {
        falotr(x, n);
        if ((trace & tr & 2) || (trace & 4))
            fmap();
    }
    return x + 4;
}

/*
 * FRELOT(A,AA,N,S): allocate relocatable array AA of N words.  *a will be
 * kept pointing at it, and fix() (if not NULL) called after every move.
 */
word
frelot(word *a, const char *name, word n, void (*fix)(void))
{
    word x = fallot_new(name, n) - 4;
    word s;

    for (s = 0; s < nreloc && reloc[s].loc != NULL; s++)
        ;
    if (s == nreloc) {
        struct reloc *p = realloc(reloc, (size_t)(nreloc + 16) * sizeof *p);
        if (p == NULL) {
            imp_error(0, "OUT OF MEMORY FOR RELOCATION TABLE. ");
            return 0;
        }
        reloc = p;
        memset(reloc + nreloc, 0, 16 * sizeof *p);
        nreloc += 16;
    }
    reloc[s].loc = a;
    reloc[s].fix = fix;
    fmem[x + 3] = s + 1;
    ffixer(x);
    return *a;
}

/* FFIXER(X): the array at header X has (maybe) moved; tell its owner. */
static void
ffixer(word x)
{
    word y = fmem[x + 3];

    if (y == 0)
        return;
    if (y == -1) {
        xfix = x;
        return;
    }
    *reloc[y - 1].loc = x + 4;
    if (reloc[y - 1].fix != NULL)
        reloc[y - 1].fix();
}

/*
 * FGAP(N): header of the array after which there is the smallest gap of at
 * least N words, or 0.  The gap before FEND is not counted (FTOP's job).
 */
static word
fgap(word n)
{
    word k = forgn, k0 = 0, k1 = INT64_MAX, k2, k3;

    while (k != fend) {
        k2 = FWD(k);
        if (k != BACK(k2)) {
            fmap();
            imp_error(0, "FREE STORAGE UNLINKED. ");
            return 0;
        }
        if (k2 != fend) {
            k3 = k2 - (k + 4 + LEN(k));
            if (n <= k3 && k3 < k1) {
                k0 = k;
                k1 = k3;
            }
        }
        k = k2;
    }
    return k0;
}

/* FPACK: slide relocatable arrays down over the gaps, filling gaps that a
 * fixed array leaves with the largest relocatable array that fits. */
static void
fpack(void)
{
    word i = forgn, j, j1, k, k0, k1, k2, kb, kf;

    for (;;) {
        j = FWD(i);
        if (j == fend)
            return;
        j1 = i + 4 + LEN(i);
        if (j1 == j) {
            i = j;
            continue;
        }
        if (fmem[j + 3] != 0) {         /* j can move down to j1 */
            SETFWD(i, j1);
            k = FWD(j);
            SETBACK(k, j1);
            memmove(&fmem[j1], &fmem[j], (size_t)(LEN(j) + 4) * sizeof *fmem);
            ffixer(j1);
            i = j1;
            continue;
        }
        /* j is fixed: find the largest relocatable array that fits the gap */
        k = FWD(j);
        k0 = k1 = 0;
        while (k != fend) {
            if (fmem[k + 3] != 0) {
                k2 = j1 + 3 + LEN(k);   /* last word it would occupy */
                if (k2 < j && k2 > k1) {
                    k0 = k;
                    k1 = k2;
                }
            }
            k = FWD(k);
        }
        if (k0 == 0) {
            i = j;
            continue;
        }
        memmove(&fmem[j1], &fmem[k0], (size_t)(LEN(k0) + 4) * sizeof *fmem);
        SETFWD(i, j1);
        SETBACK(j, j1);
        kb = BACK(j1);                  /* old neighbours of the moved array */
        kf = FWD(j1);
        fmem[j1] = HW(i, j);
        SETFWD(kb, kf);
        SETBACK(kf, kb);
        ffixer(j1);
        i = j1;
    }
}

/* FTOP(N): make sure N words are free before FEND, growing core if need be.
 * Returns nonzero on success. */
static word
ftop(word n)
{
    word k = BACK(fend), top, y;

    k = fend - (k + 4 + LEN(k));
    if (n <= k)
        return -1;
    top = 1023 | (fend + 3 + (n - k));
    if (!core(top))
        return 0;
    k = top - 3;
    fmem[k] = fmem[fend];
    fmem[k + 1] = fmem[k + 2] = fmem[k + 3] = 0;
    y = BACK(k);
    SETFWD(y, k);
    fend = k;
    return -1;
}

/* FCLOSE: repack, shrink core, and rescind initialisation. */
void
fclose_(void)
{
    word x, y, i;

    fpack();
    x = BACK(fend);
    y = x + 4 + LEN(x);
    for (i = 0; i <= 3; i++)
        fmem[y + i] = fmem[fend + i];
    SETFWD(x, y);
    fend = y;
    forgn = y + 4;      /* a later FINIT opens a new area above the old */
    init = 0;
}

static void
falotr(word x, word n)
{
    char buf[16];

    unpackname(fmem[x + 1], buf);
    fprintf(stderr, "FALLOT %s %ld\n", buf, (long)n);
}

void
fstats(void)
{
    fprintf(stderr, "FREE STORAGE %ld WORDS: %lo - %lo\n",
        (long)(fend + 4 - 0), (long)forgn, (long)(fend + 3));
    if (falln) {
        fprintf(stderr, "%6ld REALLOTS\n", falln);
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

    if (fmem == NULL)
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
