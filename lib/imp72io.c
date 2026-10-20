/*
 * imp72io.c -- PRINT and READ, the formatted input/output of the manual
 * (section 2.2.7), and FINI.
 *
 * The compiler turns a PRINT statement into a call imp_pr(n, p) for each item
 * of its list, and a READ statement into imp_rd(n, p); n is the kind of item
 * and p the address of its operand (NULL if it has none):
 *
 *   0  a value (PRINT) or a variable to read into (READ), in the current format
 *   1  OCT w    octal, zero filled
 *   2  IGR w    decimal
 *   3  STG w    a string of at most w characters (w = 0: all)
 *   5  /        end of line
 *   6  FILE     four items in a row: name, extension, project, programmer
 *   7  IMAGE MODE   words transferred as they are
 *   8  DEVICE d     'TTY:' makes the next file the terminal
 *   9  TAB n    PRINT only: next character in column n
 *  10  FILL c   PRINT only: the character that fills leading positions
 *  11  FLT m.n  a real in m columns with n digits after the point; the
 *               operand packs m (low 32 bits) and n (high 32 bits)
 *
 * A format stays until another is given, separately for PRINT and READ.  A
 * width is a number of columns; when the value does not fit, the field is as
 * wide as it needs.  The value of PRINT is the column printed to; of READ -1
 * at the end of the file, 1 past the end of a line, 0 otherwise.
 *
 * Standard output is the file PO and standard input PI.  At most four files
 * are open; FINI(-1) closes them all, FINI(0) closes them and exits.
 *
 * Differences from the PDP-10 library: words are 64 bits, so image mode moves
 * 8 bytes; a project and programmer number select nothing; a name is a
 * string of up to 7 characters (it is the bytes of the word that holds it).
 */
#include <ctype.h>
#include <stdint.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAXFILES 4

struct file {
    char name[256];
    FILE *fp;
    int out;                            /* opened for output */
};

static struct file files[MAXFILES];

/* the state of one direction */
struct dir {
    int fmt;                            /* 0 IGR (default), 1 OCT, 3 STG, 11 FLT */
    long width;
    long fltm, fltn;
    int image;
    FILE *fp;                           /* NULL: the terminal */
    char line[1024];
    int col;                            /* PRINT: characters in line; READ: position */
    int len;                            /* READ: characters in line */
    int eof;
    char devname[16];
    int tty;                            /* DEVICE 'TTY:' seen: the next file spec ends it */
    char part[4][64];                   /* FILE items so far */
    int nparts;
};

static struct dir po = { 2, 0, 0, 0, 0, NULL, "", 0, 0, 0, "", 0, { "" }, 0 };
static struct dir pi = { 2, 0, 0, 0, 0, NULL, "", 0, 0, 0, "", 0, { "" }, 0 };
static int fillch = ' ';                /* affects every output file */

static FILE *
openfile(const char *name, int out)
{
    int i, freeslot = -1;

    for (i = 0; i < MAXFILES; i++) {
        if (files[i].fp != NULL && files[i].out == out && strcmp(files[i].name, name) == 0)
            return files[i].fp;
        if (files[i].fp == NULL && freeslot < 0)
            freeslot = i;
    }
    if (freeslot < 0) {
        fprintf(stderr, "imp72: more than %d files open\n", MAXFILES);
        exit(1);
    }
    files[freeslot].fp = fopen(name, out ? "w" : "r");
    if (files[freeslot].fp == NULL) {
        fprintf(stderr, "imp72: cannot open %s\n", name);
        exit(1);
    }
    snprintf(files[freeslot].name, sizeof files[freeslot].name, "%s", name);
    files[freeslot].out = out;
    return files[freeslot].fp;
}

/* the fourth item of FILE: open the file */
static void
filespec(struct dir *d, int out)
{
    char name[256];

    snprintf(name, sizeof name, "%s%s%s", d->part[0], d->part[1][0] ? "." : "", d->part[1]);
    d->tty = 0;
    d->fp = name[0] ? openfile(name, out) : NULL;
    d->nparts = 0;
    d->col = 0;
    d->len = 0;
    d->eof = 0;
}

