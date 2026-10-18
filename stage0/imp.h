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
#define SW_GTRACE      0x10L      /* /E  trace productions entered in the graph */
#define SW_CONTINUE    0x4L       /* /C  compile another file after this one */
#define SW_FMAP        0x20L      /* /F  trace free storage, print its map */
#define SW_PTRACE      0x40L      /* /G  trace the parser and error corrector */
#define SW_HELP        0x80L      /* /H */
#define SW_DSPILL      0x100L     /* /I  print the directory after hashing */
/* ERROR sets the /L bit (4000B) so that source lines are listed from the
 * first error on; the original does the same. */
#define SW_LIST        0x800L     /* /L  source listing */
#define SW_ERRORS      SW_LIST
#define SW_SEMPER      0x40000L   /* /S  print the semantics array */
#define SW_TPRINT      0x80000L   /* /T  print the code tree */
#define SW_GPRINT      0x800000L  /* /X  print the syntax graph */
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

/*
 * FREE and FREES are functions, not macros: `FREES(a, f())` with an f that
 * allocates must not use the `fmem` it loaded before the call.
 */
static inline word FREE(word a) { return fmem[a]; }
static inline void FREES(word a, word v) { fmem[a] = v; }
static inline word *freep(word a) { return &fmem[a]; }

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
const char *gname(word i);
void dname_str(word i, char *buf, size_t n);  /* the whole name */                 /* GNAME: name as a C string */
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

/* ---- graph.c: the syntax graph (graph.imc) ----------------------------- */

#define NS 2                                   /* words in a graph node */

word graph(word synt, word nsynt, word pnq);   /* GRAPH: enter a production */
word gmatr(word tfl);                          /* GMATR: remake NBITM */
word ggoal(word nt);
word gtype(word n);
word gval(word n);
word gnb(word n);
word scr(word n);
word alt(word n);
word gentry(void);
word gconn(word q, word qgoal);
word gmade(word q, word qgoal);
word galt(word q, word qgoal);
word gnext(word q, word qgoal);
word gleap(word nt, word no);
word gnt2t(word nt, word *ni, word gl);
word gnt2tx(word nt, word *ni, word gl);
void gpout1(word cnode, word nnode, word put);
void gpout2(word cnode, word goal, word put);
void graphf(void);
void gprint(void);
extern word ga, ng, nprods;                    /* GA, NG, NPRODS */
/* graph state, for debug.c */
extern word gr_nodes, gr_ntgent, gr_nbitn, gr_bitm[3], gr_nbitm[3];
long imp_msec(void);                           /* CALLI(27B,0): run time, ms */

/* ---- parse.c, ercor.c: the Irons parser (parse.imc, ercor.imc) ---------- */

word parse(const char *goal);                  /* PARSE */
void pforce(word sy);                          /* insert a symbol in the input */
word pout(word qq, word vv);                   /* POUT: output vv on parse qq */
word isname(word s);                           /* ISNAME */
word ercor(word op, word np, word ss);         /* ERCOR */
void ambig(word p, word *np, word *pn, word o, word tr);
void pstats(void);
void parsef(void);                             /* PARSEF */
extern void (*parse_cotree)(word k);           /* COTREE: gets each output */

/* ---- pr.c, debug.c: PRINT formats and the diagnostic print-outs ---------- */

/* The part of PMOD (the formatted print module) that debug.c needs.  Output
 * goes to imp_out(). */
void pr_stg(int w, const char *s);    /* STG w: at most w chars, right justified */
void pr_igr(int w, word v);           /* IGR w: signed decimal */
void pr_oct(int w, word v);           /* OCT w: 36 bit unsigned octal, zero filled */
void pr_nl(void);                     /* / */

#define SW_MATRICES 0x2000L           /* /N  print the connectivity matrices */

void helpd(void);                     /* HELPD */
void partr1(word cnode, word ctyp, word cval, word p, word ip, word o, word cw);
void gprin1(void);                    /* GPRIN1: the graph and its matrices */

/* ---- 36 bit words ------------------------------------------------------ */

/*
 * SEMS, PROSEM, the code tree, the code stack and the semantic stack keep the
 * 36 bit formats of the original (so debug.c can print the same octal).  A
 * word of one of them is a 36 bit two's complement number held sign-extended
 * in a `word`.  The halves are 18 bits: L18() and R18() take them, W18()
 * makes a word; sx36() wraps a result to 36 bits, u36() takes the bits.
 * RS (logical right shift) of such a word is `u36(w) >> n`; ARS is `w >> n`.
 */
#define MASK36 ((word)0xFFFFFFFFFL)
#define MASK18 ((word)0777777L)

