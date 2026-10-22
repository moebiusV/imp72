#!/usr/bin/env python3
"""runoff2mdoc.py IMP.RNO > imp72.7: the DECUS RUNOFF source of the IMP72 manual as an mdoc(7) page (UTF-8).

The source is typed in capitals and RUNOFF lowers them: `^` raises the next letter, `^^` ... `\\\\` raises
everything between, `^&` ... `\\&` underlines, `_` quotes the next character (`__` is `_`, `_#` is `#`).
Whole words with every letter raised (`^G^O`, `^N^O^T`) are IMP words or names; a lone raised letter (`^A`) is a
variable of the description; words that mix letters and operators (`^A=>^B`) are code; the quotes the author
typed around code become literal quotes.  That is the semantic markup: Ic for words of the language, Ar for
the variables of a description, Li for code, Pa for file names, Dq/Sq for quoted prose.

Layout commands: .SUBTITLE (a heading), .PARAGRAPH, .SKIP and .BREAK (a new paragraph), .INDENT -N and
.LEFT MARGIN (lists and indented blocks), .NOFILL/.FILL (a literal block), .CENTER (the heading that follows
.SUBTITLE is dropped).  .INDEX, .PAGE, .TEST, .RIGHT MARGIN, .TAB STOPS, .SPACING, .NUMBER, .JUSTIFY, .TITLE
and .PRINT only affect the paper and are dropped.  tools/runoff-fixes.txt corrects the typing mistakes of the
source: "old<TAB>new" lines applied to the text after the case flags are resolved."""
import os
import re
import sys

# words of the language whose capitals are the author's but that are names of things, not code
NAMES = {"IMP", "IMP72", "PDP", "ASCII", "FORTRAN", "TENEX", "APL", "ALGOL", "CDC", "DEC", "BNF", "DDT",
         "UUO", "IM", "II", "III", "IV", "VI", "TTY", "REL", "CR", "LIB", "CMD", "DSK", "RSYN", "IMPLIB",
         "PMOD", "AMOD", "RMOD", "OMOD", "MAC", "RNO", "HLP", "IML", "FORTIO", "MAXWELL", "MAXEND", "DEBUG",
         "IMPSEM", "COTREE", "DOSEM", "ERCOR", "ENTREE", "PARSE", "GRAPH", "LEX", "FREE", "STACK", "DIR",
         "SAMPLE", "EXPRESSION", "MEANING", "CONTENTS"}
# words in capitals that are prose (error messages, headings) and not words of the language
PROSE = set()
FILE_RE = re.compile(r"^[A-Z0-9]+\.(IMP|REL|LIB|TMP|CMD|MAC|RNO|SHR|LOW|HLP|LST|CRF|DAT|TXT|BAK|EXE)$", re.I)
MACROS = set("""Ad An Ao Ac Ap Aq Ar At Bc Bd Bf Bk Bl Bo Bq Brc Bro Brq Bsx Bt Bx Cd Cm D1 Db Dc Dd Dl Do Dq
Dt Dv Dx Ec Ed Ef Ek El Em En Eo Er Es Ev Ex Fa Fc Fd Fl Fn Fo Fr Ft Fx Hf Ic In It Lb Li Lk Lp Ms Mt Nd Nm
No Ns Nx Oc Oo Op Os Ot Ox Pa Pc Pf Po Pp Pq Qc Ql Qo Qq Re Rs Rv Sc Sh Sm So Sq Ss St Sx Sy Ta Tn Ud Ux
Va Vt Xc Xo Xr""".split())
OPEN = "(["
CLOSE = ".,:;)]?!"


def load_fixes():
    path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "runoff-fixes.txt")
    fixes = []
    if os.path.exists(path):
        for line in open(path, encoding="utf-8"):
            line = line.rstrip("\n")
            if not line or line.startswith("#") or "\t" not in line:
                continue
            old, new = line.split("\t", 1)
            fixes.append((old, new))
    return fixes


class Caser:
    """RUNOFF's case flags; the state of ^^ carries across lines."""

    def __init__(self):
        self.caps = False
        self.under = False

    def run(self, text, mark=False):
        """The text with the flags resolved.  With mark, a letter that was raised by its own ^ is returned as
        a (char, True) pair, so that the words made only of raised letters can be told from the others."""
        out = []
        i = 0
        n = len(text)
        while i < n:
            c = text[i]
            if c == "^" and i + 1 < n:
                d = text[i + 1]
                if d == "^":
                    self.caps = True
                    i += 2
                    continue
                if d == "&":
                    self.under = True
                    i += 2
                    continue
                out.append((d.upper(), True) if d.isalpha() else (d, False))
                i += 2
                continue
            if c == "\\":
                nx = text[i + 1:i + 2]
                if nx == "\\":
                    self.caps = False
                    i += 2
                    continue
                if nx == "&":
                    self.under = False
                    i += 2
                    continue
                i += 1
                if i < n:
                    out.append((text[i].lower(), False))
                    i += 1
                continue
            if c == "_" and i + 1 < n:
                out.append((text[i + 1], False))
                i += 2
                continue
            if c.isalpha():
                out.append((c.upper(), self.caps) if self.caps else (c.lower(), False))
            else:
                out.append((c, False))
            i += 1
        if mark:
            return out
        return "".join(ch for ch, _ in out)


