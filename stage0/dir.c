/*
 * dir.c -- the directory of names.  Ports src/dir.imc (decus/dir.imc: DIR, DSEQ, PNAME,
 * DNAME, GNAME, DSTATS, DSPILL, RADI50, DORGFIX, CONVC, STCON, NEWNAME).  The 36 bit
 * formats of the tape are gone: the words are those of src/dir.imc and src/impsyn.imc.
 *
 * A hash coded directory.  Free array PRP is a hash table of 512 buckets of
 * three word nodes (indices 3*h), followed by overflow nodes added at NPRP.
 * A name's "directory index" is the index in PRP of its node, so it is
 * the same number the stage compiler computes: the hash below is that of
 * src/dir.imc, taken over the first word of the name (eight characters).
 *
 *   node word 0   HL: sibling (next node with a colliding name)  HR: ALF index
 *   node word 1   HL: 'ENTRY'  HR: 'NODE'
 *   node word 2   HR: 'SEM';  above it the small fields of dprop()
 *                 (ATYPE 32-34, COM 35-36, ISNM 37-38, LXTY 39-40, USE 41-43)
 *
 * Free array ALF holds the names themselves, concatenated: eight characters
 * (bytes) to a word, the first in the LOW byte, with a NUL terminator that
 * costs a word when the length is a multiple of eight.
 */
#include <stdlib.h>
#include <string.h>
#include "imp.h"

word prp, alf;                      /* PRP, ALF */
word convc_const;                   /* CONST */

static int dinit;                   /* DINIT */
static word nprp, nalf;             /* NPRP, NALF */
static long dm, dn;                 /* DM, DN: buckets used; overflow nodes */
static word nmark = -1;             /* NMARK (DSEQ) */
static word newnum;                 /* NEWNUM */

/* DORGFIX: recompute pointers derived from PRP.  Here nothing is derived
 * (dprop() reads through `prp`), so it does nothing. */
static void
dorgfix(void)
{
}

/* Pack up to eight characters, the first in the low byte, as src/dir.imc. */
static word
pack8(const char *s, size_t n)
{
    uword w = 0;
    size_t i;

    for (i = 0; i < 8 && i < n; i++)
        w |= (uword)(unsigned char)s[i] << (8 * i);
    return (word)w;
}

/* HASH(NM): the 9 bit hash of the first packed word:
 * NM xor NM RS 32, xor RS 16, xor RS 9, and 777B. */
static unsigned
hash(word nm)
{
    uword h = (uword)nm ^ ((uword)nm >> 32);

    h = h ^ (h >> 16);
    h = h ^ (h >> 9);
    return (unsigned)(h & 0777);
}

/* DIR(NM): the directory index of a name, entering it if new. */
word
dir(const char *nm)
{
    static word *pk;
    static size_t pkcap;
    size_t len = strlen(nm), nw = len / 8 + 1, c;
    word h, hn, i, i1, q, j, alfx, k;
    unsigned char *p;
    long n;

    if (!dinit) {
        dinit = 1;
        frelot(&prp, "PRP", 2100, dorgfix);
        fincset(prp, 90);
        nprp = DIR_HASHROWS;
        frelot(&alf, "ALF", 900, dorgfix);
        fincset(alf, 45);
        nalf = 1;
    }
    if (nw > pkcap) {
        word *t = realloc(pk, nw * sizeof *pk);
        if (t == NULL) {
            imp_error(0, "OUT OF MEMORY IN DIR. ");
            return 0;
        }
        pk = t;
        pkcap = nw;
    }
    p = (unsigned char *)nm;
    for (c = 0; c < nw; c++)
        pk[c] = pack8((const char *)p + 8 * c, len > 8 * c ? len - 8 * c : 0);
    n = (long)nw - 1;                   /* last word of the name */

    h = hash(pk[0]);
    hn = 2;
l1: i = h * 3;
    if (i == 0)
        i = 3;
l2: q = FREE(prp + i);
    j = HR(q);
    if (j == 0) {                       /* empty bucket: take it */
        FREES(prp + i, nalf);
        dm++;
        goto l4;
    }
    alfx = alf + j;
    for (k = 0; k <= n; k++) {
        if (pk[k] != FREE(alfx + k)) {
            if (hn) {                   /* try the two alternate buckets */
                h ^= hn;
                hn >>= 1;
                goto l1;
            }
            i1 = i;
            i = HL(q);
            if (i)
                goto l2;                /* follow the sibling chain */
            goto l3;
        }
    }
    return i;                           /* found */

l3: i = nprp;                           /* chain a new overflow node */
    dn++;
    FREES(prp + i1, q | HW(i, 0));
    k = nprp + 3;
    if (flen(prp) <= k)
        faddex(&prp, k);
    FREES(prp + nprp, nalf);
    nprp = k;
l4: k = nalf + n + 1;
    if (flen(alf) <= k)
        faddex(&alf, k);
    alfx = alf + nalf;
    nalf = k;
    for (k = 0; k <= n; k++)
        FREES(alfx + k, pk[k]);
    return i;
}

