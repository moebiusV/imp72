/*
 * gen.h -- the code generator of the stage compilers, in C (the seed's twin of src/llvm-ir, the .imp files).
 *
 * gen_impsem.c   ports src/llvm-ir/impsem.imp   (registers, subroutines, declarations, lists, names)
 * gen_code.c     ports src/llvm-ir/code.imp     (UNPCK, FETCH, DEWOP, STORE, COPYCO, SUBARG, ...)
 * gen_code2.c    ports src/llvm-ir/code2.imp    (ADDREC, HOOK, CONOP, SUBRCALL, FLOAT, PRINPAR, ...)
 * gen_code3.c    ports src/llvm-ir/code3.imp    (SUBCALL, SUBSCRIPT, BYTEP, PCODE)
 * gen_demit.c    ports src/llvm-ir/demit.imp    (the chain of records into IR trees) and the IRxxx
 *                                               helpers of src/llvm-ir/ix.imp (names, kinds, pools).
 *
 * The tree builders of ix.imp are NOT ported: stage0/ir.c's are called (ir.h).  ix.imp was compared with
 * ir.c, routine by routine, and they agree (see the report of the port; the one place where they have a
 * different shape, IXSETTARGET/TGINIT against ir_settarget, is not used here).
 *
 * The word formats are those of the IMP port (64 bit words, halves of 32 bits: HL, HR, HW of imp.h; the
 * formats of src/impsyn64.imc, src/cotree.imp, src/entree.imp, src/dosem.imp, src/dir.imp).
 *
 * Three differences of the language that the port keeps in mind:
 *
 *  - IMP evaluates the ARGUMENTS OF A CALL FROM THE RIGHT TO THE LEFT (G(F(1),F(2),F(3)) calls F(3) first;
 *    tried with build/llvm-ir2/imp72stage-llvm-ir), the operands of an operator from the left, and the
 *    right side of an assignment before its subscript.  Where two arguments of one call have effects (a
 *    new temporary, a statement written, a new label) the C evaluates them in that order, in temporaries.
 *  - THE BODY OF A FOR LOOP RUNS ONCE EVEN WHEN THE END IS BEYOND THE START (FOR I IN 0,1,-1 runs for I=0;
 *    FOR I FROM -1 runs for -1): the test is at the end.  The loops are do-while here, and the ones the IMP
 *    guards (NTAGS>0 => ...) are guarded the same.
 *  - Locals are static.  Where the IMP code reads a local it did not set in the call, the C keeps it static
 *    too (said in a comment); elsewhere C locals do the same.
 */
#ifndef GEN_H
#define GEN_H

#include "imp.h"
#include "ir.h"

/* the entry point: DSEM of every semantic routine, under the name the syntax calls it */
void gen_init(void);