/* common handling of the items that mean the same in both directions */
static int
common(struct dir *d, int out, long n, int64_t *p)
{
    switch (n) {
    case 1: case 2: case 3:
        d->fmt = n == 2 ? 0 : (int)n;
        d->width = 0;
        if (p != NULL)
            d->width = (long)*p;
        d->image = 0;
        return 1;
    case 6: {
        const char *s = p == NULL ? "" : (const char *)p;

        if (d->nparts < 4) {
            if (d->nparts < 2)
                snprintf(d->part[d->nparts], sizeof d->part[0], "%.7s", s);
            else
                d->part[d->nparts][0] = '\0';
            d->nparts++;
        }
        if (d->nparts == 4)
            filespec(d, out);
        return 1;
    }
    case 7:
        d->image = 1;
        return 1;
    case 8: {
        const char *s = p == NULL ? "" : (const char *)p;

        if (strncmp(s, "TTY:", 4) == 0) {
            d->tty = 1;
            d->fp = NULL;
            d->col = 0;
            d->len = 0;
        }
        return 1;
    }
    case 11:
        d->fmt = 11;
        d->fltm = p != NULL ? (long)(int)(*p & 0xFFFFFFFFLL) : 0;
        d->fltn = p != NULL ? (long)((*p >> 32) & 0xFFFFFFFFLL) : 0;
        d->image = 0;
        return 1;
    default:
        return 0;
    }
}

/* ---- PRINT ---------------------------------------------------------------- */

static void
put(const char *s, int n)
{
    while (n-- > 0 && po.col < (int)sizeof po.line - 1)
        po.line[po.col++] = *s++;
}

static void
pad(int n, int ch)
{
    while (n-- > 0 && po.col < (int)sizeof po.line - 1)
        po.line[po.col++] = (char)ch;
}

static void
field(const char *s, long w, int zero)
{
    int n = (int)strlen(s);

    if (w > n)
        pad((int)w - n, zero ? '0' : fillch);
    put(s, n);
}

/*
 * A real, as the original's PMOD prints it.  This is the FLT part of PMOD's PRN. routine
 * (the compiled IMP, read off the original's code with DDT), instruction for instruction:
 * the number is scaled into .1 <= z < 1 by multiplying by 10^10, 10^-10 and 0.1 (the last two
 * not exact), a rounding quantity 0.5*0.1^(digits-1) is added, and each digit is the last
 * digit of IFIX(z*10) with z multiplied by 10 each time and never reduced.  All the
 * arithmetic is PDP-10 single precision (27 bit mantissa, rounded), so the inexact constants
 * and the rounding of the products show in the last digits, as they do there: 0.001 prints
 * as .99999"-3, 65536.0 with three decimals as 65535.999, and ties go either way.
 *
 *  - no 0 before the point; zero is a bare 0;
 *  - fixed notation unless the digits do not fit the field (m wide: the digits and the
 *    point must be fewer than m-1), then .ddd"e with m-7 digits (two, unpadded, when the
 *    field is narrower than 9);
 *  - a negative m is the scientific form always, in -m columns, with n+1 digits.
 */
static double
r27(double x)                                   /* a rounded floating point result */
{
    uint64_t b, mag;

    memcpy(&b, &x, sizeof b);
    mag = b & 0x7FFFFFFFFFFFFFFFULL;
    if (mag == 0 || mag >= 0x7FF0000000000000ULL)
        return x;
    mag = (mag + 0x2000000ULL) & ~0x3FFFFFFULL;
    b = (b & 0x8000000000000000ULL) | mag;
    memcpy(&x, &b, sizeof b);
    return x;
}

/* the constants of PMOD, from their words: 1.0e-10, 0.1, 1.0e10 as 27 bit floats */
static double
pdpfloat(uint64_t w)
{
    int e = (int)((w >> 27) & 0377) - 128;
    double f = (double)(w & 0x7FFFFFFULL) / 134217728.0;

    return f * (e >= 0 ? (double)(1ULL << e) : 1.0 / (double)(1ULL << -e));
}