/* DNAME(I,J): the jth word (eight characters) of the name of entry i. */
word
dname(word i, word j)
{
    return FREE(alf + (j + HR(FREE(prp + i))));
}

/* The whole name of entry i as a C string (valid until the next call). */
static const char *
fullname(word i, size_t limit)
{
    static char *buf;
    static size_t cap;
    word j, w;
    size_t n = 0;
    int c;

    for (j = 0;; j++) {
        w = dname(i, j);
        for (c = 0; c < 8; c++) {
            char ch = (char)(((uword)w >> (8 * c)) & 0xFF);
            if (ch == '\0' || n >= limit)
                goto done;
            if (n + 2 > cap) {
                char *t = realloc(buf, cap = cap ? cap * 2 : 64);
                if (t == NULL)
                    goto done;
                buf = t;
            }
            buf[n++] = ch;
        }
    }
done:
    if (buf == NULL && (buf = malloc(cap = 64)) == NULL)
        return "";
    buf[n] = '\0';
    return buf;
}

/* DNAME_STR(I,BUF,N): the whole name of entry i, up to n-1 characters. */
void
dname_str(word i, char *buf, size_t n)
{
    const char *s = fullname(i, n - 1);

    strncpy(buf, s, n - 1);
    buf[n - 1] = '\0';
}

/* GNAME(N): the first 24 characters of the name. */
const char *
gname(word i)
{
    return fullname(i, 24);
}

/* PNAME(N): print the name on the listing. */
void
pname(word i)
{
    if (i >= 0)
        fputs(fullname(i, (size_t)-1), imp_out());
}

/* DSEQ(N): step through all entries.  Start with N < 0; ends with N < 0. */
word
dseq(word n)
{
    if (n < 0) {
        n = -3;
        nmark = -1;
    }
    if (nmark >= 0) {
        n = HL(FREE(prp + n));
        if (n == 0) {
            n = nmark;
            nmark = -1;
        }
    }
    if (nmark < 0) {
        for (;;) {
            n += 3;
            if (n >= DIR_HASHROWS) {
                n = -1;
                break;
            }
            if (FREE(prp + n) != 0) {
                nmark = n;
                break;
            }
        }
    }
    return n;
}

void
dspill(void)
{
    FILE *o = imp_out();
    word n = -1, h1 = -1, h;

    fprintf(o, "HASH CODE   TOKEN\n");
    for (;;) {
        n = dseq(n);
        if (n < 0)
            break;
        h = hash(dname(n, 0));
        if (h != h1) {
            fprintf(o, "    %03lo", (long)h);
            h1 = h;
        } else
            fprintf(o, "       ");
        fprintf(o, "    ");
        pname(n);
        fputc('\n', o);
    }
}

