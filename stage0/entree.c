/*
 * entree.c -- the code tree.  Ports src/entree.imp (decus/entree.imc): ENTREE, ENTMARK, INSNO,
 * NUNO, COMPNO, CODEID, MATCH, ENTRCA, ENTRC1, TPRINT, and the tree accessors
 * TSCR, TALT, TNODEN that impsyn64.imc expands in line.  The 36 bit formats of
 * the tape are gone: the words are those of src/entree.imp.
 *
 * The code tree is kept in free array CT, in nodes of two or three words
 * (the node is named by the index of its links word):
 *
 *   word -1   the "special" field: the first production node among this
 *             node's alternates (set by INSNO)
 *   word 0    links: SCR (successor), ALT (alternate), NODEN (node number),
 *             three fields of 20 bits (SCR 0-19, ALT 20-39, NODEN 40-59)
 *   word 1    the stuff word, with the node type in bits 61-63 (bit 63 is
 *             free: ENTRCA marks nodes with it, NMARK):
 *               2  match the kth item of the pattern (k in the right half)
 *               3  match identifier id (dir index in the right half; if NCLS
 *                  is set the id is a one word constant whose value is in
 *                  word 2 of the node, and matching by value is also tried)
 *               1  match a stack item of a kind given by the other bits:
 *
 *      BITS   61-63  60 59  58 57 56 55 54 53  24-31 16-23  8-15  0-7
 *             TTT     X  I   W  S  V  C  R  N    PP    QQ    KKK   LLL
 *
 *     X, I   floating point / integer may match
 *     W      only expressions of one word of code (IMP10)
 *     S, V   any subscripted / simple variable may match
 *     C, R, N  a constant / register / raw identifier might match, the
 *            constant being limited by KKK, LLL, the register by PP, QQ
 *     (as in entree.imc's header comment, where the rest is explained)
 *     With WSVCRN = 0 the node matches productions: NTRM says it is a
 *     terminal node (MATCHP finds the semantics), NCLS that it is a class of
 *     VAL/ productions.  A production node is one whose HL is at most PLIM.
 */
#include <string.h>
#include "imp.h"
#include "gen.h"

word ct, nct, noden = -1, sbitm[3];
static word prods, nprods_, tbl, ntbl, ply;
static int init;

#define LK_MASK ((word)0xFFFFFL)
word tscr(word n) { return FREE(ct + n) & LK_MASK; }
word talt(word n) { return (FREE(ct + n) >> 20) & LK_MASK; }
word tnoden(word n) { return (FREE(ct + n) >> 40) & LK_MASK; }
static void
stscr(word n, word v)
{
    FREES(ct + n, (FREE(ct + n) & ~LK_MASK) | (v & LK_MASK));
}
static void
stalt(word n, word v)
{
    FREES(ct + n, (FREE(ct + n) & ~(LK_MASK << 20)) | ((v & LK_MASK) << 20));
}
static void
stnoden(word n, word v)
{
    FREES(ct + n, (FREE(ct + n) & ~(LK_MASK << 40)) | ((v & LK_MASK) << 40));
}

static void
torgfix(void)
{
}

#define STUFF(n) FREE(ct + (n) + 1)

/* NUNO(A,B,C): a new node with stuff word a, and, if b, a constant c. */
static word
nuno(word a, word b, word c)
{
    word q, aa;

    nct++;
    if (flen(ct) <= nct + 4)
        faddex(&ct, nct + 4);
    q = nct;
    FREES(ct + nct, 0);
    stnoden(q, ++noden);
    aa = a;
    if (aa == (NT1 | NTRM))
        aa |= q;
    FREES(ct + nct + 1, aa);
    nct += 2;
    if (b) {
        FREES(ct + nct, c);
        nct++;
    }
    return q;
}

/*
 * COMPNO(NO,VV): compare node no with the stuff word vv.  1, 0, -1 as vv
 * belongs above, is equal to, or belongs below node no.  Nodes are ordered
 * so that the more specific cases are met first: by the restriction byte
 * WSVCRN (as a number with W complemented; the lower the more restrictive),
 * then the arithmetic type, the constant and register limits.  Production
 * nodes go last, terminal ones after the others.
 */