/* ---- the names of the routines: IMP name -> C name ---------------------------------------------------------------------
 *
 * impsem.imp: IINIT g_iinit, AREG g_areg, REG2 g_reg2, AREG1 g_areg1, TSTRN g_tstrn, NAME g_name, VAL g_val, SVAL g_sval,
 *   STACKUP g_stackup, SWITCH g_switch, NSUBP g_nsubp, HISREG g_hisreg, PAIRFIRST g_pairfirst, SUBREG g_subreg,
 *   TMPREG g_tmpreg, SAMEREG g_samereg, REGEQ g_regeq, REGSIM g_regsim, REG0 g_reg0, RMATCH g_rmatch, SUBPR0 g_subpr0,
 *   SUBPRM g_subprm, SUBTMPF g_subtmpf, SUBBEG g_subbeg, SUBEND g_subend, REMOT g_remot, CALLME g_callme,
 *   REG2S g_reg2s, NEWLIST g_newlist, ENLIST g_enlist, GETLIST g_getlist, ASSEMC g_assemc, DECL g_decl,
 *   DECLARE g_declare, LETSYN g_letsyn, ERRSRT g_errsrt, CVAL g_cval, ISCVAL g_iscval, FCON g_fcon,
 *   IMPSEM gen_init (all the DSEMs).
 * code.imp: UNPCK g_unpck, REGOF g_regof, FETCH g_fetch, ADDOP g_addop, DEWOP g_dewop, TAG g_tag, STORE g_store,
 *   COPYC1 g_copyc1, REGFIELD g_regfield, COPYCO g_copyco, NEXCO g_nexco, MCODE1 g_mcode1, MCODE g_mcode,
 *   SUBARG g_subarg, CODEI gen_init.
 * code2.imp: SETNEXT g_setnext, ADDREC g_addrec, HOOK g_hook, ANYCODE g_anycode, DEWFUN g_dewfun, CONOP g_conop,
 *   JOP g_jop, CJUMP g_cjump, MOVEBLK g_moveblk, FREEZE g_freeze, ONEWORD g_oneword, COPPY g_coppy (the semantic
 *   name COPY), SUBRCALL g_subrcall, RETURN g_return, FLOAT g_float, FIX g_fix, DATAST g_datast, SUBRPAR g_subrpar,
 *   PRINPAR g_prinpar, PRINCAL g_princal, OJUMPOP g_ojumpop, ADDR g_addr, CODE2I gen_init.
 * code3.imp: SUBCALL g_subcall, SUBSCRIPT g_subscript, BYTEP g_bytep, PCODE g_pcode, PCODE1 g_pcode1, CODE3I gen_init.
 * demit.imp: RV rv, ICN icn, ASG asg, OFFSET offset, TAGLABEL taglabel, USEVAR usevar, POOLOF poolof, SYMADDR symaddr,
 *   EADDR eaddr, VEXPR vexpr, BOOLOP boolop, CMPOF cmpof, ESHIFTV eshiftv, ESHIFT eshift, ECALL ecall, EPITEM epitem,
 *   ETABJUMP etabjump, BDFIND bdfind, TMPV tmpv, TV tv, SHL shl, SHR shr, FMASK fmask, BPVALUE bpvalue, BPIN bpin,
 *   BPOUT bpout, CHARPTR charptr, CPSTEP cpstep, CPBYTE cpbyte, BPIBP bpibp, BPLDB bpldb, BPDPB bpdpb, EFIELD efield,
 *   ENEWWORD enewword, EWRITE ewrite, ELDB eldb, EDPB edpb, EHALF ehalf, ECALLI ecalli, EXECUTEW executew, EREC erec,
 *   TAGDEF tagdef, ISXTAG isxtag, JUMPTAG jumptag, XSCAN xscan, EXPORTTAGS exporttags, ECHKTAGS echktags, EBODY ebody,
 *   EFUNC efunc, EDATA edata (the last ones static, all in gen_demit.c), DEMIT g_demit.
 * ix.imp: only the IR* helpers on directory entries are ported (the builders are ir.c's): IRNAMEOF irnameof,
 *   IRSYMNAME g_irsymname, IRKIND g_irkind, IRSTRVAL g_irstrval, IRLABNAME g_irlabname, IRPOOLEMIT g_irpoolemit,
 *   IRFNKIND g_irfnkind, IRRUNTIME g_irruntime, IRTAGERR g_irtagerr, IRJBNAME g_irjbname, IRVARREC g_irvarrec.
 *   The builders: IXICON ir_icon, IXSYM ir_sym, IXNAME ir_name, IXTEMP ir_temp, IXREG ir_reg, IXUN ir_un, IXBIN ir_bin,
 *   IXASSIGN ir_assign, IXDEREF ir_deref, IXNEWTEMP ir_newtemp, IXNEWLABEL ir_newlabel, IXSTMT ir_stmt, IXLABEL
 *   ir_label, IXGOTO ir_goto, IXCBRANCH ir_cbranch, IXPASS ir_pass, IXQUAD ir_pass(" .quad %lld"), IXFUNCBEGIN
 *   ir_func_begin, IXFUNCEND ir_func_end, IXSHIFT ir_xshift, IXCALL ir_xcall, IXPARAM ir_param, IXTODOUBLE
 *   ir_todouble, IXFROMDOUBLE ir_fromdouble, IRVARREC ir_varrec (through g_irvarrec).  IRFREE, IRNEW, IXSETTARGET,
 *   TGINIT: not needed (the driver calls ir_settarget and ir_open).
 */

