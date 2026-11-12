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
static int syn, ins, p1, p2;      /* in a syntax statement, between the " of a semantic string, and the kinds of the last two tokens (src/lex.imc TKSTATE) */
static int dead;                  /* a backslash-zero has ended the C string being read */
static int fc, nc;                /* the first character and the length of a name being read: R followed by a string is a number (rstr) */

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

/* ISTAB (src/lex.imc): the letters and digits of ASCII, the underscore (a name is a valid C name, and assignment is :=), and every byte of 128 and up, so that the UTF-8 text of a
 * name is part of it (a symbol of UTF-8 that begins a token is not: symlen) */
static int
alnum(int ch)
{
    return (ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'Z')
        || (ch >= 'a' && ch <= 'z') || ch == '_' || ch >= 128;
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

/* the files of declarations that go in front of each source (-H of imp: src/lex.imc LEXHDR); the first lexin, the syntax, is read before there is one */
#define MAXHDR 64
static const char *hdrv[MAXHDR];
static int nhdr;

void
lexhdr(const char *path)
{
    if (nhdr < MAXHDR)
        hdrv[nhdr++] = path;
}

/* append the file at path to src; 0 if ok, -1 if it cannot be read */
static int
slurp(const char *path, size_t *cap)
{
    FILE *f = fopen(path, "rb");
    size_t n;
    char buf[8192];

    if (f == NULL)
        return -1;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) {
        if (srclen + n + 1 > *cap) {
            char *t = realloc(src, *cap = (srclen + n + 1) * 2);

            if (t == NULL) {
                fclose(f);
                return -1;
            }
            src = t;
        }
        memcpy(src + srclen, buf, n);
        srclen += n;
    }
    fclose(f);
    return 0;
}

