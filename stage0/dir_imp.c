/*
 * dir_imp.c -- dir.c's interface, over the core of src/dir.imp (HASH, DIR, DSEQ, DNAME)
 * compiled by imp72stage0.  The rest stays here.  Ports decus/dir.imc: DIR, DSEQ, PNAME,
 * DNAME, GNAME, DSTATS, DSPILL, RADI50, DORGFIX, CONVC, STCON, NEWNAME.
 *
 * A hash coded directory.  Free array PRP is a hash table of 512 buckets of
 * three word nodes (indices 3*h), followed by overflow nodes added at NPRP.
 * A name's "directory index" is the index in PRP of its node, so it is
 * the same number the PDP-10 compiler computes: the hash below is the
 * original's, taken over the first five characters packed as they were.
 *
 *   node word 0   HL: sibling (next node with a colliding name)  HR: ALF index
 *   node word 1   HL: 'ENTRY'  HR: 'NODE'
 *   node word 2   HR: 'SEM';  above it the small fields of dprop()
 *
 * Free array ALF holds the names themselves, concatenated: five 7-bit
 * characters per word, left justified in the low 36 bits, with a NUL
 * terminator that costs a word when the length is a multiple of five.
 */
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "imp.h"

extern word prp, alf;               /* PRP, ALF: defined by the IMP module */
extern word dm, dn;                 /* DM, DN: buckets used; overflow nodes */
extern word impl_dir(word *), impl_dseq(word *), impl_dname(word *, word *), impl_pname(word *), impl_dstats(word *),
    impl_dspill(word *), impl_radi50(word *), impl_convc(word *), impl_stcon(word *), impl_newname(word *);

/* DIR(NM): the directory index of a name, entering it if new.  The IMP routine takes the bytes in words. */
word
dir(const char *nm)
{
    word buf[64];
    size_t len = strlen(nm);

    if (len >= sizeof buf - 8) {
        imp_error(0, "NAME TOO LONG IN DIR. ");
        return 0;
    }
    memset(buf, 0, sizeof buf);
    memcpy(buf, nm, len);
    return impl_dir(buf);
}

/* DNAME(I,J): the jth word (five characters) of the name of entry i. */
word
dname(word i, word j)
{
    return impl_dname(&i, &j);
}

/* DSEQ(N): step through all entries.  Start with N < 0; ends with N < 0. */
word
dseq(word n)
{
    return impl_dseq(&n);
}

/* The name of entry i as a C string: the words of ALF hold its characters, the first in the low byte, and end with a NUL (valid until the arena moves). */
static const char *
fullname(word i, size_t limit)
{
    static char buf[4096];
    size_t n = 0;
    word j = HR(FREE(prp + i));

    for (; n < limit && n + 1 < sizeof buf; n++) {
        buf[n] = ((const char *)&fmem[alf + j])[n];
        if (buf[n] == '\0')
            break;
    }
    buf[n < sizeof buf ? n : sizeof buf - 1] = '\0';
    return buf;
}

/* DNAME_STR(I,BUF,N): the whole name of entry i, up to n-1 characters. */
void
dname_str(word i, char *buf, size_t n)
{
    const char *s = fullname(i, n - 1);

    strncpy(buf, s, n - 1);
    buf[n - 1] = '\0';
}

/* ---- node properties (DPROP and DPROPS of impsyn.imc) ------------------ */

/* Small fields of node word 2, above the SEM halfword. */
static const struct { int shift, width; } fld[] = {
    [DP_ATYPE] = { 32, 3 }, [DP_COM] = { 35, 2 }, [DP_ISNM] = { 37, 2 },
    [DP_LXTY] = { 39, 2 },  [DP_USE] = { 41, 3 },
};

word
dprop(enum dprop p, word i)
{
    switch (p) {
    case DP_NAME:  return alf + HR(FREE(prp + i));    /* pointer to the name */
    case DP_ENTRY: return HL(FREE(prp + i + 1));
    case DP_NODE:  return HR(FREE(prp + i + 1));
    case DP_SEM:   return HR(FREE(prp + i + 2));
    default:
        return (word)(((uword)FREE(prp + i + 2) >> fld[p].shift)
            & (((uword)1 << fld[p].width) - 1));
    }
}

void
dprops(enum dprop p, word i, word v)
{
    word w;
    uword m;

    switch (p) {
    case DP_NAME:
        imp_error(0, "DPROPS: THE NAME PROPERTY IS READ ONLY. ");
        break;
    case DP_ENTRY:
        w = FREE(prp + i + 1);
        FREES(prp + i + 1, HW(v, HR(w)));
        break;
    case DP_NODE:
        w = FREE(prp + i + 1);
        FREES(prp + i + 1, HW(HL(w), v));
        break;
    case DP_SEM:
        w = FREE(prp + i + 2);
        FREES(prp + i + 2, (word)(((uword)w & ~(uword)HW_MASK)
            | ((uword)v & HW_MASK)));
        break;
    default:
        m = (((uword)1 << fld[p].width) - 1) << fld[p].shift;
        w = FREE(prp + i + 2);
        FREES(prp + i + 2, (word)(((uword)w & ~m)
            | (((uword)v << fld[p].shift) & m)));
        break;
    }
}

/* ---- the rest of the directory package is IMP (src/dir.imp) -------------------------------------------------------------- */

void pname(word i) { impl_pname(&i); }
void dspill(void) { word z = 0; impl_dspill(&z); }
void dstats(void) { word z = 0; impl_dstats(&z); }
word radi50(word i) { return impl_radi50(&i); }
word convc(word c) { return impl_convc(&c); }
word stcon(word v) { return impl_stcon(&v); }

/* the first 24 characters of the name, as a C string (valid until the next call) */
const char *
gname(word i)
{
    return fullname(i, 24);
}

word
newname(const char *s)
{
    word buf[8];
    size_t len = strlen(s);

    memset(buf, 0, sizeof buf);
    memcpy(buf, s, len < 8 * sizeof buf[0] - 1 ? len : 8 * sizeof buf[0] - 1);
    return impl_newname(buf);
}