/* ---- state shared between the files (COMMON of the IMP modules) -------------------------------------------------------- */

/* the code chain: CO, NCO, ANCO (code2.imp), REMO (impsem.imp, demit.imp) */
extern word g_co, g_nco, g_anco, g_remo;
/* the declared arrays (ARR, NARR) and the register array's size (REGN): impsem.imp, demit.imp */
extern word g_arr, g_narr, g_regn;
/* the byte descriptors (BDESC, NBD): code3.imp, demit.imp */
extern word g_bdesc, g_nbd;
/* the unpacked stack entry, as UNPCK leaves it (code.imp) */
extern word g_se, g_tse, g_dse, g_se1, g_rse, g_cse;
/* the assembler names of the directory entries, by index (SYMC, NSYMC of ix.imp: a cache of C strings; the
 * image code of the IMP side clears it after loading an image) */
extern char **g_symc;
extern word g_nsymc;

/* ---- helpers of the language ------------------------------------------------------------------------------------------- */

/* X LS N, X RS N, X ARS N with a count that may be any word, as the shifts of the stage compiler (ESHIFTV) do: 64 or more
 * shifts everything out, a negative count shifts the other way (an arithmetic shift keeps the sign) */
static inline word
g_ls(word x, word n)
{
    if (n >= 64 || n <= -64)
        return 0;
    return n >= 0 ? (word)((uword)x << n) : (word)((uword)x >> -n);
}

static inline word
g_rs(word x, word n)
{
    return g_ls(x, -n);
}

/* HMASK of impsyn64.imc, and a word shifted left by 32 (LS 32) */
#define G_HM ((word)0xFFFFFFFFLL)
#define G_L32(x) ((word)((uword)(x) << 32))

/* NEWNAME of a name given as a word (its first eight characters, the first in the low byte; imp.h's newname takes a C string) */
word g_newname_w(word w);

/* CHR(W,K): the K-th character of a word, the first in the low byte */
static inline word
g_chr(word w, int k)
{
    return (word)(((uword)w >> (8 * k)) & 0xFF);
}

/* ---- impsem.imp -------------------------------------------------------------------------------------------------------- */

word g_iinit(void);
word g_areg(word i);
word g_reg2(word n);
word g_areg1(word i, word j);
word g_tstrn(word di);
word g_name(word s);
word g_val(word s);
word g_sval(word n);
word g_stackup(word v);
word g_switch(word n);
word g_nsubp(void);
word g_hisreg(word i);
word g_pairfirst(word j);
word g_subreg(word j);
word g_tmpreg(word j);
word g_samereg(word j);
word g_regeq(word i, word j);
word g_regsim(word i, word j);
word g_reg0(word i);
word g_rmatch(word i, word j, word rn);
word g_subpr0(word i);
word g_subprm(word i);
word g_subtmpf(void);
word g_subbeg(word a);
word g_subend(void);
word g_remot(word a);
word g_callme(word a);
word g_reg2s(word s);
word g_newlist(word s);
word g_enlist(word s);
word g_getlist(word s);
word g_assemc(word a);
word g_decl(word i, word s);
word g_declare(word s);
word g_letsyn(word s);
word g_errsrt(word n, word e);
word g_cval(word a);
word g_iscval(word d);
word g_ctypeof(word d);
word g_cpfind(word d);
void g_cpadd(word d, word h, word t0, word t1);
word g_cphead(word d);
word g_cptype(word d, word k);
word g_cdecl(word t, word f, word l, word v);
word g_fcon(word a, word b, word e);
void gen_impsem_dsems(void);                    /* IMPSEM: the DSEMs of gen_impsem.c */

/* ---- code.imp ---------------------------------------------------------------------------------------------------------- */

word g_unpck(word s);
word g_regof(word s);
word g_fetch(word s);
word g_addop(word op, word s, word t);
word g_dewop(word opp, word rr, word s);
word g_tag(word s);
word g_store(word s, word t);
word g_copyc1(word j);
word g_regfield(word w, word r, word rn);
word g_copyco(word s);
word g_nexco(word i);
word g_mcode1(word i, word j);
word g_mcode(word s, word t);
word g_subarg(word s, word pp);
void gen_code_dsems(void);

