/*
 * entree.c -- the code tree.  Ports decus/entree.imc: ENTREE, ENTMARK, INSNO,
 * NUNO, COMPNO, CODEID, MATCH, ENTRCA, ENTRC1, TPRINT, and the tree accessors
 * TSCR, TALT, TNODEN that impsyn.imc expands in line.
 *
 * The code tree is kept in free array CT, in nodes of two or three words
 * (the node is named by the index of its links word):
 *
 *   word -1   the "special" field: the first production node among this
 *             node's alternates (set by INSNO)
 *   word 0    links: SCR (successor), ALT (alternate), NODEN (node number)
 *   word 1    the stuff word, with the node type in bits 35-33:
 *               2  match the kth item of the pattern (k in the low 18 bits)
 *               3  match identifier id (dir index in the low 18 bits; if bit
 *                  18 is set the id is a one word constant whose value is in
 *                  word 2 of the node, and matching by value is also tried)
 *               1  match a stack item of a kind given by the other bits:
 *
 *      3   2 111 111  5  5  7   8
 *     TTT XI WSV CRN PP QQ KKK LLL
 *
 *     X, I   floating point / integer may match
 *     W      only expressions of one word of code (IMP10)
 *     S, V   any subscripted / simple variable may match
 *     C, R, N  a constant / register / raw identifier might match, the
 *            constant being limited by KKK, LLL, the register by PP, QQ
 *     (as in entree.imc's header comment, where the rest is explained)
 *     With WSVCRN = 0 the node matches productions: bit 19 of the stuff word
 *     says it is a terminal node (MATCHP finds the semantics), bit 18 that it
 *     is a class of VAL/ productions.
 *
 * The stuff words keep their 36 bit format.  The links word does not: the
 * original packs three 12 bit fields in 36 bits (its header comment says 18
 * and 18, which is older); here they are three 20 bit fields (SCR 0-19, ALT
 * 20-39, NODEN 40-59), so the tree is not limited to 4095 words.  Nothing
 * prints that word raw.
 */
#include <string.h>
#include "imp.h"

word ct, nct, noden = -1, sbitm[3];
static word prods, nprods_, tbl, ntbl, ply;
static int init;