def esc(s, code=False):
    """An argument of a macro line."""
    s = s.replace("\\", "\\e")
    if code:
        s = s.replace("-", "\\-").replace("'", "\\(aq").replace("`", "\\(ga").replace("~", "\\(ti")
        s = s.replace("^", "\\(ha")
    s = s.replace('"', "\\(dq")
    if len(s) > 1 and s[-1] in ".,:;)]?!" and not all(ch in ".,:;()[]?!|" for ch in s):
        s = s[:-1] + "\\&" + s[-1]
    if s in MACROS or (s and s[0] in "'.") or (s and all(ch in "|.,:;()[]?!" for ch in s)):
        s = "\\&" + s
    if " " in s or s == "":
        s = '"' + s + '"'
    return s


def esctext(s):
    """A word of a text line."""
    s = s.replace("\\", "\\e")
    if s[:1] in ".'":
        s = "\\&" + s
    return s


def utf8ize(s):
    s = s.replace(" - ", " — ")
    s = re.sub(r"(?<=\w)'(?=\w)", "’", s)
    return s


class Tok:
    """A word of a paragraph: plain, or a macro (Ic, Ar, Li, Pa) with its text."""

    def __init__(self, kind, text, pre="", post="", suffix=""):
        self.suffix = suffix      # text glued after a macro's argument ('th)
        self.kind = kind          # "w", "Ic", "Ar", "Li", "Pa"
        self.text = text
        self.pre = pre            # opening delimiters glued before
        self.post = post          # closing delimiters glued after


VARNEXT = {"IS", "ARE", "AND", "OR", "NE", "LE", "GE", "LT", "GT", "EQ", "LS", "RS", "LROT", "RROT", "ALS",
           "ARS", "XOR", "EQV", "HAS", "IMPLIES", "LEFT", "RIGHT", "FOR", "WILL", "BEING", "DO", "MAY",
           "MUST", "SHOULD", "CAN", "WAS", "WERE", "WHICH"}
EMPH = {"EXCEPTION", "NOTA", "BENE", "YOUR", "SYNTACTICALLY", "SEMANTICALLY", "CORRECT", "MEANINGFUL"}
PLAIN = {"I/O", "N.B"}


def classify(word, marked):
    """word: text with flags resolved; marked: the (char, raised) pairs of the same word."""
    letters = [(ch, r) for ch, r in marked if ch.isalpha()]
    raised = [r for _, r in letters]
    core = "".join(ch for ch, _ in marked)
    if FILE_RE.match(core):
        return "Pa"
    if core in PLAIN:
        return "w"
    ops = re.search(r"[=<>+*\[\]_#%$|@]", core) is not None
    if any(raised) and re.search(r"[A-Za-z0-9][(:,][A-Za-z0-9(]", core) and not re.search(r"[a-z]{3,}", core):
        ops = True
    if re.fullmatch(r"[A-Z]-[A-Z]", core) and all(raised):
        ops = True
    slash = "/" in core and (any(raised) or re.search(r"[0-9]", core))
    if re.fullmatch(r"[/=<>+*%#@$\"']+", core):
        return "Li"
    if raised and all(raised) and re.fullmatch(r"[A-Z](\.[A-Z])(\[[A-Z,]+\])?|[A-Z]\[[A-Z,]+\]", core):
        return "Li"
    if len(letters) == 1 and all(raised) and len(core) == 1:
        return "Ar"
    if ops or slash:
        return "Li"
    if len(letters) >= 2 and all(raised) and core.isalpha():
        if core.upper() in EMPH:
            return "Sy"
        return "name" if core.upper() in NAMES else "Ic"
    if len(letters) >= 2 and all(raised) and re.fullmatch(r"[A-Z]+[0-9]+", core):
        return "name" if re.sub(r"[0-9]+$", "", core) in NAMES else "Li"
    if re.fullmatch(r"[0-9][0-9A-Z]*", core) and re.search(r"[A-Z]", core) and not re.fullmatch(r"[0-9]+K", core):
        return "Li"
    return "w"


def grab_star(spans):
    spans.append("*")
    return "\x01%d\x02 " % (len(spans) - 1)