static void
printflt(double x, long m, long n)
{
    const double c1m10 = pdpfloat(0137667633766ULL), c01 = pdpfloat(0175631463146ULL),
        c1e10 = pdpfloat(0242452013710ULL);
    char b[96], out[96];
    double zz = fabs(x), rnd;
    long d = n, g, xp, ii, j, k;
    int exp = 0, sci = 1, i, neg = x < 0, len;

    memset(b, ' ', sizeof b);
    g = m > 0 ? m - 1 : m < 0 ? -m - 1 : -1;
    if (zz == 0) {
        b[0] = '0';
        i = 0;
        goto tx;
    }
    while (zz < c01) {
        exp -= 10;
        zz = r27(c1e10 * zz);
    }
    while (zz >= c1e10) {
        exp += 10;
        zz = r27(c1m10 * zz);
    }
    while (zz >= 1.0) {
        exp++;
        zz = r27(c01 * zz);
    }
    if (m < 0)                                  /* scientific always: one more digit */
        d = n + 1, sci = 0;
    j = exp > 0 ? exp * sci : 0;
    i = (int)((d ? d + 1 : 0) + j);
    if (g >= 0 && i >= g) {                     /* it does not fit */
        sci = 0;
        d = g - 6;
        if (d <= 1) {
            d = 2;
            g = -1;
        }
        i = (int)d + 1;
    }
    if (d == 0)
        i++;
    rnd = 0.5;
    if (i > 1)
        for (ii = i - 2; ii >= 0; ii--)
            rnd = r27(c01 * rnd);
    while (r27(rnd + zz) >= 1.0)
        rnd = r27(c01 * rnd);
    zz = r27(rnd + zz);
    xp = exp * sci;
    i = -1;
    if (neg)
        b[30 - ++i] = '-';
    while (xp > 0) {
        zz = r27(zz * 10.0);
        b[30 - ++i] = (char)('0' + (long)zz % 10);
        xp--;
    }
    if (d > 0) {
        b[30 - ++i] = '.';
        for (j = d - 1; j >= 0; j--) {
            ++i;
            if (i >= g && g >= 0)
                break;
            if (xp < 0) {
                b[30 - i] = '0';
                xp++;
            } else {
                zz = r27(zz * 10.0);
                b[30 - i] = (char)('0' + (long)zz % 10);
            }
        }
    }
    j = 0;
    if (!sci) {                                 /* the exponent, before the digits in the buffer */
        j = -1;
        xp = exp < 0 ? -exp : exp;
        do {
            b[++j] = (char)('0' + xp % 10);
            xp /= 10;
        } while (xp != 0);
        if (exp < 0)
            b[++j] = '-';
        b[++j] = '"';
        j++;
    }
    for (k = 0; k <= i; k++)
        b[k + j] = b[30 - i + k];
    i += (int)j;
tx:
    /* PMOD pads while the width g exceeds the index i of the last character, then writes
     * b[i] down to b[0] with a loop that runs at least once: with nothing to write (i = -1,
     * a fraction printed with no decimals) that is the word before the buffer, a blank. */
    for (k = g; k > i; k--)
        pad(1, fillch);
    len = 0;
    k = i;
    do
        out[len++] = k >= 0 ? b[k] : ' ';
    while (--k >= 0);
    out[len] = '\0';
    put(out, len);
}

/* where PRINT goes when it has no file: stdout, unless the program has said otherwise (the compiler's
 * own IMP modules print to its listing: stage0/impglue.c sets the hook) */
FILE *(*imp_defout)(void);

static FILE *
defout(void)
{
    return imp_defout != NULL ? imp_defout() : stdout;
}

long
imp_pr(long n, int64_t *p)
{
    char buf[64];

    if (po.tty)
        po.fp = NULL;
    if (n == 10) {
        fillch = (p != NULL && (*p & 0xFF)) ? (int)(*p & 0xFF) : ' ';
        return po.col;
    }
    if (n == 5) {
        FILE *f = (po.fp != NULL ? po.fp : defout());

        if (po.image) {
            po.col = 0;
            return 0;
        }
        po.line[po.col] = '\0';
        fputs(po.line, f);
        fputc('\n', f);
        po.col = 0;
        return 0;
    }
    if (n == 9) {
        if (p != NULL && *p - 1 > po.col)
            pad((int)(*p - 1 - po.col), ' ');
        return po.col;
    }
    if (common(&po, 1, n, p))
        return po.col;
    if (n != 0 || p == NULL)
        return po.col;
    if (po.image) {                     /* a word, as it is */
        fwrite(p, 8, 1, (po.fp != NULL ? po.fp : defout()));
        return po.col;
    }
    switch (po.fmt) {
    case 3: {
        const char *s = (const char *)p;
        int len = (int)strlen(s);

        if (po.width > 0 && len > po.width)
            len = (int)po.width;
        if (po.width > len)
            pad((int)po.width - len, fillch);
        put(s, len);
        break;
    }
    case 1:
        snprintf(buf, sizeof buf, "%llo", (unsigned long long)*p);
        field(buf, po.width, 1);
        break;
    case 11: {
        double x;
        int64_t e = *p, s = e >> 63, bits = ((e ^ s) - s) | (s & (int64_t)0x8000000000000000ULL);

        memcpy(&x, &bits, sizeof x);
        printflt(x, po.fltm, po.fltn);
        break;
    }
    default:
        snprintf(buf, sizeof buf, "%lld", (long long)*p);
        field(buf, po.width, 0);
        break;
    }
    return po.col;
}