/* LEXIN: open the source file, behind the files of declarations.  0 if ok, -1 if it cannot be read. */
int
lexin(const char *path)
{
    size_t cap = 0;
    int h;

    free(src);
    srclen = 0;
    srcpos = 0;
    src = NULL;
    for (h = 0; h < nhdr; h++)
        if (slurp(hdrv[h], &cap) != 0) {
            fprintf(stderr, "imp: cannot read the declarations file %s\n", hdrv[h]);
            return -1;
        } else if (srclen > 0 && src[srclen - 1] != '\n')
            src[srclen++] = '\n';
    if (slurp(path, &cap) != 0)
        return -1;
    linelen = 0;
    if (line != NULL)
        line[0] = '\0';
    pc = 0;
    pct = 0;
    skip = 0;
    syn = ins = p1 = p2 = 0;
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

/* the bytes of the symbol of UTF-8 that the character c begins, or 0: the arrows and the mathematical operators, U+2190 to U+22FF (three bytes, E2 86 to E2 8B and one more), and the not sign
 * U+00AC (C2 AC).  They are never part of a name, so that A<=B needs no blanks; what each means is for a syntax extension (lib/utf8.imc), and any other byte of 128 and up is a letter of a name as
 * before (src/lex.imc SYMLEN).  pc is the position of the byte after c. */
static int
symlen(void)
{
    unsigned b1, b2;

    if (c == 0342) {
        b1 = (unsigned char)line[pc];
        b2 = b1 != 0 ? (unsigned char)line[pc + 1] : 0;
        if (b1 >= 0206 && b1 <= 0213 && b2 >= 0200 && b2 <= 0277)
            return 3;
    } else if (c == 0302 && (unsigned char)line[pc] == 0254)
        return 2;
    return 0;
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
    if (isanm && symlen() != 0)             /* the first byte of a symbol of UTF-8 is not a letter of a name */
        isanm = 0;
}

/* where a " is the delimiter of a semantic string and where it starts a C string: a syntax statement (<..> ::= ... ::= ...;) is from its ::= (the tokens : : =) to its ; or its %
 * outside a semantic string; in it every " is a token of its own and ins says whether we are between a pair; anywhere else a " starts a string (src/lex.imc TKSTATE) */
static void
tkstate(void)
{
    int b0 = (unsigned char)tok[0], k = 0;

    if (tok[0] == '\0' || tok[1] != '\0') {          /* more than one character: not one of these */
        p2 = p1;
        p1 = 0;
        return;
    }
    if (b0 == ':')
        k = 1;
    else if (b0 == '=')
        k = 2;
    if (k == 2 && p1 == 1 && p2 == 1)
        syn = 1;
    if (b0 == ';' && ins == 0)
        syn = 0;
    if (b0 == '%' && ins == 0)
        syn = 0;
    p2 = p1;
    p1 = k;
}

static int
hexv(int ch)
{
    if (ch >= '0' && ch <= '9')
        return ch - '0';
    if (ch >= 'A' && ch <= 'F')
        return ch - 'A' + 10;
    if (ch >= 'a' && ch <= 'f')
        return ch - 'a' + 10;
    return -1;
}

/* a byte of a C string: in the token unless a \0 has ended the string (the rest of it up to the closing " is read and left out) */
static void
cput(int v)
{
    if (!dead)
        tokput(v);
}

static void
putu(unsigned long x)
{
    if (x < 128)
        cput((int)x);
    else if (x < 2048) {
        cput((int)(192 | (x >> 6)));
        cput((int)(128 | (x & 63)));
    } else if (x < 65536) {
        cput((int)(224 | (x >> 12)));
        cput((int)(128 | ((x >> 6) & 63)));
        cput((int)(128 | (x & 63)));
    } else {
        cput((int)(240 | (x >> 18)));
        cput((int)(128 | ((x >> 12) & 63)));
        cput((int)(128 | ((x >> 6) & 63)));
        cput((int)(128 | (x & 63)));
    }
}

/* CSTR: "text" at the " that starts it.  The token is the one of a string in apostrophes (the apostrophe and the bytes), and it is the same string constant.  The escapes are
 * C's: \n \t \r \a \b \f \v \e, \0 (which ends the string: a string is ASCIZ), \\ \" \', \xHH (one byte) \uHHHH and \UHHHHHHHH (a code point, as UTF-8); any other character after a \ is itself.
 * The string ends at the next " that is not escaped, on its line. */
static void
cstr(void)
{
    unsigned long v;
    int d, i, n;

    dead = 0;
    cput('\'');
    for (;;) {
        nextchar();
        if (c == '"') {
            nextchar();
            skip = -1;
            return;
        }
        if (c == '\n') {
            imp_error(1, "A STRING IN QUOTES ENDS AT THE END OF THE LINE.");
            skip = -1;
            return;
        }
        if (c != '\\') {
            cput(c);
            continue;
        }
        nextchar();
        switch (c) {
        case 'n': cput('\n'); continue;
        case 't': cput('\t'); continue;
        case 'r': cput('\r'); continue;
        case 'a': cput(7); continue;
        case 'b': cput(8); continue;
        case 'f': cput(12); continue;
        case 'v': cput(11); continue;
        case 'e': cput(27); continue;
        case '0': dead = 1; continue;
        case 'x': n = 2; break;
        case 'u': n = 4; break;
        case 'U': n = 8; break;
        default: cput(c); continue;
        }
        v = 0;
        for (i = 0; i < n; i++) {
            nextchar();
            if ((d = hexv(c)) < 0) {
                imp_error(1, "A HEXADECIMAL DIGIT IS EXPECTED AFTER \\X OR \\U IN A STRING.");
                skip = -1;
                return;
            }
            v = (v << 4) | (unsigned long)d;
        }
        if (n == 2)
            cput((int)v);
        else
            putu(v);
    }
}

/* R'TEXT': the characters of the string, one to eight, right-justified in a number (the last in the low byte; the tape's are one to five, 7 bits each).  The lexer has read the R and
 * stands at the ' or " or ! that follows, which is how the form of the string is told ('TEXT', "TEXT" with C's escapes, or !'TEXT!, which R may prefix as well).  The string is read
 * into the token as it would be, and the token becomes the number, in octal with a B (up to 22 digits) (src/lex.imc RSTR) */
static void
rstr(int f)
{
    unsigned long v = 0;
    size_t n, i;
    int dg, started = 0, b;

    toklen = 0;
    if (f == 0) {
        tokput('\'');
        for (;;) {
            nextchar();
            if (c != '\'') {
                tokput(c);
                continue;
            }
            nextchar();
            if (c != '\'')
                break;
            tokput(c);
        }
    } else if (f == 1)
        cstr();
    else {
        nextchar();
        if (c == '\'') {
            tokput('\'');
            for (nextchar(); c != '!'; nextchar())
                tokput(c);
            nextchar();
        } else {
            imp_error(1, "R REQUIRES TEXT CONSTANT OPERAND.");
            toklen = 0;
            tokput('0');
            tokput('B');
            skip = -1;
            return;
        }
    }
    skip = -1;
    n = toklen;                                     /* the bytes of the token, the leading ' included */
    for (i = 1; i < n; i++)
        v = (v << 8) | (unsigned char)tok[i];
    if (n > 9)
        imp_error(1, "R TAKES AT MOST EIGHT CHARACTERS.");
    toklen = 0;
    if (v == 0) {
        tokput('0');
        tokput('B');
        return;
    }
    for (dg = 22; dg > 0;) {
        dg--;
        b = (int)((v >> (3 * dg)) & 7);
        if (b != 0 || started) {
            tokput('0' + b);
            started = 1;
        }
    }
    tokput('B');
}

/* LEX(S): read the next token.  *s is its directory index; the value is IC,
 * the column the lexer has reached (one past the token, for most tokens). */
word
lex(word *s)
{
    int n;

    c = csv;
    toklen = 0;
t0: if (!skip)
        nextchar();
    skip = 0;
    if (!isanm)
        goto t2;
    fc = c;
    nc = 0;
t1: tokput(c);
    nc++;
    nextchar();
    if (isanm)
        goto t1;
    if (nc == 1 && fc == 'R') {            /* R'TEXT', R"TEXT" and R!'TEXT!: the characters right-justified in a number */
        if (c == '\'') {
            rstr(0);
            goto lx;
        }
        if (c == '"' && !syn) {
            rstr(1);
            goto lx;
        }
        if (c == '!') {
            rstr(2);
            goto lx;
        }
    }
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
        if (toklen == 1) {                  /* '' is the constant 0, as it is on the tape */
            toklen = 0;
            tokput('0');
        }
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
    if (c == '"') {
        if (syn)
            ins = !ins;
        else if (lastc < '0' || lastc > '9') {      /* (a " after a digit is the exponent of a real: 3.27"-5) */
            cstr();
            if (toklen == 1) {              /* "" is the constant 0, as '' is */
                toklen = 0;
                tokput('0');
            }
            goto lx;
        }
    }
    if ((n = symlen()) != 0) {            /* a symbol of UTF-8: its bytes are one token */
        tokput(c);
        nextchar();
        tokput(c);
        if (n == 3) {
            nextchar();
            tokput(c);
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
    tkstate();
    *s = dir(tok);
    return ic;
}
