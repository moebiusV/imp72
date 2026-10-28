/*
 * imp72rt.c -- the rest of the runtime library of IMP72 programs: the
 * monitor calls the compiler's own sources make (CALLI), BLT and IFIX.  PRINT,
 * READ and FINI are in imp72io.c.
 */
#define _DEFAULT_SOURCE 1
#define _BSD_SOURCE 1                   /* (before glibc 2.19 the name of it) */
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <string.h>
#include "imp72real.h"

/*
 * CALLI(code, ac): the TOPS-10 monitor calls the compiler's sources make.
 * 0 reset, 012 exit, 014 date, 023 time of day in ms, 024 and 030 job
 * identification, 027 run time in ms; the others do nothing.
 */
#include <time.h>

long
imp_calli(long code, long ac)
{
    switch (code) {
    case 012:
        fflush(stdout);
        exit(0);
    case 014: {                         /* the date, as the monitor gives it: ((year-1964)*12 + month-1)*31 + day-1 */
        time_t t = time(NULL);
        struct tm *tm = localtime(&t);

        return tm == NULL ? 0 : ((tm->tm_year + 1900 - 1964) * 12L + tm->tm_mon) * 31 + tm->tm_mday - 1;
    }
    case 023: {
        time_t t = time(NULL);
        struct tm *tm = localtime(&t);

        return tm == NULL ? 0 : ((tm->tm_hour * 60L + tm->tm_min) * 60 + tm->tm_sec) * 1000;
    }
    case 027:
        return (long)(clock() * 1000 / CLOCKS_PER_SEC);
    case 030:
        return 1;
    default:
        return 0;
    }
}


/*
 * EXECUTE: imp_xct() does the one instruction of the pcc IR machine (docs/porting/pccir-machine.md) that the word imp_xw holds, the way the
 * compiled program would have done it where it stands.  An instruction word has the operation in bits 56-63 (the numbers of the
 * machine: 1 LOAD, 2 NEG, 3 COMPL, 4 ADDR, 5 STORE, 6 ADD ... 20 EQV, 21 LSH, 22 ASH, 23 ROT, 28 ADDX, 64 CALLI), the accumulator
 * in bits 52-55, the index register in bits 48-51, bit 47 set for an immediate operand, and in bits 0-46 the address or the number
 * (signed).  The accumulators are the words of imp_ac, which the compiled code fills from its registers 0R to 15R before the
 * call and reads back after.  As in compiled code an address is a byte address: with an index register it is that register plus
 * 8 times the address field (a word index), without one it is the field itself; an immediate operand is the field plus the
 * index register.  The floating point operations and the jumps are not done: they need the program's own registers and tags.
 */
int64_t imp_ac[16];
int64_t imp_xw;

void
imp_xct(void)
{
    int64_t w = imp_xw;
    unsigned op = (unsigned)(((uint64_t)w >> 56) & 0xFF), a = (unsigned)((w >> 52) & 15), x = (unsigned)((w >> 48) & 15);
    int64_t f = w & 0x7FFFFFFFFFFFL, v, r, *m;
    int imm = (w >> 47) & 1;

    if (f & 0x400000000000L)
        f -= 0x800000000000L;
    m = (int64_t *)(intptr_t)(x ? imp_ac[x] + 8 * f : f);
    v = imm ? f + (x ? imp_ac[x] : 0) : (op == 4 || op == 5 ? 0 : *m);
    switch (op) {
    case 1: imp_ac[a] = v; return;
    case 2: imp_ac[a] = -v; return;
    case 3: imp_ac[a] = ~v; return;
    case 4: imp_ac[a] = (int64_t)(intptr_t)m; return;
    case 5: *m = imp_ac[a]; return;
    case 6: imp_ac[a] += v; return;
    case 7: imp_ac[a] -= v; return;
    case 8: imp_ac[a] *= v; return;
    case 9: imp_ac[a] = v ? imp_ac[a] / v : 0; return;
    case 10: imp_ac[a] = v ? imp_ac[a] % v : 0; return;
    case 11: imp_ac[a] &= v; return;
    case 12: imp_ac[a] = ~imp_ac[a] & v; return;
    case 13: imp_ac[a] = imp_ac[a] & ~v; return;
    case 14: imp_ac[a] = ~imp_ac[a] & ~v; return;
    case 15: imp_ac[a] |= v; return;
    case 16: imp_ac[a] = ~imp_ac[a] | v; return;
    case 17: imp_ac[a] = imp_ac[a] | ~v; return;
    case 18: imp_ac[a] = ~imp_ac[a] | ~v; return;
    case 19: imp_ac[a] ^= v; return;
    case 20: imp_ac[a] = ~(imp_ac[a] ^ v); return;
    case 21:
    case 22:
        r = v;
        if (r >= 64 || r <= -64)
            imp_ac[a] = op == 22 && r < 0 && imp_ac[a] < 0 ? -1 : 0;
        else if (r >= 0)
            imp_ac[a] = (int64_t)((uint64_t)imp_ac[a] << r);
        else
            imp_ac[a] = op == 22 ? imp_ac[a] >> -r : (int64_t)((uint64_t)imp_ac[a] >> -r);
        return;
    case 23:
        r = ((v % 64) + 64) % 64;
        imp_ac[a] = r ? (int64_t)(((uint64_t)imp_ac[a] << r) | ((uint64_t)imp_ac[a] >> (64 - r))) : imp_ac[a];
        return;
    case 28: imp_ac[a] += 8 * v; return;
    case 64: imp_ac[a] = imp_calli((long)v, (long)imp_ac[a]); return;
    default:
        fprintf(stderr, "EXECUTE: the operation %u of the machine cannot be done from a word\n", op);
        exit(1);
    }
}

