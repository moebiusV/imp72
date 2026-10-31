/*
 * lex.c -- input and lexical analysis.  Ports decus/lex.imc: LEXIN, LEX,
 * NEXTCHAR, PLINE1, PLINE.
 *
 * Not ported: GETTY, READCMD, PUTN, PUTO, PUTSW and LEXSAV/LEXRES, which
 * parse a TOPS-10 command line (`REL,LST_DEV:FILE.EXT[P,PN]/A/B`) by running
 * LEX over a typed-in buffer.  The driver takes arguments from argv.
 *
 * The lexer is a character at a time machine.  A token is
 *   - a run of letters and digits;
 *   - 'text': an apostrophe string, '' standing for one apostrophe; the token
 *     keeps its opening apostrophe and loses its closing one;
 *   - !text!: everything between the exclamation marks, which are dropped;
 *   - or one other non-blank character.
 * Blanks and control characters (including the newline LEX is fed at the end
 * of each line) separate tokens; #...# is a comment.
 *
 * Differences: the file is read whole instead of by buffer, and the line and
 * token buffers are C buffers (the original keeps them in relocatable free
 * arrays LEXL and LEXT).
 */
#include <stdlib.h>
#include <string.h>
#include "imp.h"

static char *src;                   /* the whole input file */
static size_t srclen, srcpos;

static char *line;                  /* LINE: the current line, NUL ended */
static size_t linelen, linecap;
static size_t pc;                   /* PC: index of the next character */
static word ic;                     /* IC: characters read from this line */
static word pct;                    /* PCT: paren depth after the line read */
static int tfl;                     /* TFL: PLINE1 already printed this line */
static int skip;                    /* SKIP: the lookahead is still to be used */
static int csv;                     /* CSV: the lookahead character */
static int isanm;                   /* ISANM: it is a letter or a digit */
static int c;                       /* C */
static int lastc;                   /* the character before C (a := is one token, a ::= is not) */

static char *tok;                   /* TOKE */
static size_t toklen, tokcap;

/* ISTAB: letters and digits (ASCII only), and the underscore: a name is a valid C name, and assignment is := */
static int
alnum(int ch)
{
    return (ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'Z')
        || (ch >= 'a' && ch <= 'z') || ch == '_';
}

static void
tokput(int ch)
{
    if (toklen + 2 > tokcap) {
        char *t = realloc(tok, tokcap = tokcap ? tokcap * 2 : 64);

        if (t == NULL) {
            imp_error(0, "OUT OF MEMORY IN LEX. ");
            return;
        }
        tok = t;
    }
    tok[toklen++] = (char)ch;
}

static void
lineput(int ch)
{
    if (linelen + 2 > linecap) {
        char *t = realloc(line, linecap = linecap ? linecap * 2 : 128);

        if (t == NULL) {
            imp_error(0, "OUT OF MEMORY IN LEX. ");
            return;
        }
        line = t;
    }
    line[linelen++] = (char)ch;
}

/* LEXIN: open the source file.  0 if ok, -1 if it cannot be read. */
int
lexin(const char *path)
{
    FILE *f = fopen(path, "rb");
    size_t n;

    if (f == NULL)
        return -1;
    free(src);
    srclen = 0;
    srcpos = 0;
    src = NULL;
    {
        size_t cap = 0;
        char buf[8192];

        while ((n = fread(buf, 1, sizeof buf, f)) > 0) {
            if (srclen + n > cap) {
                char *t = realloc(src, cap = (srclen + n) * 2);
                if (t == NULL) {
                    fclose(f);
                    return -1;
                }
                src = t;
            }
            memcpy(src + srclen, buf, n);
            srclen += n;
        }
    }
    fclose(f);
    linelen = 0;
    if (line != NULL)
        line[0] = '\0';
    pc = 0;
    pct = 0;
    skip = 0;
    return 0;
}

/* PLINE(BF,PCT): list a source line, with its paren depth, if asked. */
static void
pline(const char *bf, word depth)
{
    if (finam5 & SW_LIST)
        fprintf(imp_out(), "%3ld %s\n", (long)depth, bf);
    if (finam5 & SW_TTYLIST)
        imp_msg(bf);
}

/* PLINE1(OL): print the current line to the terminal, broken before
 * character number OL (to show where an error was found).  Once a line. */
void
pline1(word ol)
{
    word j = 0;
    const char *rc;

    if (tfl != 0 || line == NULL)
        return;
    tfl = -1;
    for (rc = line; *rc != '\0'; rc++) {
        if (++j == ol)
            fputc('\n', stderr);
        fputc(*rc, stderr);
    }
    fputc('\n', stderr);
}

/* NEXTCHAR: advance C; at the end of a line, read the next one. */
static void
nextchar(void)
{
    word opct;

    lastc = c;
    c = pc < linelen ? (unsigned char)line[pc] : 0;
    pc++;
    ic++;
    if (c == 0) {
        opct = pct;
        linelen = 0;
        for (;;) {
            if (srcpos >= srclen) {
                imp_error(0, "PREMATURE END OF INPUT FILE");
                return;
            }
            c = (unsigned char)src[srcpos++];
            if (c == '\n')
                break;
            if (c != 0) {
                lineput(c);
                if (c == '(')
                    pct++;
                else if (c == ')')
                    pct--;
            }
        }
        lineput(0);
        linelen--;                  /* the NUL is not part of the line */
        pc = 0;
        ic = 0;
        tfl = 0;
        pline(line, opct);
        c = '\n';
    }
    isanm = alnum(c);
}

/* LEX(S): read the next token.  *s is its directory index; the value is IC,
 * the column the lexer has reached (one past the token, for most tokens). */
word
lex(word *s)
{
    c = csv;
    toklen = 0;
t0: if (!skip)
        nextchar();
    skip = 0;
    if (!isanm)
        goto t2;
t1: tokput(c);
    nextchar();
    if (isanm)
        goto t1;
    skip = -1;
    goto lx;
t2: if (c <= 040)
        goto t0;
    if (c == '#') {
        do
            nextchar();
        while (c != '#');
        goto t0;
    }
    if (c == '\'') {
t4:     tokput(c);
        nextchar();
        if (c != '\'')
            goto t4;
        nextchar();
        if (c == '\'')
            goto t4;
        skip = -1;
        goto lx;
    }
    if (c == '!') {
t5:     nextchar();
        if (c != '!') {
            tokput(c);
            goto t5;
        }
        goto lx;
    }
    tokput(c);
    if (c == ':' && lastc != ':') {         /* := is one token */
        nextchar();
        if (c == '=')
            tokput(c);
        else
            skip = -1;
    }
lx: tokput(0);
    csv = c;
    *s = dir(tok);
    return ic;
}
