/*
 * imp72rt.c -- the runtime library of IMP72 programs: the formatted PRINT.
 *
 * The compiler turns a PRINT statement into a call of imp_pr(n, p) for each of
 * its items, n the kind of item and p the address of its operand (0 if it has
 * none).  The kinds and their formats are those of the manual (section 2.2.7):
 *
 *   0  print the operand in the current format
 *   1  OCT w   octal, zero filled, in a field w wide
 *   2  IGR w   decimal, in a field w wide
 *   3  STG w   a string (the operand is its address) of at most w characters
 *              (w = 0: all of it), in a field w wide
 *   5  /       end the line
 *   9  TAB n   advance to column n
 *  10  FILL c  fill leading positions of fields with the character c
 *
 * A format stays in effect until another is given.  A width is the operand of
 * its item, the value of an expression.  A field too small for its value is
 * widened.  The value of the call is the column printed to.
 *
 * Standard output is the file PO; FILE, DEVICE, IMAGE MODE and FLT are not
 * done yet.  A line is written when it is ended with `/`.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fmt = 2;                     /* 1 OCT, 2 IGR, 3 STG */
static long width;
static int fill = ' ';
static char line[1024];
static int col;                         /* characters in line */

static void
put(const char *s, int n)
{
    while (n-- > 0 && col < (int)sizeof line - 1)
        line[col++] = *s++;
}

static void
pad(int n)
{
    while (n-- > 0 && col < (int)sizeof line - 1)
        line[col++] = (char)fill;
}

/* a number in a field, right justified, leading positions filled */
static void
field(const char *s, long w, int zero)
{
    int n = (int)strlen(s);

    if (w > n)
        while (w-- > n) {
            if (zero)
                put("0", 1);
            else
                pad(1);
        }
    put(s, n);
}

long
imp_pr(long n, long *p)
{
    char buf[64];

    switch (n) {
    case 0:
        if (p == NULL)
            break;
        if (fmt == 3) {
            const char *s = (const char *)p;
            int len = (int)strlen(s);

            if (width > 0 && len > width)
                len = (int)width;
            if (width > len)
                pad((int)width - len);
            put(s, len);
        } else if (fmt == 2) {
            snprintf(buf, sizeof buf, "%ld", *p);
            field(buf, width, 0);
        } else {
            snprintf(buf, sizeof buf, "%lo", (unsigned long)*p);
            field(buf, width, 1);
        }
        break;
    case 1:
    case 2:
    case 3:
        fmt = (int)n;
        width = p != NULL ? *p : 0;
        break;
    case 5:
        line[col] = '\0';
        puts(line);
        col = 0;
        break;
    case 9:
        if (p != NULL && *p > col)
            pad((int)*p - 1 - col);
        break;
    case 10:
        if (p != NULL)
            fill = (int)(*p & 0xFF) ? (int)(*p & 0xFF) : ' ';
        break;
    default:
        break;
    }
    return col;
}

/* FINI(I): close the output; I = 0 exits. */
long
fini(long *p)
{
    fflush(stdout);
    if (p != NULL && *p == 0)
        exit(0);
    return 0;
}

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
ifix(double *x)
{
    return (long)*x;
}