/* ---- code2.imp --------------------------------------------------------------------------------------------------------- */

word g_setnext(word r, word n);
word g_addrec(word s, word kind, word op, word rd, word v0, word v1, word x);
word g_hook(word d, word a, word b);
word g_anycode(void);
word g_dewfun(word s, word i, word j);
word g_conop(word s, word t, word i, word v, word w);
word g_jop(word c);
word g_cjump(word op, word p, word r, word s);
word g_moveblk(word b, word n, word a);
word g_freeze(word s);
word g_oneword(word s);
word g_coppy(word s);
word g_subrcall(word t);
word g_return(word s);
word g_float(word s);
word g_fix(word s);
word g_datast(word b);
word g_subrpar(word s, word pp);
word g_prinpar(word n, word aa, word b, word c, word d);
word g_princal(word a, word b);
word g_ojumpop(word i);
word g_addr(word s);
void gen_code2_dsems(void);

/* ---- code3.imp --------------------------------------------------------------------------------------------------------- */

word g_subcall(word a, word b);
word g_csubcall(word a, word b);
word g_c0call(word a);
word g_cbnocall(word d);
word g_cbaddr(word a);
word g_cpcount(word d, word k);
word g_cpar(word s, word pp, word d, word k);
word g_subscript(word s, word t);
word g_bytep(word a, word s, word p, word flag);
word g_pcode(word s, word ns);
word g_pcode1(word s, word ns);
void gen_code3_dsems(void);

/* ---- demit.imp and the IR helpers of ix.imp ---------------------------------------------------------------------------- */

word g_demit(word a);
const char *g_irsymname(word v);
word g_irkind(word v);
word g_irstrval(word v);
const char *g_irlabname(word l);
word g_irpoolemit(word v);
word g_irfnkind(word v);
word g_irruntime(word v);
word g_irtagerr(word d);
const char *g_irjbname(word d);
word g_irvarrec(const char *sym, word sz);

/* the offsets of the record of the chain (src/llvm-ir/irsyn.imc) */
#define G_RECW 5
#define G_RNEXT(a)  HL(FREE(g_co + (a)))
#define G_RKIND(a)  (FREE(g_co + (a)) & 0377)
#define G_ROP(a)    ((word)(((uword)FREE(g_co + (a)) >> 8) & 0377))
#define G_RRD(a)    FREE(g_co + (a) + 1)
#define G_RV0(a)    FREE(g_co + (a) + 2)
#define G_RV1(a)    FREE(g_co + (a) + 3)
#define G_RX(a)     FREE(g_co + (a) + 4)

/* kinds of records, operations (irsyn.imc; numbers without a suffix there are decimal) */
enum { KOP = 1, KTAG = 2, KWORD = 3 };
enum {
    OLOAD = 1, ONEG = 2, OCOMPL = 3, OADDR = 4, OSTORE = 5, OADD = 6, OSUB = 7, OMUL = 8, ODIV = 9, OMOD = 10,
    OAND = 11, OANDCA = 12, OANDCM = 13, OANDCB = 14, OIOR = 15, OORCA = 16, OORCM = 17, OORCB = 18, OXOR = 19,
    OEQV = 20, OLSH = 21, OASH = 22, OROT = 23, OLDB = 24, ODPB = 25, OILDB = 26, OIDPB = 27, OADDX = 28,
    OFADD = 30, OFSUB = 31, OFMUL = 32, OFDIV = 33, OFLOAT = 34, OFIX = 35, ONEGF = 36,
    OJUMP = 40, OJEQ = 41, OJLT = 42, OJLE = 43, OJGE = 44, OJGT = 45, OJNE = 46, OFJ = 10,
    OCALLB = 60, OARG = 61, OCALLE = 62, OPITEM = 63, OCALLI = 64, OXCT = 65, OHRLM = 66, OHRRM = 67, OREMOTE = 68,
    OENTRY = 70, ORET = 71, OPARAM = 72
};

#endif