static word
compno(word no, word vv)
{
    word v = FREE(ct + no + 1), tv, tvv, kv, kvv, lv, lvv;

    if (v == vv)
        return 0;
    tv = rs64(v, 61);
    if (tv == 1)
        tv = 4;
    tvv = rs64(vv, 61);
    if (tvv == 1)
        tvv = 4;
    if (tvv < tv)
        return 1;
    if (tvv == tv) {
        if (tv != 4)
            return 1;
        tv = HL(v);                             /* production nodes? */
        tvv = HL(vv);
        if (tv <= PLIM) {
            if (tv == tvv && tv == PLIM)
                return 0;
            if (tvv > PLIM)
                return 1;
            if (tv > tvv)
                return 1;
            return -1;
        }
        if (tvv <= PLIM)
            return -1;
        tv = 077 & (040 + rs64(v, 53));         /* WSCVRN */
        tvv = 077 & (040 + rs64(vv, 53));
        if (tvv < tv)
            return 1;
        if (tvv == tv) {
            tv = rs64(v, 59);                   /* arithmetic type */
            tvv = rs64(vv, 59);
            if (tvv < tv)
                return 1;
            if (tvv == tv) {
                kv = 0177 & rs64(v, 8);
                kvv = 0177 & rs64(vv, 8);
                lv = 0177 & v;
                lvv = 0177 & vv;
                if (kvv + lvv > kv + lv)        /* constants */
                    return 1;
                if (kvv + lvv == kv + lv) {
                    if (lvv > lv)
                        return 1;
                    if (lvv == lv) {
                        kv = 037 & rs64(v, 24); /* registers */
                        kvv = 037 & rs64(vv, 24);
                        if (kvv > kv)
                            return 1;
                        if (kvv == kv) {
                            kv = 037 & rs64(v, 16);
                            kvv = 037 & rs64(vv, 16);
                            if (kvv < kv)
                                return 1;
                        }
                    }
                }
            }
        }
    }
    return -1;
}

/*
 * INSNO(HOOK,STUFF,K1,K2): insert a node as a successor of hook with stuff
 * word `stuff` (and constant k2 if k1), in its place among the alternates, or
 * find the equal one already there.  Returns the node.
 */
static word
insno(word hook, word stuff, word k1, word k2)
{
    word sh, lsh, k, j;

    ply++;
    sh = tscr(hook);
    if (sh == 0) {
        j = nuno(stuff, k1, k2);
        stscr(hook, j);
        goto insx;
    }
    k = compno(sh, stuff);
    if (k == 0)
        goto insxx;
    if (k > 0) {
        j = nuno(stuff, k1, k2);
        stscr(hook, j);
        stalt(j, sh);
        goto insx;
    }
    for (;;) {
        lsh = sh;
        sh = talt(sh);
        if (sh == 0) {
            j = nuno(stuff, k1, k2);
            stalt(lsh, j);
            goto insx;
        }
        k = compno(sh, stuff);
        if (k) {
            if (k < 0)
                continue;
            j = nuno(stuff, k1, k2);
            stalt(j, sh);
            stalt(lsh, j);
            goto insx;
        }
        break;
    }
insxx:
    j = sh;
insx:
    /* set the special field to the first production node among the
     * alternates */
    sh = tscr(hook);
    while (sh) {
        if (HL(FREE(ct + sh + 1)) <= PLIM)
            break;
        sh = talt(sh);
    }
    k = tscr(hook);
    while (k != sh) {
        FREES(ct + k - 1, sh);
        k = talt(k);
    }
    return j;
}

/* ENTMARK(J): set the bit matrix SBITM for production row j at the nodes
 * just entered, back to the last production node. */
static void
entmark(word j)
{
    word k = 0, l, i;

    for (i = nprods_ - 1; i >= 0; i--) {
        l = FREE(prods + i);
        if (k && HL(FREE(ct + l + 1)) <= PLIM)
            return;
        k = -1;
        bits(sbitm, tnoden(l), j);
    }
}

/* ---- CODEID: taking an identifier of a semantic pattern apart ----------- */

static word cid_char, cid_sh, cid_lch, cid_iddir, cid_iddirx, cid_ans;
static uword cid_id;
static word n1, n2;

/*
 * CODEID(SW): disassemble an identifier (src/entree.imp).  With sw nonzero (a
 * directory index), start on that identifier.  With sw = 0, return the
 * directory index of the next name in it, or minus (1 + the index in REG CON
 * CNG VAR MEM WRD IGR FLT BYT NAM [NSE counted as WRD]) of a special
 * indicator, with n1 and n2 set to its numeric arguments or -1; 0 when done.
 * The characters 0 end a name, so Q0REG0115 is the name Q and the indicator
 * REG with 01 and 15.  The characters are the bytes of the name, the first
 * in the low byte of a word.
 */
