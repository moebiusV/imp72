/*
 * pcode.c -- a listing of the code of a stack entry or a chain in PDP-10
 * terms, for looking at what the code generating routines made.  Stands in
 * for PCODE1 of debug.imc and ALIST of amod.imc.  The mnemonics are the
 * PDP-10's (opcodes the compiler never makes are listed in octal).
 */
#include <string.h>
#include "imp.h"

static const char *const fadd_[8] = { "FAD", "FADR", "FSB", "FSBR", "FMP", "FMPR", "FDV", "FDVR" };
static const char *const bool_[16] = { "SETZ", "AND", "ANDCA", "SETM", "ANDCM",
    "SETA", "XOR", "OR", "ANDCB", "EQV", "SETCA", "ORCA", "SETCM", "ORCM",
    "ORCB", "SETO" };
static const char *const half_[16] = { "HLL", "HRL", "HLLZ", "HRLZ", "HLLO",
    "HRLO", "HLLE", "HRLE", "HRR", "HLR", "HRRZ", "HLRZ", "HRRO", "HLRO",
    "HRRE", "HLRE" };
static const char *const jcond[8] = { "", "L", "E", "LE", "A", "GE", "N", "G" };
static const char *const jbase[8] = { "CAI", "CAM", "JUMP", "SKIP", "AOJ", "AOS",
    "SOJ", "SOS" };

/* the mnemonic of opcode op */
void
opname(word op, char *buf)
{
    static const char *const arith[] = { "MOVE", "MOVS", "MOVN", "MOVM", "IMUL",
        "MUL", "IDIV", "DIV" };
    static const char *const m4[4] = { "", "I", "M", "B" };
    static const char *const m4s[4] = { "", "I", "M", "S" };
    static const char *const m4l[4] = { "", "L", "M", "B" };

    if (op >= 0140 && op <= 0177) {
        int r = (int)((op >> 2) & 1);
        sprintf(buf, "%s%s", fadd_[(op - 0140) >> 2],
            r ? m4[op & 3] : m4l[op & 3]);
        if (r && (op & 3) == 1)
            strcpy(buf + strlen(fadd_[(op - 0140) >> 2]), "I");
        return;
    }
    if (op >= 0200 && op <= 0237) {
        int k = (int)((op - 0200) >> 2);

        sprintf(buf, "%s%s", arith[k], k < 4 ? m4s[op & 3] : m4[op & 3]);
        return;
    }
    if (op >= 0270 && op <= 0277) {
        sprintf(buf, "%s%s", (op & 4) ? "SUB" : "ADD", m4[op & 3]);
        return;
    }
    if (op >= 0300 && op <= 0377) {
        sprintf(buf, "%s%s", jbase[(op >> 4) & 7 ? ((op - 0300) >> 3) : 0],
            jcond[op & 7]);
        return;
    }
    if (op >= 0400 && op <= 0477) {
        sprintf(buf, "%s%s", bool_[(op - 0400) >> 2], m4[op & 3]);
        return;
    }
    if (op >= 0500 && op <= 0577) {
        sprintf(buf, "%s%s", half_[(op - 0500) >> 2], m4s[op & 3]);
        return;
    }
    switch (op) {
    case 0047: strcpy(buf, "CALLI"); return;
    case 0132: strcpy(buf, "FSC"); return;
    case 0133: strcpy(buf, "IBP"); return;
    case 0134: strcpy(buf, "ILDB"); return;
    case 0135: strcpy(buf, "LDB"); return;
    case 0136: strcpy(buf, "IDPB"); return;
    case 0137: strcpy(buf, "DPB"); return;
    case 0240: strcpy(buf, "ASH"); return;
    case 0241: strcpy(buf, "ROT"); return;
    case 0242: strcpy(buf, "LSH"); return;
    case 0244: strcpy(buf, "ASHC"); return;
    case 0245: strcpy(buf, "ROTC"); return;
    case 0246: strcpy(buf, "LSHC"); return;
    case 0250: strcpy(buf, "EXCH"); return;
    case 0251: strcpy(buf, "BLT"); return;
    case 0254: strcpy(buf, "JRST"); return;
    case 0256: strcpy(buf, "XCT"); return;
    case 0264: strcpy(buf, "JSR"); return;
    case 0266: strcpy(buf, "JSA"); return;
    case 0267: strcpy(buf, "JRA"); return;
    default:
        sprintf(buf, "%03lo", (long)op);
    }
}

/* the code of a chain from index iw (CB) */
void
pcode_chain(word iw)
{
    FILE *o = imp_out();
    word w, w2, op, ac, ix, v, c;
    char nm[16];

    for (;;) {
        w = FREE(co + iw);
        switch (u36(w) >> 34) {
        case 2:
            if ((iw = w & MASK18) == 0)
                return;
            continue;
        case 1:
            fprintf(o, "%6ld          SF%ld  ", (long)iw,
                (long)(0177 & (u36(w) >> 27)));
            if ((0177 & (u36(w) >> 27)) == 0) {         /* a tag */
                pname(w & MASK18);
                fputc(':', o);
            } else
                fprintf(o, "%lo", (long)(w & MASK18));
            fputc('\n', o);
            iw++;
            continue;
        case 3:
            w2 = FREE(co + iw + 1);
            fprintf(o, "%6ld          CONST %012lo", (long)iw, (long)u36(w2));
            if (w & MASK18) {
                fputc(' ', o);
                pname(w & MASK18);
            }
            fputc('\n', o);
            iw += 2;
            continue;
        default:
            break;
        }
        w2 = FREE(co + iw + 1);
        op = 0777 & (u36(w) >> 24);
        ac = 03777 & (u36(w) >> 12);
        ix = 03777 & u36(w);
        v = w2 & MASK18;
        c = L18(w2);
        opname(op, nm);
        fprintf(o, "%6ld          %-6s ", (long)iw, nm);
        if (ac)
            fprintf(o, "R%ld,", (long)ac);
        if (u36(w) & 0100000000000L)
            fputc('@', o);
        if (u36(w) & 04000)
            fprintf(o, "R%ld", (long)v);
        else if (v)
            pname(v);
        else
            fprintf(o, "%lo", (long)c);
        if ((u36(w) & 04000) == 0 && v && c)
            fprintf(o, "%+ld", (long)(c & 0400000 ? c - 01000000 : c));
        if (ix)
            fprintf(o, "(R%ld)", (long)ix);
        fputc('\n', o);
        iw += 2;
    }
}

/* PCODE(S,NS): list the code of stack entry s */
void
pcode(word s)
{
    word cb = u36(FREE(s + 2)) >> 18;

    if (cb)
        pcode_chain(cb);
}