/* BLT: copy bytes words from src to dst (the ranges may overlap, as the
 * PDP-10 copies upward word by word; memmove is the safe superset). */
#include <string.h>

long
imp_blt(char *dst, char *src, long bytes)
{
    if (bytes > 0)
        memmove(dst, src, (size_t)bytes);
    return 0;
}

/* IFIX(x): a real to an integer, truncating (the library routine the
 * original reaches as MALAMUD).  The argument is the address of the real. */
long
ifix(int64_t *p)
{
    uint64_t bits = real_bits(*p);
    double x;

    memcpy(&x, &bits, sizeof x);
    return (long)x;
}

/* A REAL's word and the IEEE double it holds (lib/imp72.h). */
double
imp_real_to_double(int64_t w)
{
    uint64_t bits = real_bits(w);
    double d;

    memcpy(&d, &bits, sizeof d);
    return d;
}

int64_t
imp_double_to_real(double d)
{
    uint64_t bits;

    memcpy(&bits, &d, sizeof bits);
    return real_word(bits);
}

/* The shifts and the rotation of a word by a variable count (the code generator of src/pccir calls them): a positive count is to the left, a negative one to
 * the right; a logical shift of 64 places or more gives 0.  The arguments and the result are the addresses of 64 bit words (a 32 bit machine passes a long as
 * 32 bits, and a call cannot carry a word). */
void
imp_lsh(int64_t *px, int64_t *pn, int64_t *pr)
{
    int64_t x = *px, n = *pn;

    *pr = (n >= 64 || n <= -64) ? 0 : n >= 0 ? (int64_t)((uint64_t)x << n) : (int64_t)((uint64_t)x >> -n);
}

void
imp_ash(int64_t *px, int64_t *pn, int64_t *pr)
{
    int64_t x = *px, n = *pn;

    *pr = n >= 64 ? 0 : n >= 0 ? (int64_t)((uint64_t)x << n) : x >> (n <= -64 ? 63 : -n);
}

void
imp_rot(int64_t *px, int64_t *pn, int64_t *pr)
{
    uint64_t u = (uint64_t)*px;
    int n = (int)(*pn & 63);

    *pr = n == 0 ? *px : (int64_t)((u << n) | (u >> (64 - n)));
}

/*
 * Byte pointers.  A pointer to characters (the tape's BYTEP A<7,36>) is a byte address: IBP adds 1, LDB reads the byte there, DPB writes the low 8 bits
 * of the value to it (a character is a byte, UTF-8).  Any other pointer names a bit field and keeps the layout of the PDP-10's, in one word: bits 63-58
 * the size, 57-52 the position (the low bit of the field, counted from the right), the rest the byte address of the word; IBP moves to the next lower
 * field of the same size, or to the next word.  A byte address is below 2**47, so the size bits tell the two apart.  The pointer is in imp_bpp and the
 * byte in imp_bpv, for a call cannot carry a word (a 32 bit machine passes a long as 32 bits).
 */
int64_t imp_bpp, imp_bpv;

void
imp_ibp(void)
{
    uint64_t p = (uint64_t)imp_bpp;
    int size = (int)(p >> 58), pos = (int)((p >> 52) & 63), np;

    if (size == 0) {
        imp_bpp = (int64_t)(p + 1);
        return;
    }
    np = pos - size;
    if (np < 0) {
        np = 64 - size;
        p += 8;
    }
    imp_bpp = (int64_t)((p & ~(63ULL << 52)) | ((uint64_t)np << 52));
}

