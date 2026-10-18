/*
 * imp.h -- IMP72 stage 0: shared definitions.
 *
 * Stage 0 is a hand port, to C99, of the machine-independent kernel of the
 * IMP72 compiler (DECUS 10-198a, W. Bilofsky, 1973; see decus/PROVENANCE.md).
 * The original is IMP72 source for the PDP-10 (the .imc files in decus/).  Each C file
 * names the .imc file it ports, and each function the routine it ports.
 *
 * Differences from the original that every file shares:
 *
 *  - The machine word is 64 bits (`word`), not 36.
 *  - A halfword is 32 bits, not 18: HL()/HR()/HW() below replace the `<L>`
 *    and `<R>` byte forms and `X LS 18 OR Y` packing.
 *  - "Pointers" into free storage are word indices into one growable array
 *    (`fmem`), exactly as in the original, where they were addresses relative
 *    to FORG.  Index 0 means "none".
 *  - Names in the directory keep the original packing (5 seven-bit characters
 *    per word, left justified, NUL terminated) so that directory contents can
 *    be compared with a PDP-10 oracle.  Everything outside the directory sees
 *    ordinary C strings.
 *
 * IMP expressions have no operator precedence (`A+B*C` is `A+(B*C)`) and
 * `FOR I TO N` includes N.  The ports spell the grouping out.
 */
#ifndef IMP_H
#define IMP_H

#include <stdint.h>
#include <stdio.h>

typedef int64_t word;
typedef uint64_t uword;

/* ---- halfwords (<L> and <R>) ------------------------------------------ */

#define HW_MASK 0xFFFFFFFFu
#define HL(w)   ((word)(((uword)(w) >> 32) & HW_MASK))
#define HR(w)   ((word)((uword)(w) & HW_MASK))
#define HW(l,r) ((word)(((uword)(l) << 32) | ((uword)(r) & HW_MASK)))

/* ---- error.c: driver services the kernel calls (ERROR, MSG, PRINT) ----- */

/*
 * FINAM[5] of the original: the command switches, one bit per letter
 * (/A is bit 0).  Only the bits the kernel looks at are named.
 */
#define SW_ASSEMBLY    0x1L       /* /A  assembly listing */
#define SW_CONTINUE    0x4L       /* /C  compile another file after this one */
#define SW_FMAP        0x20L      /* /F  trace free storage, print its map */
#define SW_HELP        0x80L      /* /H */
#define SW_DSPILL      0x100L     /* /I  print the directory after hashing */
/* ERROR sets the /L bit (4000B) so that source lines are listed from the
 * first error on; the original does the same. */
#define SW_LIST        0x800L     /* /L  source listing */
#define SW_ERRORS      SW_LIST
#define SW_SEMPER      0x40000L   /* /S  print the semantics array */
#define SW_TPRINT      0x80000L   /* /T  print the code tree */
#define SW_GPRINT      0x400000L  /* /W  print the syntax graph */
#define SW_TTYLIST     0x1000000L /* /Y  list source on the terminal */
#define SW_STATS       0x2000000L /* /Z  count FALLOT calls (FSTATS) */

extern word finam5;               /* FINAM[5] */
extern FILE *listing;             /* the PRINT FILE (.LST); NULL if none */
extern word imp_err;              /* ERR: -1 once any error has been reported */
FILE *imp_out(void);              /* where PRINT output goes: listing or stdout */

void imp_error(int n, const char *msg);    /* ERROR(N,E): n==0 is fatal */
void imp_msg(const char *msg);             /* MSG(M) */
void imp_fatal_handler(void (*fn)(void));  /* where a fatal error unwinds to */

/* ---- free.c: relocatable free storage (free.imc) ----------------------- */

extern word *fmem;                /* [FORG+...]: the free storage area */

#define FREE(a)    (fmem[(a)])
#define FREES(a,v) (fmem[(a)] = (v))

word fpackname(const char *name);          /* header word 1 for an array name */
word flen(word a);                         /* FLEN: array length */
word fname(word a);                        /* FNAME: packed array name */
word fallot_new(const char *name, word n); /* FALLOT(name,N): new array */
word fallot(word a, word n);               /* FALLOT(A,N): resize, or free */
word frelot(word *a, const char *name, word n, void (*fix)(void));
void fadd(word *a, word *n, word v);       /* FADD(A,N,V) */
void faddex(word *a, word n);              /* FADDEX */
void fincset(word a, word n);
void ftrace(word v);
void fmap(void);
void fstats(void);
void finit(void);
void fclose_(void);                        /* FCLOSE (fclose is stdio's) */

/* ---- stack.c: semantic stack, pushdown stack, bit matrices (stack.imc) -- */

word stack(word n);
word stinit(void);
word stpop(word n);
word stpush(word a, word b, word c);
word setstk(word n, word a, word b, word c);
extern word st, stp;                       /* ST, STP: semantic stack state */

void pushmi(word v);
word pullyu(word sw);

/* A bit matrix descriptor is three words: a[0] free index, a[1] rows,
 * a[2] (columns / 64) - the words per row less one. */
void bits(word a[3], word i, word j);
word bit(const word a[3], word i, word j);
void bitz(word a[3]);
void bitor_(word a[3], word i, word j);
void bstats(int reset);

/* ---- dir.c: directory (dir.imc) ---------------------------------------- */

#define DIR_HASHROWS 1536          /* NPRP's initial value: 512 buckets x 3 */

word dir(const char *name);                /* DIR: index of a name */
word dname(word i, word j);                /* DNAME: jth packed word of a name */
const char *gname(word i);                 /* GNAME: name as a C string */
void pname(word i);                        /* PNAME */
word dseq(word n);                         /* DSEQ */
word radi50(word i);                       /* RADI50 */
void dstats(void);
void dspill(void);

/* directory node properties: the 3-word nodes of array PRP.  These replace
 * the DPROP/DPROPS syntax of impsyn.imc. */
enum dprop {
    DP_NAME, DP_ENTRY, DP_NODE, DP_COM, DP_ATYPE, DP_SEM, DP_ISNM,
    DP_LXTY, DP_USE
};
word dprop(enum dprop p, word i);
void dprops(enum dprop p, word i, word v);
extern word prp, alf;                      /* the PRP and ALF array pointers */

/* CONVC / STCON / NEWNAME (also in dir.imc) */
word convc(word c);                        /* 0, -1 (numeric), or string array */
extern word convc_const;                   /* CONST */
word stcon(word v);
word newname(const char *s);

/* ---- lex.c: input and lexical analysis (lex.imc) ----------------------- */

int lexin(const char *path);               /* LEXIN: 0 ok, -1 can't open */
word lex(word *s);                         /* LEX: sets *s to the token's
                                              directory index, returns IC */
void pline1(word ol);

#endif
