/*
 * dir_imp.c -- dir.c's interface, over the core of src/dir.imp (HASH, DIR, DSEQ, DNAME)
 * compiled by imp72stage0.  The rest stays here.  Ports decus/dir.imc: DIR, DSEQ, PNAME,
 * DNAME, GNAME, DSTATS, DSPILL, RADI50, DORGFIX, CONVC, STCON, NEWNAME.
 *
 * A hash coded directory.  Free array PRP is a hash table of 512 buckets of
 * three word nodes (indices 3*h), followed by overflow nodes added at NPRP.
 * A name's "directory index" is the index in PRP of its node, so it is
 * the same number the PDP-10 compiler computes: the hash below is the
 * original's, taken over the first five characters packed as they were.
 *
 *   node word 0   HL: sibling (next node with a colliding name)  HR: ALF index
 *   node word 1   HL: 'ENTRY'  HR: 'NODE'
 *   node word 2   HR: 'SEM';  above it the small fields of dprop()
 *
 * Free array ALF holds the names themselves, concatenated: five 7-bit
 * characters per word, left justified in the low 36 bits, with a NUL
 * terminator that costs a word when the length is a multiple of five.
 */
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "imp.h"

extern word prp, alf;               /* PRP, ALF: defined by the IMP module */
extern word dm, dn;                 /* DM, DN: buckets used; overflow nodes */
extern word impl_hash(word *), impl_dir(word *), impl_dseq(word *), impl_dname(word *, word *);

word convc_const;                   /* CONST */

static word newnum;                 /* NEWNUM */

/* Pack up to five characters, left justified in 36 bits as on the PDP-10. */
static word
pack5(const char *s, size_t n)
{
    word w = 0;
    size_t i;

    for (i = 0; i < 5 && i < n; i++)
        w |= (word)((unsigned char)s[i] & 0x7F) << (29 - 7 * i);
    return w;
}

/* DIR(NM): the directory index of a name, entering it if new. */
word
dir(const char *nm)
{
    static word *pk;
    static size_t pkcap;
    size_t len = strlen(nm), nw = len / 5 + 1, c;

    if (nw > pkcap) {
        word *t = realloc(pk, nw * sizeof *pk);
        if (t == NULL) {
            imp_error(0, "OUT OF MEMORY IN DIR. ");
            return 0;
        }
        pk = t;
        pkcap = nw;
    }
    for (c = 0; c < nw; c++)
        pk[c] = pack5(nm + 5 * c, len > 5 * c ? len - 5 * c : 0);
    return impl_dir(pk);
}

/* DNAME(I,J): the jth word (five characters) of the name of entry i. */
word
dname(word i, word j)
{
    return impl_dname(&i, &j);
}

/* DSEQ(N): step through all entries.  Start with N < 0; ends with N < 0. */
word
dseq(word n)
{
    return impl_dseq(&n);
}

/* hash(): the 9 bit hash of a packed word, for DSPILL. */
static unsigned
hash(word nm)
{
    return (unsigned)impl_hash(&nm);
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
        for (c = 0; c < 5; c++) {
            char ch = (char)((w >> (29 - 7 * c)) & 0x7F);
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
 * word 0 is N and words 1..N the string, five characters per word without
 * its opening quote (convc_const is the first word).
 *
 * Numbers: decimal; `dddB` octal; `dddBr` in radix r (letters are digits
 * above 9).  The original's constants are 36 bit; a twelve digit octal
 * constant with the top bit set is read as the negative 36 bit number it is
 * there, so that -1 is still 777777777777B.
 */
word
convc(word c)
{
    word nc = 0, bas = -1, cc = 0, w, ww, ch, n, j, b, i, k, ncfr, val = 0;

    if (!cinit) {
        cinit = 1;
        frelot(&cbuf, "CONVC", 25, NULL);
    }
    w = dname(c, cc);
    if ((w >> 29) == 047) {             /* a string: the multiword case */
        if (!jnit) {
            jnit = 1;
            cfr = fallot_new("CONST", 10);
        }
        ncfr = 1;
        for (;;) {
            ww = (w << 7) & MASK36;
            if (w & 0377) {
                cc++;
                w = dname(c, cc);
                ww |= 0376 & (w >> 28);
            }
            fadd(&cfr, &ncfr, ww);
            if (!(ww & 0377))
                break;
        }
        FREES(cfr, ncfr - 1);
        convc_const = FREE(cfr + 1);
        return cfr;
    }
    ww = w >> 29;
    if (ww < 060 || ww > 071)
        return 0;
    for (;;) {                          /* pick the number apart */
        for (n = 29; n >= 1; n -= 7) {
            ch = 0177 & (w >> n);
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
        w = dname(c, cc);
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
    convc_const = 0;
    for (i = 0; i <= j; i++) {
        if (b == 8)
            convc_const <<= 3;
        else
            convc_const *= b;
        k = FREE(cbuf + i) - 060;
        if (k >= b)
            return 0;
        convc_const += k;
    }
    if (b == 8 && j == 11 && (convc_const & ((word)1 << 35)))
        convc_const -= (word)1 << 36;
    val = -1;
    return val;
}

/* STCON(V): directory entry for the constant V, named in octal. */
word
stcon(word v)
{
    char buf[40];
    uword u;

    if (v < 0 && v >= -((word)1 << 35))
        u = (uword)v & (uword)MASK36;
    else
        u = (uword)v;
    if (u == 0)
        return dir("0");
    /* as in dir.c: a leading 0 keeps a positive number with bit 35 set from being read back as negative */
    snprintf(buf, sizeof buf, (v > 0 && u >> 35 && !(u >> 36)) ? "0%lloB" : "%lloB", (unsigned long long)u);
    return dir(buf);
}

/* NEWNAME(I): a new, unique name: % + the first four characters of the
 * name s + a serial number. */
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