#define LK_MASK 0xFFFFFL
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
    if (aa == 0100002000000L)
        aa |= q;
    FREES(ct + nct + 1, sx36(aa));
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
    tv = u36(v) >> 33;
    if (tv == 1)
        tv = 4;
    tvv = u36(vv) >> 33;
    if (tvv == 1)
        tvv = 4;
    if (tvv < tv)
        return 1;
    if (tvv == tv) {
        if (tv != 4)
            return 1;
        tv = u36(v) >> 18;                      /* production nodes? */
        tvv = u36(vv) >> 18;
        if (tv <= 0100002) {
            if (tv == tvv && tv == 0100002)
                return 0;
            if (tvv > 0100002)
                return 1;
            if (tv > tvv)
                return 1;
            return -1;
        }
        if (tvv <= 0100002)
            return -1;
        tv = 077 & (040 + (u36(v) >> 25));      /* WSCVRN */
        tvv = 077 & (040 + (u36(vv) >> 25));
        if (tvv < tv)
            return 1;
        if (tvv == tv) {
            tv = u36(v) >> 31;                  /* arithmetic type */
            tvv = u36(vv) >> 31;
            if (tvv < tv)
                return 1;
            if (tvv == tv) {
                kv = 0177 & (u36(v) >> 8);
                kvv = 0177 & (u36(vv) >> 8);
                lv = 0177 & u36(v);
                lvv = 0177 & u36(vv);
                if (kvv + lvv > kv + lv)        /* constants */
                    return 1;
                if (kvv + lvv == kv + lv) {
                    if (lvv > lv)
                        return 1;
                    if (lvv == lv) {
                        kv = 037 & (u36(v) >> 20);      /* registers */
                        kvv = 037 & (u36(vv) >> 20);
                        if (kvv > kv)
                            return 1;
                        if (kvv == kv) {
                            kv = 037 & (u36(v) >> 15);
                            kvv = 037 & (u36(vv) >> 15);
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
        if (L18(FREE(ct + sh + 1)) <= 0100002)
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
        if (k && L18(FREE(ct + l + 1)) <= 0100002)
            return;
        k = -1;
        bits(sbitm, tnoden(l), j);
    }
}

/* ---- CODEID: taking an identifier of a semantic pattern apart ----------- */

static word idtb[11];
static word cid_char, cid_sh, cid_lch, cid_iddir, cid_iddirx, cid_nsw, cid_ans;
static word n1, n2;

static void
codeid_init(void)
{
    static const char *const tb[11] = { "REG", "CON", "CNG", "VAR", "MEM",
        "WRD", "IGR", "FLT", "BYT", "NAM", "NSE" };
    int i;

    for (i = 0; i < 11; i++)
        idtb[i] = ((word)tb[i][0] << 14) | ((word)tb[i][1] << 7) | tb[i][2];
}

/*
 * CODEID(SW): disassemble an identifier.  With sw nonzero (a directory
 * index), start on that identifier.  With sw = 0, return the directory index
 * of the next name in it, or minus (1 + the index in REG CON CNG VAR MEM WRD
 * IGR FLT BYT NAM [NSE counted as WRD]) of a special indicator, with n1 and
 * n2 set to its numeric arguments or -1; 0 when done.  The characters 0 end a
 * name, so Q0REG0115 is the name Q and the indicator REG with 01 and 15.
 */
static word
codeid(word sw)
{
    static int cinit;
    word na0 = 0, nan = 0, c, id = 0;
    char piece[64];
    int np = 0, first_full = 0;

    if (!cinit) {
        cinit = 1;
        codeid_init();
    }
    if (sw) {
        cid_char = cid_sh = 0;
        cid_lch = cid_char = -1;
        cid_iddir = sw;
        cid_iddirx = -1;
        return cid_ans;
    }
    cid_nsw = cid_ans = 0;
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
        if (cid_sh < 1) {
            cid_iddirx++;
            id = dname(cid_iddir, cid_iddirx);
            cid_sh = 29;
        } else
            id = dname(cid_iddir, cid_iddirx);
        cid_lch = cid_char = 0177 & (id >> cid_sh);
        cid_sh -= 7;
    }
l9:
    if (cid_nsw)
        goto l3;
    for (c = 10; c >= 0; c--) {
        if (!first_full && np <= 3 && na0 == idtb[c])
            goto l2;
    }
    if (cid_ans == 0) {                 /* make a name */
        if (cid_char != 0 && cid_char != 060) {
            if (np < (int)sizeof piece - 1)
                piece[np++] = (char)cid_char;
            na0 = (na0 << 7) | cid_char;
            if (np >= 5)
                first_full = 1;
            nan = np;
            goto nxch;
        }
        piece[np] = '\0';
        cid_ans = dir(piece);
        cid_char = 0;
        return cid_ans;
    }
l2:
    cid_nsw = 1;
    if (c == 10)
        c = 5;
    cid_ans = -(c + 1);
    if (c > 2)
        return cid_ans;
l3:
    if (cid_nsw == 5)
        return cid_ans;
    if (cid_char >= 060 && cid_char <= 071) {
        switch (cid_nsw - 1) {
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
        cid_nsw++;
        goto nxch;
    }
    if (cid_nsw & 1)
        return cid_ans;
    imp_error(1, "NUMBER MISSING OR OUT OF PLACE IN SEMANTICS IDENTIFIER **");
    cid_ans = 0;
    (void)nan;
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
    word node, noty, nty1, kon, st, ss, eatit, j, jj, k, kk, i, sno;

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
            noty = 0300001000000L | -j;
            fadd(&tbl, &ntbl, 0);
            goto l11;
        }
        codeid(-j);
        j = codeid(0);
        kk = j;
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
                        noty = i | ((word)2 << 33);
                        goto l11;
                    }
                }
            }
            j = codeid(0);
        }
        /* first occurrence of the id: look for restrictions */
        noty = 0160000000000L;
        nty1 = 0007603700177L;
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
                noty &= ~(ls36(3, 31));
                goto l21;
            }
            if (k > 5) {
                noty &= ~(ls36(1, 38 - k));
                goto l21;
            }
            if (k == 5) {
                noty |= 010000000000L;
                goto l21;
            }
            nty1 = 0;
            noty |= ls36(1, k + 25);
            if (j == -3)
                noty |= 0200;
            if (k == 2) {
                if (n1 < 0)
                    n1 = 0177;
                if (n2 < 0)
                    n2 = 0;
                noty |= n2 | ls36(n1, 8);
            }
            if (k == 1) {
                if (n1 < 0) {
                    n1 = 0;
                    n2 = 037;
                }
                if (n2 < 0)
                    n2 = n1;
                noty |= ls36(n1, 20) | ls36(n2, 15);
            }
l21:
            j = codeid(0);
            goto l12;
        }
        noty |= nty1;
