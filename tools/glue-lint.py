#!/usr/bin/env python3
"""glue-lint.py FILE.imp...: a ported module must reach the routines of the other modules through the glue
(stage0/impglue.c, names IMP...), which takes arguments by reference.  Lists the calls of a module that are
neither its own SUBRs, nor macros of src/impsyn64.imc, nor glue.  Exit 1 if there are any."""
import re
import sys

MACROS = set("""FREE FREES FLEN FADD FADDEX FRELOT FALLOT FINCSET DPROP DPROPS SX36 U36 L18 R18 W18 LS36 DIR ERROR PRINT
STG IGR OCT FLT BIT SCR ALT GTYPE GVAL GNB TSCR TALT TNODEN STSCR STALT STNODEN HL HR HW HLS FORG LOC
BYTEP FOR WHILE UNTIL ELSE IS ARE DO RETURN""".split())
bad = 0
for f in sys.argv[1:]:
    s = open(f).read()
    s = re.sub(r"'[^'\n]*'", "''", s)                  # strings first: they may hold a number sign
    s = re.sub(r"#[^#]*#", "", s)
    calls = set(re.findall(r"(?<![A-Z0-9])([A-Z][A-Z0-9]*)\(", s)) | set(re.findall(r"_([A-Z][A-Z0-9]*)\(", s))
    defs = set(re.findall(r"SUBR ([A-Z0-9]+)", s))
    extra = sorted(c for c in calls - defs - MACROS if not c.startswith("IMP"))
    if extra:
        print("%s: calls that bypass the glue: %s" % (f, " ".join(extra)))
        bad = 1
sys.exit(bad)
