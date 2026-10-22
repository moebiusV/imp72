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

/* Add one in the last place of the decimal digit string s (digits only); returns 1 if it grew a digit. */
static int
incdec(char *s, int len)
{
    int k;

    for (k = len - 1; k >= 0; k--) {
        if (s[k] == '9')
            s[k] = '0';
        else {
            s[k]++;
            return 0;
        }
    }
    memmove(s + 1, s, (size_t)len + 1);
    s[0] = '1';
    return 1;
}

/*
 * The double zz written with d digits after the point, rounded half away from zero on its exact decimal value
 * (printf rounds a tie to even; PMOD and FORTRAN round it up): "iii.ddd", the integer part "0" if it is zero.
 */
static void
fixeddigits(double zz, long d, char *out, size_t cap)
{
    char t[1400], dig[1400];
    char *dot;
    int ilen, len, nx;

    snprintf(t, sizeof t, "%.*f", (int)d + 40, zz);
    dot = strchr(t, '.');
    ilen = (int)(dot - t);
    memcpy(dig, t, (size_t)ilen);
    memcpy(dig + ilen, dot + 1, (size_t)d);
    len = ilen + (int)d;
    dig[len] = '\0';
    nx = dot[1 + d];
    if (nx >= '5')
        ilen += incdec(dig, len);
    snprintf(out, cap, "%.*s%s%s", ilen, dig, d ? "." : "", d ? dig + ilen : "");
}

/* The same in the form D.DDDe+X with d significant digits: digits in dg (d of them), the exponent of 0.DDD in *ex. */
static void
scidigits(double zz, long d, char *dg, int *ex)
{
    char t[1400];
    char *e;
    int k, nx;

    snprintf(t, sizeof t, "%.*e", (int)d - 1 + 40, zz);
    e = strchr(t, 'e');
    *ex = atoi(e + 1) + 1;
    dg[0] = t[0];
    for (k = 1; k < d; k++)
        dg[k] = t[1 + k];
    dg[d] = '\0';
    nx = t[1 + d];
    if (nx >= '5' && incdec(dg, (int)d)) {
        dg[d] = '\0';
        (*ex)++;
    }
}

/*
 * A real, as PRINT FLT m.n writes it.  The layout is that of the original's PMOD (docs/internals/pmod.md); the
 * digits are those of the IEEE 754 double, correctly rounded (the original computed them in 27 bit floating
 * point, with inexact constants, so that 0.001 printed as .99999"-3):
 *
 *  - no 0 before the point; zero is a bare 0;
 *  - fixed notation (n digits after the point) unless the digits and the point do not fit the field (m wide:
 *    they must be fewer than m-1), then .ddd"e with m-7 digits (two, unpadded, when the field is narrower
 *    than 9), where e is the power of ten of the number written as 0.ddd;
 *  - a negative m is the scientific form always, in -m columns, with n+1 digits;
 *  - a number that is not finite is INF or NAN.
 */
static void
printflt(double x, long m, long n)
{
    char body[512], num[512], *p;
    double zz = fabs(x);
    long d = n, g = m > 0 ? m - 1 : m < 0 ? -m - 1 : -1;
    int fixed = m >= 0, neg = x < 0, len = 0, ex;

    if (d > 60)
        d = 60;
    if (m < 0)
        d = n + 1;
    if (x != x || zz > 1.7976931348623157e308) {
        strcpy(body, x != x ? "NAN" : neg ? "-INF" : "INF");
        len = (int)strlen(body);
        goto tx;
    }
    if (zz == 0) {
        strcpy(body, "0");
        len = 1;
        goto tx;
    }
    if (fixed) {
        int j = 0;

        fixeddigits(zz, d, num, sizeof num);
        p = num;
        if (p[0] == '0' && p[1] == '.')
            p++;
        while (p[j] >= '0' && p[j] <= '9')
            j++;
        if (g >= 0 && (d ? d + 1 : 0) + j >= g) {       /* it does not fit */
            fixed = 0;
            d = g - 6;
            if (d <= 1) {
                d = 2;
                g = -1;
            }
        } else {
            len = snprintf(body, sizeof body, "%s%s", neg ? "-" : "", p);
            goto tx;
        }
    }
    if (d < 1)
        d = 1;
    scidigits(zz, d, num, &ex);
    len = snprintf(body, sizeof body, "%s.%s\"%d", neg ? "-" : "", num, ex);
tx:
    if (g >= 0 && g + 1 > len)
        pad((int)(g + 1 - len), fillch);
    put(body, len);
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