def tokenize(text, caser, groups=True):
    """A paragraph's raw text to a list of Tok."""
    # ^^...\\ spans may hold spaces: make each a single word first
    spans = []

    def grab(m):
        spans.append(m.group(1))
        return "\x01%d\x02" % (len(spans) - 1)

    raw = re.sub(r"\^\^(.*?)\\\\", grab, text, flags=re.S)
    raw = re.sub(r"\s+", " ", raw).strip()
    raw = re.sub(r"(?<!\S)\*(?=[A-Za-z^])", lambda m: grab_star(spans), raw)
    toks = []
    for w in raw.split(" "):
        if not w:
            continue
        mq = re.fullmatch(r"(['\"])([^A-Za-z0-9\s])\1([.,:;)\]?!]*)", w)
        if mq:
            toks.append(Group(mq.group(1), [Tok("Li", mq.group(2))], "", mq.group(3)))
            continue
        mb = re.fullmatch(r"([(\[\"']*)(!.+?!)([.,:;)\]?\"']*)", w)
        if mb:
            pre, core, post = mb.groups()
            toks.append(Tok("Li", Caser().run(core), pre, post))
            continue
        m = re.fullmatch(r"([(\[\"']*)(.*?)([.,:;)\]?!\"']*)", w)
        pre, core, post = m.groups()
        while post[:1] in (")", "]") and core.count("(" if post[0] == ")" else "[") > core.count(post[0]):
            core += post[0]
            post = post[1:]
        if core == "" and pre and not post:
            core, pre = pre, ""
        if "\x01" in core:
            inner = []
            for part in re.split(r"(\x01\d+\x02)", core):
                mm = re.fullmatch(r"\x01(\d+)\x02", part)
                inner.append(spans[int(mm.group(1))] if mm else Caser().run(part))
            t = "".join(inner)
            mt = re.fullmatch(r"([A-Z][A-Z0-9 ]*[A-Z0-9])([,;.]+)\s*", t)
            if mt:
                t = mt.group(1)
                post = mt.group(2) + post
            t = t.strip()
            if re.search(r"[a-z]", t) and re.search(r"[A-Z]", t) and not re.search(r"[=<>+*/\[\](),:]", t):
                toks.append(Tok("w", t, pre, post))
                continue
            if re.fullmatch(r"[A-Z]+(, [A-Z]+)+", t):
                words = t.split(", ")
                for q, wd in enumerate(words):
                    toks.append(Tok("Ic", wd, pre if q == 0 else "", post if q == len(words) - 1 else ","))
                continue
            if FILE_RE.match(t):
                kind = "Pa"
            elif re.search(r"[=<>+*/\[\](),:]", t):
                kind = "Li"
            elif not re.search(r"[A-Za-z]", t):
                kind = "Li"
            elif t.split()[0].upper() in NAMES:
                kind = "w"
            elif " " in t:
                kind = "Li"
            else:
                kind = "Ic"
            toks.append(Tok(kind, t, pre, post))
            continue
        marked = caser.run(core, mark=True)
        if re.fullmatch(r"\^[A-Z]'(TH|th)", core):
            toks.append(Tok("Ar", core[1], pre, post, "\u2019th"))
            continue
        kind = classify(core, marked) if core else "w"
        t = "".join(ch for ch, _ in marked)
        toks.append(Tok("w" if kind == "name" else kind, t, pre, post))
    # a lone A or I is the article or the pronoun unless the word after it says it is a variable
    for k, t in enumerate(toks):
        if t.kind == "Ar" and t.text in ("A", "I"):
            nxt = toks[k + 1] if k + 1 < len(toks) else None
            nt = nxt.text.upper() if nxt else ""
            if t.post or nxt is None or not nt[:1].isalnum() or nt in VARNEXT or nxt.kind in ("Li",):
                continue
            t.kind = "w"
    if groups:
        k = 0
        while k < len(toks):
            a = toks[k]
            quote_pre = "".join(c for c in a.pre if c in "\"'")
            paren_pre = "".join(c for c in a.pre if c not in "\"'")
            full = paren_pre + a.text + a.post
            if a.kind not in ("w", "group") and full.count("(") > full.count(")"):
                j = k + 1
                depth = full.count("(") - full.count(")")
                text = full
                while j < len(toks) and depth > 0 and j < k + 12:
                    f = toks[j].pre + toks[j].text + toks[j].post
                    depth += f.count("(") - f.count(")")
                    text += " " + f
                    j += 1
                if depth == 0:
                    m = re.fullmatch(r"(.*?)([.,;:]*)", text)
                    a.pre, a.text, a.post, a.kind = quote_pre, m.group(1), m.group(2), "Li"
                    del toks[k + 1:j]
            elif a.kind == "Li" and k + 2 < len(toks) and toks[k + 1].kind == "Ic" and toks[k + 1].text == "ELSE" \
                    and toks[k + 2].kind in ("Ar", "Li") and not a.post and not toks[k + 1].post:
                b, c = toks[k + 1], toks[k + 2]
                a.text = a.pre + a.text + " ELSE " + c.pre + c.text
                a.pre = ""
                a.post = c.post
                del toks[k + 1:k + 3]
                continue
            k += 1
    k = 0
    while k + 1 < len(toks):
        a, b = toks[k], toks[k + 1]
        if a.kind == "Li" and b.kind == "Li" and a.post in (";", ",") and not b.pre:
            a.text = a.text + a.post + " " + b.text
            a.post = b.post
            del toks[k + 1]
            continue
        k += 1
    prev = None
    for t in toks:
        if prev is not None and t.kind == "w" and t.text[:1].islower() and not t.pre and not re.match(r"[a-z]\.[a-z]\.", t.text):
            pt = prev.text + prev.post
            if prev.kind == "w" and re.search(r"[a-z]{3,}[.?!]$", pt) and not re.search(r"(^|\W)(etc|sect|sec|fig|vs|approx)\.$", pt, re.I):
                t.text = t.text[0].upper() + t.text[1:]
            elif prev.kind != "w" and prev.post[-1:] in (".", "?", "!"):
                t.text = t.text[0].upper() + t.text[1:]
        prev = t
    return group_quotes(toks) if groups else toks