void
dstats(void)
{
    fprintf(imp_out(), "DIRECTORY HASHED INTO %ld BOXES.  OVERFLOW IS %ld.\n",
        dm, dn);
    if (finam5 & SW_DSPILL)
        dspill();
}

/* RADI50(N): RADIX50 value of the first six characters of the name. */
word
radi50(word i)
{
    const char *s = fullname(i, 6);
    word val = 0;
    int k;

    for (; *s != '\0'; s++) {
        int c = (unsigned char)*s;

        if (c == '.')
            k = 37;
        else if (c == '$')
            k = 38;
        else if (c == '%')
            k = 39;
        else if (c > 0132)
            continue;
        else if (c > 0100)
            k = c - 066;
        else if (c > 071)
            continue;
        else if ((k = c - 057) <= 0)
            continue;
        val = val * 40 + k;
    }
    return val;
}

/* ---- node properties (DPROP and DPROPS of impsyn.imc) ------------------ */

/* Small fields of node word 2, above the SEM halfword. */
static const struct { int shift, width; } fld[] = {
    [DP_ATYPE] = { 32, 3 }, [DP_COM] = { 35, 2 }, [DP_ISNM] = { 37, 2 },
    [DP_LXTY] = { 39, 2 },  [DP_USE] = { 41, 3 },
};

word
dprop(enum dprop p, word i)
{
    switch (p) {
    case DP_NAME:  return alf + HR(FREE(prp + i));    /* pointer to the name */
    case DP_ENTRY: return HL(FREE(prp + i + 1));
    case DP_NODE:  return HR(FREE(prp + i + 1));
    case DP_SEM:   return HR(FREE(prp + i + 2));
    default:
        return (word)(((uword)FREE(prp + i + 2) >> fld[p].shift)
            & (((uword)1 << fld[p].width) - 1));
    }
}

void
dprops(enum dprop p, word i, word v)
{
    word w;
    uword m;

    switch (p) {
    case DP_NAME:
        imp_error(0, "DPROPS: THE NAME PROPERTY IS READ ONLY. ");
        break;
    case DP_ENTRY:
        w = FREE(prp + i + 1);
        FREES(prp + i + 1, HW(v, HR(w)));
        break;
    case DP_NODE:
        w = FREE(prp + i + 1);
        FREES(prp + i + 1, HW(HL(w), v));
        break;
    case DP_SEM:
        w = FREE(prp + i + 2);
        FREES(prp + i + 2, (word)(((uword)w & ~(uword)HW_MASK)
            | ((uword)v & HW_MASK)));
        break;
    default:
        m = (((uword)1 << fld[p].width) - 1) << fld[p].shift;
        w = FREE(prp + i + 2);
        FREES(prp + i + 2, (word)(((uword)w & ~m)
            | (((uword)v << fld[p].shift) & m)));
        break;
    }
}

/* ---- constants --------------------------------------------------------- */

static word cbuf;                   /* CBUF: the digits of a number */
static word cfr;                    /* CFR: string constant words */
static int cinit, jnit;

/*
 * CONVC(C): convert the constant named by directory entry C.
 * Returns 0 if it is not a constant; -1 if numeric, the value being left in
 * convc_const; or, for a string constant, a pointer to a free array whose
 * word 0 is N and words 1..N the string, eight characters per word without
 * its opening quote (convc_const is the first word).
 *
 * Numbers: decimal; `dddB` octal; `dddBr` in radix r (letters are digits
 * above 9).  The value is a 64 bit number that wraps (the 36 bit wrap of the
 * tape is gone): 777777777777B is 68719476735, and -1 is written -1.
 */
/* The words of a text constant are in memory order, as in src/dir.imc (its CHR0, REST and FRONTLAST are those of src/impend-le.imc or impend-be.imc): the first character is
 * the first byte of the word in memory, the low byte on a little endian machine and the high byte on a big endian one.  The names of the directory stay first-character-low
 * inside this program, so the words of a constant are swapped on the way out where the machine is big endian. */