void
imp_ldb(void)
{
    uint64_t p = (uint64_t)imp_bpp;
    int size = (int)(p >> 58), pos = (int)((p >> 52) & 63);

    if (size == 0) {
        imp_bpv = *(unsigned char *)(intptr_t)p;
        return;
    }
    imp_bpv = (int64_t)(((uint64_t)*(int64_t *)(intptr_t)(p & 0xFFFFFFFFFFFFFULL) >> pos) & (size >= 64 ? ~0ULL : (1ULL << size) - 1));
}

void
imp_dpb(void)
{
    uint64_t p = (uint64_t)imp_bpp;
    int size = (int)(p >> 58), pos = (int)((p >> 52) & 63);
    uint64_t m;
    int64_t *w;

    if (size == 0) {
        *(unsigned char *)(intptr_t)p = (unsigned char)imp_bpv;
        return;
    }
    m = (size >= 64 ? ~0ULL : (1ULL << size) - 1) << pos;
    w = (int64_t *)(intptr_t)(p & 0xFFFFFFFFFFFFFULL);
    *w = (int64_t)(((uint64_t)*w & ~m) | (((uint64_t)imp_bpv << pos) & m));
}

/* MOVE B THROUGH N TO A: N+1 words from b to a, ascending, as BLT does (the arguments are pointers) */
long
imp_blk(int64_t *a, int64_t *b, int64_t *n)
{
    int64_t i;

    for (i = 0; i <= *n; i++)
        a[i] = b[i];
    return 0;
}

/* the arena of the IMP compiler's free storage (src/free.imp, CORE), which has to be here for the system's constants: */
/*
 * The low segment of the tape's free storage grows in place (CORE sets its size and nothing moves), so the addresses of the arrays, and the byte pointers
 * into them, stay.  Here the address space for the arena is reserved once, without access, and CORE makes more of it usable.
 */
#ifndef MAP_ANON
#define MAP_ANON MAP_ANONYMOUS
#endif
#ifndef MAP_NORESERVE
#define MAP_NORESERVE 0
#endif
static char *core_base;
static size_t core_reserved, core_usable;

long
impcorebase(long z)
{
    size_t r = sizeof(void *) > 4 ? (size_t)1 << 34 : (size_t)1 << 28;
    /* the same place every time, so that an image of the compiler (U and V, image.c) can be put back where its pointers into the arena are good */
    void *hint = sizeof(void *) > 4 ? (void *)((size_t)1 << 44) : (void *)(size_t)0x20000000;
    void *p;

    (void)z;
    if (core_base != NULL)
        return (long)core_base;
    for (; r >= ((size_t)1 << 24); r >>= 1)
        if ((p = mmap(hint, r, PROT_NONE, MAP_PRIVATE | MAP_ANON | MAP_NORESERVE, -1, 0)) != MAP_FAILED) {
            core_base = p;
            core_reserved = r;
            return (long)core_base;
        }
    return 0;
}

/* make the first nbytes of the arena usable (zero to begin with); 1 for success */
long
impcoreset(long nbytes)
{
    size_t want = ((size_t)nbytes + 65535) & ~(size_t)65535;

    if (core_base == NULL || want > core_reserved)
        return 0;
    if (want > core_usable) {
        if (mprotect(core_base + core_usable, want - core_usable, PROT_READ | PROT_WRITE) != 0)
            return 0;
        core_usable = want;
    }
    return 1;
}

/* the arena, for the image: where it is and how much of it is usable; and, to load an image, the arena at its old place with at least nbytes usable */
void *
imp_core_info(size_t *usable)
{
    *usable = core_usable;
    return core_base;
}

int
imp_core_at(void *base, size_t nbytes)
{
    if (impcorebase(0) == 0 || (void *)core_base != base)
        return 0;
    return impcoreset((long)nbytes) != 0;
}

/* fclose for the IMP compiler: its tape routine FCLOSE (src/free.imp) is the name a CCALL of the C library's would meet */
long
impfclose(FILE *f)
{
    return fclose(f);
}

/* a call through an address, for the table of semantic routines of the IMP compiler (src/dir.imp SEMCALL): the routine is an IMP routine, which takes its
 * arguments by reference; it gets pointers to a copy of the five parameters, which it may change */
int64_t
impsemcall(void *fn, const int64_t *pars)
{
    int64_t a[5];

    memcpy(a, pars, sizeof a);
    return ((int64_t (*)(int64_t *, int64_t *, int64_t *, int64_t *, int64_t *))fn)(&a[0], &a[1], &a[2], &a[3], &a[4]);
}