static inline word u36(word w) { return w & MASK36; }
static inline word sx36(word w) { return ((w & MASK36) ^ ((word)1 << 35)) - ((word)1 << 35); }
static inline word L18(word w) { return (w >> 18) & MASK18; }
static inline word R18(word w) { return w & MASK18; }
static inline word W18(word l, word r) { return sx36((l << 18) | (r & MASK18)); }
/* 1 LS n, and x LS n, in a 36 bit word; a negative n shifts right */
static inline word ls36(word x, word n)
{
    if (n >= 36 || n <= -36)
        return 0;
    return n >= 0 ? sx36((word)((uword)x << n)) : sx36(u36(x) >> -n);
}

/* ---- sem.c, dosem.c: semantic routines and the semantics array ---------- */

/*
 * A semantic routine is called by DOSEM with the words it was given, as
 * `fn(p)`: p[0], p[1], ... are its arguments (p has room for MARGS = 10; what
 * is beyond the count passed is left over from earlier calls, as in the
 * original).  The routine's address in the original is its id here: 1 + its
 * slot in a table, kept as the 'SEM' property of its name.
 */
typedef word (*semfn)(const word *p);
void dsem(const char *name, semfn fn);         /* DSEM */
word semcall(word id, const word *pars);
void sem_init(void);                           /* register the routines */

#define MARGS 10

/* dosem.c */
word dosem(word ppo);
word setsem(word ty, word val);
void resetsem(word val, word a);
word semfix(word i, word a);
word semfx1(word i, word a, word na);
void docase(word id, word semn, word prodn);
void enprod(word pn, word no, word sem);
word matchp(word pn, word n);
void noteprod(word pn, word n);
word plies(word i);
word snoden(word i);
word enstack(word v);
word snarg(word n);
word semget(word n);
word semed(word a);
void semor(word a, word n);
word cksval(word v, word i);
word par(word i);
void semper(void);
void sempr1(word sems, word prosem, word nsems);
void pstac1(word s, word n);
word synarg(word i);                           /* SYNARG (rsyn.c) */
extern word sems, nsems, prosem, nprosem, scratch;
extern word entries;                           /* ENTRIES (cotree.c sets it) */

/* entree.c */
word entree(word s, word cs, word sr, word pn);
word match(word *tnp, word sn, word snonr);
void entrca(word semn);
void tprint(void);
void tprin1(void);
extern word ct, nct, noden, sbitm[3];
word tscr(word n);
word talt(word n);
word tnoden(word n);

/* cotree.c */
word cotree(word ss);
void cfudge(void);
void codent(word a, word b, word c, word f);
void ckdone(void);
word maxwell(word n);
word maxend(word a);
word setpri(word a);
word sameas(word sn, word k);
void pstack(word s, word n);
extern word cs, ncs;

/* rsyn.c */
void rsyn(void);
word synt(word a, word b);
word sytrm(word a);
word synts(word n);
word semantics(void);
word syntax(word n);
word semr(word a, word b);
word semp(word a, word b);
word quosem(word a);
word equose(word a);
word defcase(word n);
word docase_(word a, word f);
word sembit(word a);
word repval(word s, word a);
word valu(word n, word s);

/* sem.c: the semantic routines of impsem.imc and code2.imc that syntax
 * statements use when they are read */
word newlist(word s);
word enlist(word s);
word getlist(word s);
word hook(word d, word a, word b);
word assemc(word a);
void trans_open(FILE *f);                       /* write pcc IR to f */
void trans_unit(word a);

/* codegen.c, impsem.c: the code generator (PDP-10 code chains) */
extern word co, nco, anco;
void codegen_init(void);
word addcode(word s, word w1, word w2);
word dewop(word op, word rr, word s);
word fetch(word s);
word store(word s, word t);
word regof(word s);
word addop(word op, word s, word t);
word tag(word s);
word dewfun(word s, word i, word j);
word freeze(word s);
word subrcall(word t);
word subrpar(word s, word pp);
word return_(word s);
word conop(word s, word t, word i, word v, word w);
word float_(word s);
word fix_(word s);
word ojumpop(word i);
word datast(word b);
word addr_(word s);
word coppy(word s);
word subscript(word s, word t);
word prinpar(word n, word aa, word b, word c, word d);
word princal(word a, word b);
word subcall(word a, word b);
void copyco(word s);
word mcode(word s, word t);
word oneword(word s);
word ochk_(word opc);
word hisreg(word i);
word subreg(word j);
word tmpreg(word j);
word samereg(word j);
word regeq(word i, word j);
word regsim(word i, word j);
word reg0(word i);
word rmatch(word i, word j, word rn);
word subpr0(word i);
word subprm(word i);
word subtmpf(void);
word subbeg(word a);
word subend(void);
word remot(word a);
word callme(word a);
word reg2s(word s);
word nsubp(void);
word impsem_array(word i, word *size);          /* the declared arrays, ARR */
word impsem_narr(void);
word impsem_nregs(void);
word anyco(void);
void opname(word op, char *buf);
void pcode_chain(word iw);
void pcode(word s);

/* impsem.c */
void impsem_init(void);
word name(word s);
word tstrn(word dirindex);
word areg(word i);
word areg1(word i, word j);
word decl(word i, word s);
word declare(word s);

#endif