static word
codeid(word sw)
{
    static const char *const tb[11] = { "REG", "CON", "CNG", "VAR", "MEM",
        "WRD", "IGR", "FLT", "BYT", "NAM", "NSE" };
    static char na[84];                 /* NA: the name being made (10 words of 8 characters, and its end) */
    word nsw = 0, nan = 0, c;

    if (sw) {
        cid_char = 0;
        cid_sh = 64;                    /* CHBEYOND */
        cid_lch = cid_char = -1;
        cid_iddir = sw;
        cid_iddirx = -1;
        return cid_ans;
    }
    cid_ans = 0;
    na[0] = '\0';
    if (cid_lch == 0) {
        cid_ans = 0;
        return cid_ans;
    }
    n1 = n2 = -1;
    if (cid_char <= 0) {
nxch:
        if (cid_lch == 0) {
            cid_char = 0;
            goto l9;
        }
        if (cid_sh > 56) {              /* CHPAST(SH) */
            cid_iddirx++;
            cid_id = (uword)dname(cid_iddir, cid_iddirx);
            cid_sh = 0;
        }
        cid_lch = cid_char = (word)((cid_id >> cid_sh) & 0377);
        cid_sh += 8;
    }
l9:
    if (nsw)
        goto l3;
    for (c = 10; c >= 0; c--) {         /* the name so far is one of the three letter words */
        if (nan == 3 && strcmp(na, tb[c]) == 0)
            goto l2;
    }
    if (cid_ans == 0) {                 /* make a name */
        if (cid_char != 0 && cid_char != 060) {
            na[nan++] = (char)cid_char;
            na[nan] = '\0';
            if (nan >= 80)
                imp_error(0, "NAME TOO LONG **");
            goto nxch;
        }
        cid_ans = dir(na);
        cid_char = 0;
        return cid_ans;
    }
l2:
    nsw = 1;
    if (c == 10)
        c = 5;
    cid_ans = -(c + 1);
    if (c > 2)
        return cid_ans;
l3:
    if (nsw == 5)
        return cid_ans;
    if (cid_char >= 060 && cid_char <= 071) {
        switch (nsw - 1) {
        case 0:
            n1 = 10 * (cid_char - 060);
            break;
        case 1:
            n1 = n1 + (cid_char - 060);
            break;
        case 2:
            n2 = 10 * (cid_char - 060);
            break;
        default:
            n2 = n2 + (cid_char - 060);
            break;
        }
        nsw++;
        goto nxch;
    }
    if (nsw & 1)
        return cid_ans;
    imp_error(1, "NUMBER MISSING OR OUT OF PLACE IN SEMANTICS IDENTIFIER **");
    cid_ans = 0;
    return cid_ans;
}

/*
 * ENTREE(S,CS,SR,PN): enter in the tree the parser output in free array s
 * (negative directory indices of identifiers and positive production numbers,
 * the instance of production cs of the quoted semantics, ended by 0) so that
 * semantic routine sr is invoked for production pn (which differs from cs
 * if a case is being defined).  Returns a free array of the directory
 * indices of the identifier of each terminal of s (0 for a constant), ended
 * by a negative.  Alters s.
 *
 * The parser output is cut for the tree: everything above the highest
 * occurrence of cs goes; [1] semantics go; a non-name identifier whose first
 * production above has one argument is taken to be <NAM> ::= NAM" and goes.
 */
