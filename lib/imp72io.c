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
 * A real, as the original's PMOD prints it (read off the original running: docs/ORACLE.md):
 *
 *  - no 0 before the point: .5, -.25; with n = 0 no point, and nothing at all for an
 *    integer part of 0 (just the sign of a negative number);
 *  - digits are rounded half up from the exact decimal value of the number;
 *  - fixed notation is used when the unsigned text and two more columns fit in the field
 *    (always when the width is 0); otherwise the number is written as .ddd"e (the " is
 *    'times ten to the'; the mantissa is a fraction from .1 up, the exponent has no sign
 *    when positive and no leading zeros), with w-7 digits, padded to w; a field narrower
 *    than 9 gets two digits and no padding;
 *  - a negative width is the scientific form always, in -m columns, with n+1 digits;
 *  - zero is a bare 0 right-aligned in the field (the original does not stop to print it in
 *    the scientific form: it hangs, and the output is lost);
 *  - a carry out of the digits of the scientific form is dropped (99999.5 is .99999"5).
 *
 * The original works in 27 bit single precision and its digits for numbers that are not
 * exact in binary (0.001, 12345678.0) differ in the last places: not reproduced.
 */
#define NDIG 41

/* the exact decimal digits of ax: value = 0.d[0]d[1]... * 10^*e */
static void
decdigits(double ax, char *d, int *e)
{
    char buf[64];

    if (ax == 0) {
        memset(d, '0', NDIG);
        d[NDIG] = '\0';
        *e = 0;
        return;
    }
    snprintf(buf, sizeof buf, "%.*e", NDIG - 1, ax);    /* d.ddd...e+XX */
    d[0] = buf[0];
    memcpy(d + 1, buf + 2, NDIG - 1);
    d[NDIG] = '\0';
    *e = atoi(strchr(buf, 'e') + 1) + 1;
}

/* keep the first k digits of d, rounded half up; 1 if that carried out of the front */
static int
roundk(char *d, int k)
{
    int i, carry;

    if (k < 0) {
        d[0] = '\0';
        return 0;
    }
    carry = d[k] >= '5';
    d[k] = '\0';
    for (i = k - 1; i >= 0 && carry; i--) {
        if (d[i] == '9')
            d[i] = '0';
        else {
            d[i]++;
            carry = 0;
        }
    }
    return carry;
}

static void
printflt(double x, long m, long n)
{
    char d[NDIG + 1], r[NDIG + 8], txt[256];
    int e, carry, neg = x < 0, len;
    long w = m < 0 ? -m : m;

    if (x == 0) {                               /* zero is a bare 0, in any format */
        field("0", w, 0);
        return;
    }
    decdigits(fabs(x), d, &e);
    if (m >= 0) {
        int k = e + (int)n;
        char *lead;

        /* r: the digits of |x| * 10^n rounded to an integer, without leading zeros */
        if (k < 0)
            r[0] = '\0', carry = 0;
        else {
            strcpy(r + 1, d);
            carry = roundk(r + 1, k);
            r[0] = '1';
        }
        lead = r + (carry ? 0 : 1);
        if (k < 0)
            lead = r;
        else if (k == 0 && carry)
            strcpy(r, "1"), lead = r;
        while (*lead == '0')
            lead++;
        len = (int)strlen(lead);
        {
            char *q = txt;
            int ni = len - (int)n, i;

            if (ni > 0) {
                memcpy(q, lead, (size_t)ni);
                q += ni;
            }
            if (n > 0) {
                *q++ = '.';
                for (i = 0; i < n; i++) {
                    int idx = ni + i;           /* index into lead; before its start: a zero */

                    *q++ = idx >= 0 ? lead[idx] : '0';
                }
            }
            *q = '\0';
        }
        if (m == 0 || (long)strlen(txt) + 2 <= m) {
            char out[300];

            snprintf(out, sizeof out, "%s%s", neg ? "-" : "", txt);
            field(out, m, 0);
            return;
        }
    }
    {
        int pad = 1, dg = m < 0 ? (int)n + 1 : (int)(w - 7);

        if (dg < 2)                             /* a field too narrow for any good form: two digits, unpadded */
            dg = 2, pad = 0;
        char out[300];

        if (dg > NDIG - 2)
            dg = NDIG - 2;
        if (roundk(d, dg)) {                    /* the carry is dropped: all nines */
            memset(d, '9', (size_t)dg);
            d[dg] = '\0';
        }
        snprintf(out, sizeof out, "%s.%s\"%d", neg ? "-" : "", d, e);
        field(out, pad ? w : 0, 0);
    }
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
        FILE *f = po.fp != NULL ? po.fp : stdout;

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
        fwrite(p, 8, 1, po.fp != NULL ? po.fp : stdout);
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
