/*
 * lex_imp.c -- lex.c's interface, over src/lex.imp compiled by imp72stage0: LEX, NEXTCHAR and PLINE1 are IMP;
 * the file is read here (LEXIN, and the bytes IMP asks for through impglue.c).  Ports decus/lex.imc: LEXIN, LEX,
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
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "imp.h"

extern word impl_lex(word *), impl_lexreset(word *), impl_pline1(word *);

static char *src;                   /* the whole input file */
static size_t srclen, srcpos;


/* the next byte of the file, or -1 (for IMPSRCGETC) */
word
lex_srcgetc(void)
{
    return srcpos < srclen ? (unsigned char)src[srcpos++] : -1;
}

/* LEXIN: open the source file.  0 if ok, -1 if it cannot be read. */
int
lexin(const char *path)
{
    FILE *f = fopen(path, "rb");
    size_t n, cap = 0;
    char buf[8192];
    word nil = 0;

    if (f == NULL)
        return -1;
    free(src);
    srclen = 0;
    srcpos = 0;
    src = NULL;
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
    fclose(f);
    impl_lexreset(&nil);
    return 0;
}

word
lex(word *s)
{
    word r;

    r = impl_lex(s);
    return r;
}

void
pline1(word ol)
{
    impl_pline1(&ol);
}