word
entree(word s, word cs_, word sr, word pn)
{
    word node, noty, nty1, kon, st, ss, eatit, j, jj, k, i, sno;

    ply = ntbl = 0;
    node = 1;
    if (!init) {
        init = 1;
        frelot(&ct, "TREE", 550, torgfix);
        sbitm[0] = 0;
        sbitm[1] = 340;
        sbitm[2] = 4;
        nct = 3;
        fincset(ct, 40);
        FREES(ct + 1, 0);
        FREES(ct + 2, 0);
        frelot(&prods, "NTRE2", 10, NULL);
        noden = -1;
        frelot(&tbl, "NTREE", 10, NULL);
    }
    st = s;
    nprods_ = 0;
    while (FREE(st))
        st++;
    for (;;) {
        if (--st < s) {
            imp_error(1, "QUOTED SEMANTICS NOT AN INSTANCE OF PRODUCTION.");
            goto entrx;
        }
        if (FREE(st) == cs_)
            break;
    }
    ss = s;
    eatit = 0;
next:
    j = FREE(ss);
    ss++;
    if (j < 0) {                                /* an identifier */
        eatit = 1;
        kon = convc(-j);
        if (kon) {
            noty = NT3 | NCLS | -j;
            fadd(&tbl, &ntbl, 0);
            goto l11;
        }
        codeid(-j);
        j = codeid(0);
        fadd(&tbl, &ntbl, j);
        if (j > 0) {                            /* starts with a name: seen? */
            if (ntbl > 1) {
                for (i = 0; i <= ntbl - 2; i++) {
                    k = FREE(tbl + i);
                    if (k == j) {
l22:                    j = codeid(0);
                        if (j != 0) {
                            if (j == -10) {
                                eatit = 0;
                                goto l22;
                            }
                            pname(k);
                            fputc(' ', imp_out());
                            imp_error(1, "MODIFIER ON OTHER THAN FIRST "
                                "OCCURRENCE OF NAME IS IGNORED");
                            goto l22;
                        }
                        noty = i | NT2;
                        goto l11;
                    }
                }
            }
            j = codeid(0);
        }
        /* first occurrence of the id: look for restrictions */
        noty = NT1 | ls64(3, 59);
        nty1 = ls64(037, 53) | ls64(037, 16) | 0177;
l12:
        if (j) {
            if (j > 0) {
                pname(j);
                fputc(' ', imp_out());
                imp_error(1, "BAD MODIFIER **");
                goto l11;
            }
            k = -j;
            if (k > 2)
                k--;
            if (k == 9)
                k = eatit = 0;
            if (k == 8) {
                noty &= ~ls64(3, 59);
                goto l21;
            }
            if (k > 5) {
                noty &= ~ls64(1, 66 - k);
                goto l21;
            }
            if (k == 5) {
                noty |= ls64(1, 58);
                goto l21;
            }
            nty1 = 0;
            noty |= ls64(1, k + 53);
            if (j == -3)
                noty |= 0200;
            if (k == 2) {
                if (n1 < 0)
                    n1 = 0177;
                if (n2 < 0)
                    n2 = 0;
                noty |= n2 | ls64(n1, 8);
            }
            if (k == 1) {
                if (n1 < 0) {
                    n1 = 0;
                    n2 = 037;
                }
                if (n2 < 0)
                    n2 = n1;
                noty |= ls64(n1, 24) | ls64(n2, 16);
            }
l21:
            j = codeid(0);
            goto l12;
        }
        noty |= nty1;
l11:
        node = insno(node, noty, kon, convc_const);
        fadd(&prods, &nprods_, node);
        goto next;
    }
    if (ss <= st) {                             /* a production */
        if (j == 1)
            goto next;
        jj = j;
        if (HL(j)) {                            /* a VAL/ production */
            fadd(&tbl, &ntbl, j);
            j = HR(j);
            jj = NCLS | j;
        }
        if (eatit) {
            /* eat the first semantics node with one arg after an identifier */
            eatit = 0;
            k = snarg(j);
            if (k == 1)
                goto next;
        }
        eatit = 0;
        if (j == cs_ && cs_ != pn)
            imp_error(2, "CASE CONTAINS SUBEXPRESSION WHICH IS CASE BUT WONT "
                "GENERALIZE.");
        noteprod(j, ply);
        sno = snoden(j);
        node = insno(node, jj | NT1, 0, 0);
        fadd(&prods, &nprods_, node);
        entmark(sno);
        goto next;
    }
    /* made the production: end off the tree */
    noteprod(pn, ply);
    node = insno(node, NT1 | NTRM, 0, 0);
    enprod(pn, node, sr);
    fadd(&prods, &nprods_, node);
    j = snoden(pn);
    entmark(j);
entrx:
    fadd(&tbl, &ntbl, -1);
    return tbl;
}

/*
 * MATCH(TN,SN,SNONR): does tree node tn match the stack entry at sn?  0 if
 * not, -1 if so, -2 if so for a VAL/ production read as its original, or at a
 * terminal node the semantics entry.  tn's alternates are tried in turn;
 * *tnp is left at the node that matched (the original's TN is a reference
 * parameter that COTREE reads afterwards).
 */
