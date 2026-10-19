#!/usr/bin/env python3
"""imp-trace.py IN.imp OUT.imp: a program of imp-fuzz2.py with a PRINT of the subroutine's
variables after every statement of every SUBR body, so that two implementations can be
compared statement by statement (the first line where their output differs names the
statement)."""
import re, sys


def split_top(body):
    parts, depth, cur = [], 0, ""
    for ch in body:
        if ch == "(" or ch == "[":
            depth += 1
        elif ch == ")" or ch == "]":
            depth -= 1
        if ch == ";" and depth == 0:
            parts.append(cur)
            cur = ""
        else:
            cur += ch
    parts.append(cur)
    return parts


def main(src, dst):
    out = []
    for line in open(src).read().split("\n"):
        m = re.match(r"^SUBR (\w+)\(([^)]*)\) IS \((.*)\);?$", line)
        if m:
            name, params, body = m.groups()
            vs = [p for p in params.split(",")] + ["L%s%d" % (name[1:], j) for j in (0, 1)]
            tr = "PRINT STG 0,'%s',%s,/" % (name, ",".join("IGR 6,%s" % v for v in vs + list("ABCD")))
            parts = [p.strip() for p in split_top(body)]
            new = []
            for k, p in enumerate(parts):
                new.append(p)
                if k < len(parts) - 1:
                    new.append(tr)
            line = "SUBR %s(%s) IS (%s)%s" % (name, params, "; ".join(new), ";" if line.rstrip().endswith(";") else "")
        out.append(line)
    open(dst, "w").write("\n".join(out))


if __name__ == "__main__":
    main(*sys.argv[1:3])