int tgtswap;                        /* 1 when the byte order of the target is not the host's (TGTSWAP of src/dir.imc); the seed compiles for its host, so it stays 0 */

static uword
memorder(uword w)
{
    unsigned x = 1;
    int be = (*(unsigned char *)&x == 0) ^ (tgtswap != 0);      /* the byte order of the target: the host's, or the other */

    if (be) {
        uword r = 0;
        int i;

        for (i = 0; i < 8; i++, w >>= 8)
            r = (r << 8) | (w & 0377);
        return r;
    }
    return w;
}

word
convc(word c)
{
    word nc = 0, bas = -1, cc = 0, ww, ch, n, j, b, i, k, ncfr, val = 0;
    uword w;
    uword cv;

    if (!cinit) {
        cinit = 1;
        frelot(&cbuf, "CONVC", 25, NULL);
    }
    w = (uword)dname(c, cc);
    if ((w & 0377) == 047) {            /* a string: the multiword case */
        if (!jnit) {
            jnit = 1;
            cfr = fallot_new("CONST", 10);
        }
        ncfr = 1;
        for (;;) {
            uword wd = w >> 8;          /* REST(W) */

            if (w >> 56) {              /* LASTC(W): the name goes on */
                cc++;
                w = (uword)dname(c, cc);
                wd |= (w & 0377) << 56; /* FRONTLAST(W) */
            }
            fadd(&cfr, &ncfr, (word)memorder(wd));
            if (!(wd >> 56))
                break;
        }
        FREES(cfr, ncfr - 1);
        convc_const = FREE(cfr + 1);
        return cfr;
    }
    ww = (word)(w & 0377);
    if (ww < 060 || ww > 071)
        return 0;
    for (;;) {                          /* pick the number apart */
        for (n = 0; n <= 56; n += 8) {
            ch = (word)((w >> n) & 0377);
            if (ch == 0)
                goto t3;
            if (ch < 060 || ch > 071) {
                ch &= 0137;
                if (ch == 0102) {       /* B */
                    bas = nc;
                } else {
                    bas = -1;
                    if (ch > 0100 && ch <= 0132)
                        ch -= 7;
                    else
                        return 0;
                }
            }
            fadd(&cbuf, &nc, ch);
        }
        cc++;
        w = (uword)dname(c, cc);
    }
t3: j = nc - 1;
    b = 10;
    if (bas > 0) {
        if (bas == j) {
            j--;
            b = 8;
        } else {
            b = 0;
            for (i = bas + 1; i <= j; i++) {
                k = FREE(cbuf + i);
                b = (k - 060) + 10 * b;
            }
            j = bas - 1;
        }
    }
    cv = 0;
    for (i = 0; i <= j; i++) {
        if (b == 8)
            cv <<= 3;
        else
            cv *= (uword)b;
        k = FREE(cbuf + i) - 060;
        if (k >= b) {
            convc_const = (word)cv;
            return 0;
        }
        cv += (uword)k;
    }
    convc_const = (word)cv;
    val = -1;
    return val;
}

/* STCON(V): directory entry for the constant V, named by its octal digits as an unsigned 64 bit number
 * (up to 22, without the leading zeros) and a B. */
word
stcon(word v)
{
    char buf[40];

    snprintf(buf, sizeof buf, "%lloB", (unsigned long long)(uword)v);
    return dir(buf);
}

/* NEWNAME(I): a new, unique name: % + the first four characters of the
 * name s + a serial number (decimal). */
word
newname(const char *s)
{
    char buf[40];
    size_t n = strlen(s);

    newnum++;
    snprintf(buf, sizeof buf, "%%%.*s%ld", n < 4 ? (int)n : 4, s,
        (long)newnum);
    return dir(buf);
}