class Group:
    def __init__(self, quote, toks, pre, post):
        self.kind = "group"
        self.text = ""
        self.suffix = ""
        self.quote = quote
        self.toks = toks
        self.pre = pre
        self.post = post


def group_quotes(toks):
    """Tokens between matching quote marks become a Group."""
    out = []
    k = 0
    n = len(toks)
    while k < n:
        t = toks[k]
        q = next((c for c in t.pre if c in "\"'"), None)
        if q:
            end = None
            for j in range(k, min(n, k + 14)):
                tj = toks[j]
                if q in tj.post and (j > k or t.post.count(q) > 0):
                    end = j
                    break
            if end is not None:
                lead = t.pre[:t.pre.index(q)]
                t.pre = t.pre[t.pre.index(q) + 1:]
                te = toks[end]
                cut = te.post.index(q)
                trail = te.post[cut + 1:]
                te.post = te.post[:cut]
                inner = toks[k:end + 1]
                out.append(Group(q, group_quotes(inner) if end > k else inner, lead, trail))
                k = end + 1
                continue
        out.append(t)
        k += 1
    return out


def textword(t):
    return esctext(utf8ize(t.pre + t.text + t.post))


def emit_par(toks, out):
    """Tokens to mdoc lines."""
    line = []

    def flush():
        nonlocal line
        if line:
            out.append(" ".join(line))
        line = []

    i = 0
    n = len(toks)
    while i < n:
        t = toks[i]
        if t.kind == "w":
            line.append(textword(t))
            nxt = toks[i + 1] if i + 1 < n else None
            ends = re.search(r"[a-z0-9)][.?!]$", t.text + t.post) is not None and not re.search(r"(^|\W)(etc|sect|sec|fig|vs|approx|e\.g|i\.e)\.$", t.text + t.post, re.I)
            if len(" ".join(line)) > 66 or (ends and (nxt is None or nxt.kind != "w" or nxt.text[:1].isupper())):
                flush()
            i += 1
            continue
        if t.kind == "group":
            flush()
            inner = t.toks
            codey = any(getattr(x, "kind", "w") in ("Li", "Ar", "Ic", "Pa") for x in inner)
            if codey or len(inner) == 1 and t.quote == "'" or (len(inner) == 1 and not re.search(r"[A-Za-z0-9]", getattr(inner[0], "text", "x"))):
                if t.quote == '"':
                    text = " ".join(x.pre + x.text + x.post for x in inner if x.kind != "group")
                    pre = (".Pf " + esc(t.pre) + " Dq Li ") if t.pre else ".Dq Li "
                    out.append(pre + esc(text, True) + (" " + " ".join(t.post) if t.post else ""))
                    i += 1
                    continue
                text = " ".join(x.pre + x.text + x.post if x.kind != "group" else "" for x in inner)
                lead = [t.pre] if t.pre else []
                head = (".Pf " + esc(t.pre) + " Ql ") if t.pre else ".Ql "
                out.append(head + esc(text, True) + (" " + " ".join(t.post) if t.post else ""))
            else:
                macro = "Dq" if t.quote == '"' else "Sq"
                sub = []
                emit_par(inner, sub)
                simple = all(not l.startswith(".") for l in sub) and len(sub) == 1
                pre = (".Pf " + esc(t.pre) + " ") if t.pre else "."
                if simple:
                    out.append(pre + macro + " " + " ".join(esc(w) for w in sub[0].split(" ")) + (" " + " ".join(t.post) if t.post else ""))
                else:
                    out.append(pre + ("Do" if macro == "Dq" else "So"))
                    out.extend(sub)
                    out.append("." + ("Dc" if macro == "Dq" else "Sc") + (" " + " ".join(t.post) if t.post else ""))
            i += 1
            continue
        macro = t.kind
        args = []
        pre = t.pre
        j = i
        while j < n and toks[j].kind == macro and (j == i or (not toks[j].pre and not toks[j - 1].post)):
            args.append(toks[j].text)
            j += 1
        post = toks[j - 1].post
        flush()
        code = macro in ("Li", "Ic", "Pa")
        if macro == "Sy":
            args = [" ".join(args)]
        parts = [(".Pf " + esc(pre) + " " + macro) if pre else "." + macro]
        if macro == "Li" and len(args) > 1:
            args = [" ".join(args)]
        parts.extend(esc(a if code else utf8ize(a), code) for a in args)
        if toks[j - 1].suffix:
            parts.extend(["Ns", esc(toks[j - 1].suffix)])
        if post:
            parts.extend(post)
        out.append(" ".join(parts))
        i = j
    flush()