word
match_(word *tnp, word sn, word snonr)
{
    word tn = *tnp, val = -1, s = FREE(sn), sty = s & 037, sdi = HL(s), sval = FREE(sn + 1),
        t, tty, tval, j, snn, mask, rest;

    if (sty == 0) {
ty0al0:
        if (rs64(FREE(ct + 1 + tn), 53) != 0400) {
            tn = FREE(ct + tn - 1);
            if (tn)
                goto ty0al0;
            { *tnp = tn; return 0; }
        }
        t = FREE(tn + ct + 1);
        if (t & NTRM) {                         /* a terminal node */
            val = matchp(sdi, tn);
            if (val == 0)
                goto ty0alt;
            { *tnp = tn; return val; }
        }
        if (HR(t) == sdi)
            { *tnp = tn; return -1; }
        if (sval == 0)
            goto ty0alt;
        if (HR(t) == HL(sval))
            { *tnp = tn; return -2; }
ty0alt:
        tn = talt(tn);
        if (tn)
            goto ty0al0;
        { *tnp = tn; return 0; }
    }
    snn = snonr;
tyalt:
    if (snn >= 0 && bit(sbitm, tnoden(tn), snn) == 0)
        goto nix;
    t = FREE(tn + ct + 1);
    tty = rs64(t, 61);
    if (tty == 1) {
        if ((rs64(t, 53) & 077) == 0)           /* productions are last */
            { *tnp = tn; return 0; }
        j = s & rs64(t, 53);
        if ((037 & j) == 0)
            goto nix;
        if ((j >> 6) == 0) {
            if ((3 & rs64(t, 59)) != 3)
                goto nix;
        }
        if ((sty & 6) == 0)
            goto mex;
        if (sty == 2)
            goto mex;                           /* RMATCH does not decide */
        /* does the constant meet the requirements? */
        if ((t & 0177777) == 0177)
            goto mex;
        mask = ls64(ls64(1, rs64(t, 8) & 0177) - 1, t & 0177);
        if (mask == 0)
            goto mex;
        mask = ~mask;
        rest = 0;
        if (t & 0200)
            rest = mask;
        if ((sval & mask) == rest)
            goto mex;
        goto nix;
    }
    tval = HR(t);
    if (tty == 0) {
        val = tval;
        goto mex;
    }
    if (tty == 2) {
        if ((val = sameas(sn, tval)) == 0)
            goto nix;
        goto mex;
    }
    /* match with an identifier: dir indices equal and no offset */
    if (FREE(sn + 2))
        goto nix;
    if (sdi == tval && sval == 0)
        goto mex;
    if ((t & NCLS) && sty == 4) {
        j = FREE(ct + tn + 2);
        if (j == sval)
            goto mex;
    }
    goto nix;
mex:
    if ((t & ls64(1, 58)) && g_oneword(sn) == 0)
        goto nix;
    if (val == 0)
        val = -1;
    { *tnp = tn; return val; }
nix:
    tn = talt(tn);
    if (tn)
        goto tyalt;
    { *tnp = tn; return 0; }
}

word
match(word *tnp, word sn, word snonr)
{
    return match_(tnp, sn, snonr);
}

/* ENTRC1: step through the tree depth first, using the pushdown stack; 0 at
 * the end (ENTRNO is then back at the root, ready for another walk). */
static word entrno = 1;

static word
entrc1(void)
{
    word j;

    if ((j = tscr(entrno))) {
        pushmi(entrno);
        return entrno = j;
    }
    for (;;) {
        if ((entrno = talt(entrno)))
            return entrno;
        if ((entrno = pullyu(0)) == 1)
            return 0;
    }
}

/*
 * ENTRCA(SEMN): called when semn is a case of another production; completes
 * SBITM by brute force, marking in the sign bit of the stuff word.
 */
void
entrca(word semn)
{
    word i, j, flag, s;

    entrno = 1;
    while ((i = entrc1())) {
        if (HL(FREE(ct + i + 1)) == PLIM && matchp(semn, i))
            FREES(ct + i + 1, FREE(ct + i + 1) | NMARK);
    }
    do {
        flag = 0;
        while ((i = entrc1())) {
            if (HLS(FREE(ct + i + 1)) > PLIM) {
                j = tscr(i);
                while (j) {
                    if (FREE(ct + j + 1) < 0) {
                        FREES(ct + i + 1, FREE(ct + i + 1) | NMARK);
                        flag = -1;
                        j = 0;
                    } else
                        j = talt(j);
                }
            }
        }
    } while (flag != 0);
    s = snoden(semn);
    while ((i = entrc1())) {
        if (FREE(ct + i + 1) < 0) {
            FREES(ct + i + 1, FREE(ct + i + 1) & ~NMARK);
            bits(sbitm, tnoden(i), s);
        }
    }
}

void
tprint(void)
{
    tprin1();
}
