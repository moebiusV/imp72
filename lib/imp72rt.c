/*
 * imp72rt.c -- the rest of the runtime library of IMP72 programs: the
 * monitor calls the compiler's own sources make (CALLI), BLT and IFIX.  PRINT,
 * READ and FINI are in imp72io.c.
 */
#include <stdint.h>
#include <stdio.h>
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
 * EXECUTE (the PDP-10's XCT): imp_xct(w) does the instruction in the word w, in the 36 bit format (opcode 9 bits at 27, AC at 23,
 * indirect at 22, index at 18, address in the right 18).  The accumulators are the words of imp_ac, which the compiled code fills
 * from its registers before the call and reads back after.  An address is as in compiled code: the index register holds a byte
 * address and the address field counts words from it (without an index register the address is a number).  Done: MOVE, MOVEI, MOVEM,
 * MOVES, MOVN, MOVM, EXCH, ASH, LSH, ADD, SUB, the boolean group, the halfword group (halves are 32 bits), CALLI, TTCALL (OUTCHR,
 * OUTSTR) and RELEAS; any other instruction is an error, for the PDP-10's other instructions work on its 18 bit addresses.
 */
int64_t imp_ac[16];

/* one of the halfword instructions (500 to 577) on the words src and dst: bit 2 of the opcode crosses the halves, bit 5 says the
 * destination is the right half, bits 3-4 what happens to the other half (kept, zero, ones, the sign of the moved half) */
static int64_t
xhalf(unsigned op, int64_t src, int64_t dst)
{
    uint64_t m = 0xFFFFFFFFu, s = (uint64_t)src, d = (uint64_t)dst, h, other;
    int fromleft = ((op >> 2) & 1) == ((op >> 5) & 1), toright = (op >> 5) & 1;

    h = fromleft ? (s >> 32) & m : s & m;
    switch ((op >> 3) & 3) {
    case 0: other = toright ? d & ~m : d & m; break;
    case 1: other = 0; break;
    case 2: other = toright ? ~m : m; break;
    default: other = (h & 0x80000000u) ? (toright ? ~m : m) : 0; break;
    }
    if (toright)
        return (int64_t)((other & ~m) | h);
    return (int64_t)((other & m) | (h << 32));
}

void
imp_xct(int64_t w)
{
    unsigned op = (unsigned)((w >> 27) & 0777), a = (unsigned)((w >> 23) & 15), x = (unsigned)((w >> 18) & 15);
    int64_t y = w & 0777777, e, *m, v, r;

    if (w & (1 << 22)) {
        fprintf(stderr, "EXECUTE: indirect instruction words are not supported\n");
        exit(1);
    }
    if (y & 0400000)
        y -= 01000000;
    e = y + (x ? imp_ac[x] : 0);                        /* the effective address as a number */
    m = (int64_t *)(intptr_t)(x ? imp_ac[x] + 8 * y : 8 * y);   /* and as the address of a word */
    if (op == 047) {                                    /* CALLI */
        imp_ac[a] = imp_calli((long)e, (long)imp_ac[a]);
        return;
    }
    if (op == 051) {                                    /* TTCALL */
        if (a == 1)
            putchar((int)(*m & 0177));
        else if (a == 3)
            fputs((const char *)m, stdout);
        return;
    }
    if (op == 071)                                      /* RELEAS */
        return;
    if (op >= 0200 && op < 0220) {                      /* MOVE, MOVS, MOVN, MOVM and their modes */
        int fn = (op >> 2) & 3, mode = op & 3;

        if (fn == 1) {
            fprintf(stderr, "EXECUTE: MOVS is not supported\n");
            exit(1);
        }
        v = mode == 1 ? e : (mode == 2 ? imp_ac[a] : *m);
        if (fn == 2)
            v = -v;
        else if (fn == 3 && v < 0)
            v = -v;
        if (mode == 2)
            *m = v;
        else if (mode == 3) {
            *m = v;
            if (a)
                imp_ac[a] = v;
        } else
            imp_ac[a] = v;
        return;
    }
    if (op == 0250) {                                   /* EXCH */
        v = *m;
        *m = imp_ac[a];
        imp_ac[a] = v;
        return;
    }
    if (op == 0240 || op == 0242) {                     /* ASH, LSH */
        r = (int)e;
        if (r >= 64 || r <= -64)
            imp_ac[a] = 0;
        else if (op == 0240)
            imp_ac[a] = r >= 0 ? (int64_t)((uint64_t)imp_ac[a] << r) : imp_ac[a] >> -r;
        else
            imp_ac[a] = r >= 0 ? (int64_t)((uint64_t)imp_ac[a] << r) : (int64_t)((uint64_t)imp_ac[a] >> -r);
        return;
    }
    if (op >= 0270 && op < 0300) {                      /* ADD, SUB */
        int mode = op & 3, sub = op >= 0274;
        int64_t b = mode == 1 ? e : *m;

        r = sub ? imp_ac[a] - b : imp_ac[a] + b;
        if (mode == 2 || mode == 3)
            *m = r;
        if (mode != 2)
            imp_ac[a] = r;
        return;
    }
    if (op >= 0400 && op < 0500) {                      /* the boolean group: 16 functions in 4 modes */
        int f = (op - 0400) >> 2, mode = op & 3;
        int64_t ac = imp_ac[a], b = mode == 1 ? e : (mode == 2 ? ac : *m), q = mode == 2 ? *m : ac;

        if (mode == 2)
            ac = *m, b = imp_ac[a];
        else
            q = ac;
        r = 0;
        if (f & 1)
            r |= q & b;
        if (f & 2)
            r |= ~q & b;
        if (f & 4)
            r |= q & ~b;
        if (f & 8)
            r |= ~q & ~b;
        if (mode == 2 || mode == 3)
            *m = r;
        if (mode != 2)
            imp_ac[a] = r;
        return;
    }
    if (op >= 0500 && op < 0600) {                      /* the halfword group */
        int mode = op & 3;
        int64_t src = mode == 1 ? e & 0xFFFFFFFF : (mode == 2 ? imp_ac[a] : *m);
        int64_t dst = mode == 2 ? *m : imp_ac[a];
        int64_t out;

        out = xhalf(op, src, dst);
        if (mode == 2 || mode == 3)
            *m = out;
        if (mode != 2)
            imp_ac[a] = out;
        return;
    }
    fprintf(stderr, "EXECUTE: the instruction %o is not supported\n", op);
    exit(1);
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

/* MOVE B THROUGH N TO A: N+1 words from b to a, ascending, as BLT does (the arguments are pointers) */
long
imp_blk(int64_t *a, int64_t *b, int64_t *n)
{
    int64_t i;

    for (i = 0; i <= *n; i++)
        a[i] = b[i];
    return 0;
}