HEADER = """.Dd August 1, 1973
.Dt IMP72 7
.Os
.Sh NAME
.Nm imp72
.Nd the IMP72 systems programming language, reference manual
.Sh DESCRIPTION
This is the reference manual of IMP72 version 1.5, written for the PDP-10 by Walt Bilofsky
at the Yale University Department of Computer Science (DECUS program 10-198), August 1973.
The compiler was based largely on previous work of E. T. Irons;
substantial contributions were made by Steven Weingart and Terry Lyons.
The manual reflects version 1.5 of the compiler, written to run under version 5.06 of the
DECsystem-10
operating system and run successfully under version 1.31.23 of
TENEX.
.Pp
The text is the author's own, converted from the RUNOFF source
with the typesetting of 1973 replaced by mdoc markup."""


PAPER = {"INDEX", "PAGE", "TEST", "RIGHT", "TAB", "SPACING", "NUMBER", "JUSTIFY", "TITLE", "PRINT", "FOOTNOTE"}


def events(lines):
    """Source lines to events: ("cmd", NAME, arg) and ("text", [lines])."""
    ev = []
    buf = []
    for raw in lines:
        if raw.startswith("."):
            m = re.match(r"\.([A-Za-z]+)\s*(.*)", raw)
            if m and m.group(1).upper() in PAPER:
                continue
            if buf:
                ev.append(("text", buf))
                buf = []
            if m:
                ev.append(("cmd", m.group(1).upper(), m.group(2).strip()))
            continue
        if raw.strip() == "":
            continue
        buf.append(raw)
    if buf:
        ev.append(("text", buf))
    return ev


def find_function(text):
    """NAME(ARGS): description, with the name raised or in a ^^ span."""
    m = re.match(r"(\^\^[^:]*?):\s+(.*)", text, re.S)
    if not m and text.startswith("^^") and ":" not in text:
        m = re.match(r"(\^\^.*)()", text, re.S)
    if not m or len(m.group(1)) > 80:
        return None
    head = re.sub(r"\s+", " ", Caser().run(m.group(1))).strip()
    if re.fullmatch(r"[A-Z][A-Z0-9]*\(.*\)", head, re.I):
        return head, m.group(2).strip()
    return None


def fn_split(args):
    """<a,b> becomes <a and b>, so that Fn prints it as <a, b>."""
    res = []
    for a in args:
        if a.startswith("<") and a.endswith(">") and "," in a:
            parts = a[1:-1].split(",")
            res.extend(["<" + parts[0]] + parts[1:-1] + [parts[-1] + ">"])
        else:
            res.append(a)
    return res


def fn_parts(head):
    name, rest = head.split("(", 1)
    rest = rest[:-1]
    args, depth, cur = [], 0, ""
    for ch in rest:
        if ch == "<":
            depth += 1
        elif ch == ">":
            depth -= 1
        if ch == "," and depth == 0:
            args.append(cur)
            cur = ""
        else:
            cur += ch
    if cur.strip():
        args.append(cur)
    return name, fn_split([a.strip() for a in args])


def find_message(text):
    """An error message and its explanation: mostly raised letters up to the first colon."""
    m = re.search(r":(\s|$)", text)
    if not m or m.start() > 140 or m.start() < 4:
        return None
    head = text[:m.start()]
    flagged = len(re.findall(r"\^[A-Z]", head))
    letters = len(re.findall(r"[A-Za-z]", head))
    if letters == 0 or flagged < 0.6 * letters or flagged < 4:
        return None
    return head, text[m.end():].strip()