l11:
        node = insno(node, sx36(noty), kon, convc_const);
        fadd(&prods, &nprods_, node);
        goto next;
    }
    if (ss <= st) {                             /* a production */
        if (j == 1)
            goto next;
        jj = j;
        if (j >> 18) {                          /* a VAL/ production */
            fadd(&tbl, &ntbl, j);
            j &= 0777777;
            jj = 01000000 | j;
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
        node = insno(node, jj | 0100000000000L, 0, 0);
        fadd(&prods, &nprods_, node);
        entmark(sno);
        goto next;
    }
    /* made the production: end off the tree */
    noteprod(pn, ply);
    node = insno(node, 0100002000000L, 0, 0);
    enprod(pn, node, sr);
    fadd(&prods, &nprods_, node);
    j = snoden(pn);
    entmark(j);
entrx:
    fadd(&tbl, &ntbl, -1);
    return tbl;
}

/* ONEWORD(S): does the code of stack entry s take exactly one word?  Entries
 * without code do not. */
static word
oneword(word sn)
{
    if (FREE(sn + 2) == 0)
        return 0;
    imp_error(0, "ONEWORD: CODE IS NOT YET PORTED");
    return 0;
}

/*
 * MATCH(TN,SN,SNONR): does tree node tn match the stack entry at sn?  0 if
 * not, -1 if so, -2 if so for a VAL/ production read as its original, or at a
 * terminal node the semantics entry.  tn's alternates are tried in turn.
 */
word
match(word tn, word sn, word snonr)
{
    word val = -1, s = FREE(sn), sty = s & 037, sdi = L18(s), sval = FREE(sn + 1),
        t, tty, tval, j, snn, mask, rest;

    if (sty == 0) {
ty0al0:
        if (((u36(FREE(ct + 1 + tn)) >> 25) & 03777) != 0400) {
            tn = FREE(ct + tn - 1);
            if (tn)
                goto ty0al0;
            return 0;
        }
        t = FREE(tn + ct + 1);
        if (t & 02000000) {                     /* a terminal node */
            val = matchp(sdi, tn);
            if (val == 0)
                goto ty0alt;
            return val;
        }
        if (R18(t) == sdi)
            return -1;
        if (sval == 0)
            goto ty0alt;
        if (R18(t) == L18(sval))
            return -2;
ty0alt:
        tn = talt(tn);
        if (tn)
            goto ty0al0;
        return 0;
    }
    snn = snonr;
tyalt:
    if (snn >= 0 && bit(sbitm, tnoden(tn), snn) == 0)
        goto nix;
    t = FREE(tn + ct + 1);
    tty = u36(t) >> 33;
    if (tty == 1) {
        if (((u36(t) >> 25) & 077) == 0)        /* productions are last */
            return 0;
        j = s & (u36(t) >> 25);
        if ((037 & j) == 0)
            goto nix;
        if ((j >> 6) == 0) {
            word rot = ((u36(t) << 5) | (u36(t) >> 31)) & MASK36;

            if ((3 & rot) != 3)
                goto nix;
        }
        if ((sty & 6) == 0)
            goto mex;
        if (sty == 2)
            goto mex;                           /* RMATCH does not decide */
        /* does the constant meet the requirements? */
        if ((u36(t) & 077777) == 0177)
            goto mex;
        mask = ls36(ls36(1, (u36(t) >> 8) & 0177) - 1, u36(t) & 0177);
        if (mask == 0)
            goto mex;
        mask = sx36(~mask);
        rest = 0;
        if (t & 0200)
            rest = mask;
        if ((sval & mask) == rest)
            goto mex;
        goto nix;
    }
    tval = R18(t);
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
    if ((t & 01000000) && sty == 4) {
        j = FREE(ct + tn + 2);
        if (j == sval)
            goto mex;
    }
    goto nix;
mex:
    if ((t & 010000000000L) && oneword(sn) == 0)
        goto nix;
    if (val == 0)
        val = -1;
    return val;
nix:
    tn = talt(tn);
    if (tn)
        goto tyalt;
    return 0;
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
        if (L18(FREE(ct + i + 1)) == 0100002 && matchp(semn, i))
            FREES(ct + i + 1, sx36(FREE(ct + i + 1) | 0400000000000L));
    }
    do {
        flag = 0;
        while ((i = entrc1())) {
            if ((FREE(ct + i + 1) >> 18) > 0100002) {
                j = tscr(i);
                while (j) {
                    if (FREE(ct + j + 1) < 0) {
                        FREES(ct + i + 1,
                            sx36(FREE(ct + i + 1) | 0400000000000L));
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
            FREES(ct + i + 1, sx36(FREE(ct + i + 1) & ~0400000000000L));
            bits(sbitm, tnoden(i), s);
        }
    }
}

void
tprint(void)
{
    tprin1();
}