/* ---- READ ----------------------------------------------------------------- */

/* make sure a line is there; 0 at the end of the file */
static int
getline_(struct dir *d)
{
    FILE *f = d->fp != NULL ? d->fp : stdin;
    int c, n = 0;

    if (d->col < d->len)
        return 1;
    if (d->eof)
        return 0;
    while ((c = getc(f)) != EOF && c != '\n') {
        if (c == '\t')
            c = ' ';
        if (n < (int)sizeof d->line - 1)
            d->line[n++] = (char)c;
    }
    if (c == EOF && n == 0) {
        d->eof = 1;
        return 0;
    }
    d->line[n] = '\0';
    d->len = n;
    d->col = 0;
    return 1;
}

/* the next input field: up to the terminator, the end of the line or 128
 * characters; blanks are ignored unless keep is set; returns 1 past the end
 * of the line */
static int
nextfield(struct dir *d, int term, int fixed, int keep, char *out, int *past)
{
    int n = 0, start = d->col;

    *past = 0;
    if (d->col >= d->len) {
        *past = 1;
        out[0] = '\0';
        return 0;
    }
    while (d->col < d->len && n < 128) {
        int c = (unsigned char)d->line[d->col];

        if (fixed && d->col - start >= fixed)
            break;
        if (!fixed && c == term) {
            d->col++;
            break;
        }
        d->col++;
        if (c == ' ' && !keep && term != ' ')
            continue;
        out[n++] = (char)c;
    }
    out[n] = '\0';
    return n;
}

long
imp_rd(long n, int64_t *p)
{
    char buf[200];
    int past = 0;
    int term = ',', fixed = 0;

    if (pi.tty)
        pi.fp = NULL;
    if (n == 5) {                       /* skip the rest of the line */
        pi.col = pi.len;
        return 0;
    }
    if (n == 9 || n == 10)
        return 0;
    if (common(&pi, 0, n, p))
        return 0;
    if (n != 0 || p == NULL)
        return 0;
    if (pi.image) {
        if (fread(p, 8, 1, pi.fp != NULL ? pi.fp : stdin) != 1)
            return -1;
        return 0;
    }
    if (!getline_(&pi))
        return -1;
    /* the "width" of a field read is its terminator when it is a one
     * character string (a printable code); a small number is a width */
    if (pi.width >= 32 && pi.width < 127)
        term = (int)pi.width;
    else if (pi.width > 0 && pi.width < 32)
        fixed = (int)pi.width;
    switch (pi.fmt) {
    case 3: {
        int k = nextfield(&pi, term, fixed, 1, buf, &past);

        memcpy(p, buf, (size_t)k + 1);  /* the caller provides room for the field */
        break;
    }
    case 1:
        nextfield(&pi, term, fixed, 0, buf, &past);
        *p = (int64_t)strtoull(buf, NULL, 8);
        break;
    case 11:                            /* on input, FLT m.n is IGR m */
    default:
        nextfield(&pi, term, fixed, 0, buf, &past);
        *p = (int64_t)strtoll(buf, NULL, 10);
        break;
    }
    return past;
}

/* ---- FINI ----------------------------------------------------------------- */

/* FINI(I): close the files; I = 0 exits. */
long
fini(int64_t *p)
{
    int i;

    for (i = 0; i < MAXFILES; i++)
        if (files[i].fp != NULL) {
            fclose(files[i].fp);
            files[i].fp = NULL;
        }
    po.fp = pi.fp = NULL;
    fflush(stdout);
    if (p != NULL && *p == 0)
        exit(0);
    return 0;
}