def tag_line(toks):
    """The arguments of an .It line for the tokens of a tag."""
    if len(toks) == 1 and re.fullmatch(r"\d+\.", toks[0].text + toks[0].post):
        return toks[0].text + toks[0].post
    if all(t.kind == "w" for t in toks):
        return "Li " + esc(" ".join(t.pre + t.text + t.post for t in toks), True)
    parts = []
    plain = []

    def flush_plain():
        nonlocal plain
        if plain:
            parts.append(("No " if parts else "") + " ".join(esc(w) for w in plain))
            plain = []

    i = 0
    while i < len(toks):
        t = toks[i]
        if t.kind == "w":
            plain.append(utf8ize(t.pre + t.text + t.post))
            i += 1
            continue
        flush_plain()
        if t.pre not in ("", "(") or (t.post and t.post[0] in "]"):
            parts.append("Li " + esc(t.pre + t.text + t.post, True))
            i += 1
            continue
        macro = t.kind
        args = [t.text]
        j = i + 1
        while j < len(toks) and toks[j].kind == macro and not toks[j].pre and not toks[j - 1].post:
            args.append(toks[j].text)
            j += 1
        if macro == "Li" and len(args) > 1:
            args = [" ".join(args)]
        code = macro in ("Li", "Ic", "Pa")
        parts.append((("Pf " + esc(t.pre) + " ") if t.pre else "") + macro + " " + " ".join(esc(a if code else utf8ize(a), code) for a in args) + (" " + " ".join(toks[j - 1].post) if toks[j - 1].post else ""))
        i = j
    flush_plain()
    return " ".join(parts)


def tidy(out):
    """Drop paragraph breaks that mandoc finds redundant."""
    res = []
    for k, l in enumerate(out):
        if l == ".Pp" and k + 1 < len(out) and (out[k + 1].startswith((".Bd", ".Bl", ".Pp", ".Sh", ".Ss", ".It", ".El", ".Ed"))):
            continue
        if l == ".Pp" and res and res[-1] in (".Ed", ".El") and False:
            continue
        res.append(l)
    return res


