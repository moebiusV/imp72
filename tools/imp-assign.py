#!/usr/bin/env python3
"""imp-assign.py FILE... | imp-assign.py --tape FILE...: IMP72 writes an assignment as := (the tape wrote _, which is now a letter of names, so that a name is a valid C name).

    imp-assign.py FILE...    rewrites each file in place: every _ that is code becomes :=
    imp-assign.py --tape FILE...    prints the tape's text of each (:= back to _), for the original compiler, which only has _; changes nothing

A file is read statement by statement.  A statement of syntax, <CLASS> ::= syntax part ::= semantic part, has its syntax part and its semantic strings ("...") treated apart: the semantic
strings are code, which the compiler parses with the grammar, so the _ in them is :=; in the syntax part a token that is not a name is quoted ('<'), so the element is written ':=' (the lexer reads
:= as one token, and RSYN, which takes the first colon it meets for the start of ::=, does not meet one).  Anything else is code (an IMP program, or the declarations of a file of syntax).  Left alone everywhere: comments (#...#), text constants ('...', a
quote doubled inside), !special identifiers! outside the strings, and the quoted characters '"' and '_' of a syntax part."""
import re
import sys

CLASS = re.compile(r"<[A-Z0-9]+(?:,[A-Z0-9]+)?>\s*::=")


def copy_until(text, i, close):
    """the end (exclusive) of the construct that opens at i and closes with the character close, a doubled close being inside it"""
    n = len(text)
    j = i + 1
    while j < n:
        if text[j] == close:
            if close == "'" and j + 1 < n and text[j + 1] == "'":
                j += 2
                continue
            return j + 1
        j += 1
    return n


def convert(text):
    out = []
    i, n = 0, len(text)
    state = "start"                  # start (before the first token of a statement), code, syn1 (the class), syn2 (the syntax part), syn3 (the semantic part)
    dq = False                       # inside a semantic string
    while i < n:
        c = text[i]
        if not dq and c == "#":
            j = copy_until(text, i, "#")
            out.append(text[i:j]); i = j; continue
        if state == "start" and not c.isspace():
            state = "syn1" if CLASS.match(text, i) else "code"
        if c == "'":
            if text[i:i + 3] in ("'\"'", "'_'"):
                out.append(text[i:i + 3]); i += 3; continue
            j = copy_until(text, i, "'")
            out.append(text[i:j]); i = j; continue
        if not dq and c == "!" and state in ("code", "syn3"):
            j = copy_until(text, i, "!")
            out.append(text[i:j]); i = j; continue
        if state in ("syn1", "syn2") and text[i:i + 3] == "::=":
            state = "syn2" if state == "syn1" else "syn3"
            out.append("::="); i += 3; continue
        if state == "syn3" and c == '"':
            dq = not dq
            out.append(c); i += 1; continue
        if c == ";" and not dq:
            state = "start"
            out.append(c); i += 1; continue
        if c == "_":
            out.append("':='" if state == "syn2" else ":=")
            i += 1; continue
        out.append(c); i += 1
    return "".join(out)


def to_tape(text):
    """every := that is code becomes _ (the inverse, for programs; a file of syntax is not sent to the original this way)"""
    out = []
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        if c == "#":
            j = copy_until(text, i, "#"); out.append(text[i:j]); i = j; continue
        if c == "'":
            j = copy_until(text, i, "'"); out.append(text[i:j]); i = j; continue
        if c == "!":
            j = copy_until(text, i, "!"); out.append(text[i:j]); i = j; continue
        if text[i:i + 2] == ":=":
            out.append("_"); i += 2; continue
        out.append(c); i += 1
    return "".join(out)


if __name__ == "__main__":
    args = sys.argv[1:]
    if args and args[0] == "--tape":
        for p in args[1:]:
            sys.stdout.write(to_tape(open(p, errors="surrogateescape", newline="").read()))
        sys.exit(0)
    for p in args:
        t = open(p, errors="surrogateescape", newline="").read()
        r = convert(t)
        if r != t:
            open(p, "w", errors="surrogateescape", newline="").write(r)