def main(path):
    fixes = load_fixes()
    src = open(path, encoding="latin-1").read().split("\n")
    for old, new in fixes:
        src = [l.replace(old, new) if not l.startswith(".") else l for l in src]
    start = next(k for k, l in enumerate(src) if l.startswith(".SUBTITLE 1:"))
    ev = events(src[start:])
    out = ['.\\" Generated by tools/runoff2mdoc.py from decus/imp.rno: do not edit.']
    out.extend(HEADER.split("\n"))
    caser = Caser()
    st = {"list": None, "item": False, "sub": False, "rag": False, "firstpar": True, "lit": None}

    def fixp(text):
        for old, new in fixes:
            text = text.replace(old, new)
        return text

    def par(lines):
        toks = tokenize(fixp(" ".join(lines)), caser)
        caser.caps = False
        emit_par(toks, out)

    def close_sub():
        if st["sub"]:
            out.append(".El")
            st["sub"] = False

    def close_all():
        close_sub()
        if st["rag"]:
            out.append(".Ed")
            st["rag"] = False
        if st["list"]:
            out.append(".El")
            st["list"] = None
            st["firstpar"] = False
        st["item"] = False

    def body_par(lines):
        """A paragraph of the body of the current item or the block."""
        if st.get("pp") and not st["firstpar"]:
            out.append(".Pp")
        st["pp"] = False
        par(lines)
        st["firstpar"] = False

    def finish_lit():
        while st["lit"] and not st["lit"][0].strip():
            st["lit"].pop(0)
        while st["lit"] and not st["lit"][-1].strip():
            st["lit"].pop()
        if st["lit"] is not None and st["lit"] and all(l == l.lstrip() for l in st["lit"] if l.strip()) \
                and sum(len(l.split()) for l in st["lit"]) / max(1, len(st["lit"])) >= 6:
            if st.get("pp") and not st["firstpar"]:
                out.append(".Pp")
            par([l for l in st["lit"] if l.strip()])
            st["pp"] = True
            st["firstpar"] = False
            st["lit"] = None
            return
        if st["lit"] is not None:
            if st["item"] and not st["firstpar"]:
                out.append(".Pp")
            lit = [l.expandtabs(8) if False else l for l in st["lit"]]
            ind = min((len(l) - len(l.lstrip(" ")) for l in lit if l.strip()), default=0)
            out.append(".Bd -literal -offset indent")
            out.extend(esc_lit(l[ind:], fixes) for l in lit)
            out.append(".Ed")
            st["firstpar"] = False
            st["pp"] = True
        st["lit"] = None
        return

    i = 0
    n = len(ev)
    while i < n:
        e = ev[i]
        i += 1
        if e[0] == "text":
            if st["lit"] is not None:
                st["lit"].extend(caser.run(l) for l in e[1] if not l.lstrip().startswith("^&"))
                continue
            lines = [l for l in e[1] if not l.lstrip().startswith("^&") and not re.fullmatch(r"-{5,}|!", l.strip())]
            if not lines:
                continue
            # a paragraph in a margin region: numbered items, or an indented block
            if st.get("margin") and st["list"] is None and not st["rag"] and not st["sub"]:
                if re.match(r"\d+\.\s", lines[0]):
                    out.append(".Bl -tag -width 3n -compact")
                    st["list"] = "num"
                else:
                    out.append(".Bd -ragged -offset indent")
                    st["rag"] = True
                    st["firstpar"] = True
            if st["list"] == "num":
                words = " ".join(lines)
                mm = re.match(r"(\d+\.)\s+(.*)", words, re.S)
                if mm:
                    out.append(".It " + mm.group(1))
                    par([mm.group(2)])
                    continue
            if len(lines) == 1 and "::=" in lines[0] and i < n and ev[i][0] == "cmd" and ev[i][1] == "INDENT" \
                    and int(ev[i][2] or 0) >= 0:
                ev.insert(i - 1, ("cmd", "CENTER", ""))
                n += 1
                i -= 1
                continue
            if st["list"] is None or True:
                prevcmd = ev[i - 2][1] if i >= 2 and ev[i - 2][0] == "cmd" else None
                nextcmd = ev[i][1] if i < n and ev[i][0] == "cmd" else None
                if len(lines) == 1 and prevcmd in ("SKIP", "BREAK") and nextcmd in ("SKIP", "BREAK", "LEFT", None):
                    toks = tokenize(lines[0], caser, groups=False)
                    if toks and all(t.kind != "w" for t in toks) and (len(toks) > 1 or toks[0].kind == "Li"):
                        text = " ".join(t.pre + t.text + t.post for t in toks)
                        if st.get("pp") and not st["firstpar"]:
                            out.append(".Pp")
                        out.append(".Dl " + esc(text, True))
                        st["pp"] = True
                        st["firstpar"] = False
                        continue
            body_par(lines)
            continue
        _, cmd, arg = e
        if st["lit"] is not None and cmd not in ("FILL", "SUBTITLE"):
            continue
        if (cmd == "CENTER" or (cmd == "INDENT" and int(arg or 0) >= 0)) and i < n and ev[i][0] == "text":
            run = []
            j = i - 1
            while j < n and ev[j][0] == "cmd" and (ev[j][1] == "CENTER" or (ev[j][1] == "INDENT" and int(ev[j][2] or 0) >= 0)) \
                    and j + 1 < n and ev[j + 1][0] == "text":
                chunk = ev[j + 1][1]
                joined = " ".join(chunk)
                prose = joined.rstrip().endswith(".") and len(joined.split()) >= 6
                if prose:
                    break
                k = int(ev[j][2] or 0) if ev[j][1] == "INDENT" else 0
                for l in chunk:
                    run.append((k, l))
                j += 2
            textual = not run
            if not textual:
                base = min(k for k, _ in run)
                lines = [" " * (k - base) + Caser().run(l) for k, l in run]
                if st.get("pp") and not st["firstpar"]:
                    out.append(".Pp")
                out.append(".Bd -literal -offset indent")
                out.extend(esc_lit(l, fixes) for l in lines)
                out.append(".Ed")
                st["pp"] = True
                st["firstpar"] = False
                i = j
                continue
        if cmd == "SUBTITLE":
            finish_lit()
            close_all()
            st["margin"] = 0
            title = re.sub(r"\s+", " ", Caser().run(arg)).strip().rstrip(".")
            num = re.match(r"([0-9.]+):\s*(.*)", title)
            if num:
                depth = num.group(1).count(".") + 1
                head = num.group(1).rstrip(".") + " " + num.group(2)
            else:
                depth = 1
                head = title.replace(":", "")
            head = re.sub(r"\s+", " ", head).strip()
            if head.upper() == "INDEX":
                continue
            head = re.sub(r'"([^"]*)"', "\u201c\\1\u201d", head)
            out.append((".Sh " if depth == 1 else ".Ss ") + esc(utf8ize(head).upper() if depth == 1 else utf8ize(head)))
            st["firstpar"] = True
            st["pp"] = False
            # the heading's own copy: .CENTER and one text event, possibly after other layout commands
            k = i
            while k < n and ev[k][0] == "cmd" and ev[k][1] in ("PAGE", "SKIP", "INDEX", "TEST", "RIGHT", "TAB", "LEFT"):
                k += 1
            if k < n and ev[k][0] == "cmd" and ev[k][1] == "CENTER" and k + 1 < n and ev[k + 1][0] == "text":
                rest = ev[k + 1][1][1:]
                i = k + 2
                if rest:
                    ev.insert(i, ("text", rest))
                    n += 1
            continue
        if cmd == "NOFILL":
            st["lit"] = []
            continue
        if cmd == "FILL":
            finish_lit()
            continue
        if cmd == "LEFT":
            m = re.match(r"MARGIN\s+(\d+)", arg, re.I)
            close_sub()
            if m and int(m.group(1)) == 0:
                close_all()
                st["margin"] = 0
            elif m:
                st["margin"] = int(m.group(1))
                st["firstpar"] = True
            continue
        if cmd in ("PARAGRAPH", "SKIP", "BREAK"):
            if cmd != "BREAK":
                close_sub()
                st["pp"] = True
            continue
        if cmd == "INDENT":
            k = int(arg or 0)
            if k >= 0:
                # an indented paragraph inside a body: a bullet
                if i < n and ev[i][0] == "text":
                    if not st["sub"]:
                        out.append(".Bl -bullet -compact -offset indent")
                        st["sub"] = True
                    out.append(".It")
                    par(ev[i][1])
                    i += 1
                continue
            width = -k
            close_sub()
            tags = []
            body = None
            if i < n and ev[i][0] == "text":
                tags.append(ev[i][1])
                i += 1
            while i < n and ev[i][0] == "cmd" and ev[i][1] == "INDENT" and int(ev[i][2] or 0) == k and i + 1 < n and ev[i + 1][0] == "text":
                tags.append(ev[i + 1][1])
                i += 2
            brk = i < n and ev[i][0] == "cmd" and ev[i][1] == "BREAK"
            if tags and not brk and tags[0][0].startswith("(*)"):
                st["pp"] = True
                body_par(tags[0])
                continue
            if brk:
                i += 1
            else:
                # no .BREAK: the item's tag is its first word (a number) or the ^^span before a colon
                first = tags[0] if tags else []
                text = " ".join(first)
                mref = re.match(r"([^:]*?\s\d{4}):\s+(.*)", text, re.S)
                msg = find_message(text)
                fn = find_function(text)
                mm = re.match(r"(\d+\.)\s+(.*)", text, re.S)
                m2 = re.match(r"(\^\^.*?\\\\|<[^<>\s]+>):\s*(.*)", text, re.S)
                if mm:
                    tags, body = [[mm.group(1)]], mm.group(2)
                elif mref and len(mref.group(1)) < 40:
                    tags, body = [["\x02" + re.sub(r"\s+", " ", Caser().run(mref.group(1)))]], mref.group(2)
                elif fn:
                    tags, body = [["\x01" + fn[0]]], fn[1]
                elif msg:
                    tags, body = [["\x00" + msg[0]]], msg[1]
                elif m2:
                    tags, body = [[m2.group(1)]], m2.group(2)
                else:
                    tags, body = [], text
            if not tags and body is not None or (not tags and body is None):
                # an item without a tag is a paragraph (of the item before it, if any)
                if body:
                    body_par([body])
                    st["pp"] = False
                    st["firstpar"] = False
                    st["pp"] = True
                continue
            if st["list"] is None:
                out.append(".Bl -tag -width %dn -compact" % max(width, 3))
                st["list"] = "tag"
            if st["rag"]:
                pass
            if tags:
                for tg in tags:
                    if tg and tg[0].startswith("\x02"):
                        out.append(".It " + esc(utf8ize(tg[0][1:])))
                        continue
                    if tg and tg[0].startswith("\x01"):
                        name, args = fn_parts(tg[0][1:])
                        out.append(".It Fn " + esc(name, True) + "".join(" " + esc(a, True) for a in args))
                        continue
                    if tg and tg[0].startswith("\x00"):
                        out.append(".It Li " + esc(re.sub(r"\s+", " ", Caser().run(tg[0][1:])).strip(), True))
                        continue
                    toks = tokenize(" ".join(tg), caser, groups=False)
                    out.append(".It " + tag_line(toks))
            elif body and len(body) < 40 and body.rstrip().endswith(":"):
                out.append(".It Sy " + esc(utf8ize(Caser().run(body).strip())))
                body = None
            else:
                out.append(".Pp") if st["item"] else out.append(".It")
            st["item"] = True
            st["firstpar"] = True
            st["pp"] = False
            if body:
                par([body])
                st["firstpar"] = False
            continue
        # everything else only affects the paper
    close_all()
    sys.stdout.write("\n".join(tidy(out)) + "\n")


def esc_lit(line, fixes):
    line = line.rstrip()
    line = line.replace("\\", "\\e")
    if line[:1] in ".'":
        line = "\\&" + line
    return line


if __name__ == "__main__":
    main(sys.argv[1])
