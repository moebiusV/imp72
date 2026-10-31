#!/usr/bin/env python3
"""lst-compare.py [-v] ORACLE.lst OURS.txt: compare the instructions the original compiler assembled (its /A listing) with the code chain imp72s0 -s A prints, and say what differs and why.

The original's listing is after the register allocator and the assembler (RMOD, AMOD, OMOD: registers are numbers, constants are at the end); ours is the chain before them (virtual
registers R66 and the like), made for a back end that allocates the registers itself.  So the two are not the same text, and each difference between them has a reason that is checked,
here, in the code of the original or in the choices of the port.  Both sides are reduced by the rules below (each counted), and what is left is compared as sequences of mnemonics and
labels; anything left over is reported as unexplained and the exit status is 1.

Not instructions, on either side:
  - words of data the listing took for instructions: the lists of PRINT (a number where the mnemonic should be), the byte pointer words (the word after a %BYTEn: tag);
  - the special functions SF0 to SF11 of AMOD (tags and register declarations), CONST and END of ours.

Rules on the original's side (what the port does without):
  spill      an instruction that names a spill temporary, %TMPn: RMOD saves a register in memory around a call and loads it back (rmod.imc: NEWNAME('TMP')); LLVM allocates here.
  bytepair   HRRZI r,A and HRLI r,SP before LDB/DPB/ILDB/IDPB/IBP: the original builds the byte pointer in a register where it is used; ours names a descriptor (src/llvm-ir/code3.imp
             BYTEP) and the operation reads it.
  float      the half-word immediates of the PDP-10 (HRLZI, HRLOI, FADRI, FSBRI, FMPRI, FDVRI, and MOVEI r,0 for a 0.0 that a floating operation uses) are a real constant of 18 bits in the left half; a REAL is an IEEE double here, a whole
             word of the constant pool, loaded by MOVE and used by FADR, FSBR, FMPR, FDVR.  Seen as the same operation.
Rules on ours:
  reload     MOVE r,X where X is in a register already on the straight path to it (a MOVE or MOVEM of X, or SKIPcc/AOS/SOS/xxxB with X and a register, with nothing between that
             stores X, a call, a tag, a jump that is not the second half of a skip): the original's AMOD tracks the contents of the registers and suppresses the load (rmod.imc REGSET,
             RFL).  Its REGSET zaps what a store changes only when the store is not indirect (IND=0): it does not take a store through a pointer to change a variable, which is not
             always so (a parameter by reference can be the variable); ours keeps the load.
  copy       MOVE r,s of one register to another: the original's allocation gives them one register and suppresses the move (rmod.imc: AC=W, OB=5, RFL_1).
-v lists every instruction a rule took away."""
import difflib
import re
import sys

PSEUDO = {"SF0", "SF1", "SF2", "SF3", "SF4", "SF5", "SF6", "SF7", "SF8", "SF9", "SF10", "SF11", "CONST", "END", "000"}
STORES = {"MOVEM", "ADDM", "ADDB", "SUBM", "SUBB", "IMULM", "IMULB", "IDIVM", "ANDM", "ANDB", "IORM", "IORB", "XORM", "XORB", "AOS", "SOS", "SETZM", "SETOM", "MOVNM", "MOVNB", "HRRM", "HRLM",
          "DPB", "EXCH", "MOVSM", "SETCAM", "SETCMM", "BLT", "AOSE", "AOSN", "SOSE", "SOSN", "FADM", "FSBM", "FMPM", "FDVM", "FADB", "FSBB", "FMPB", "FDVB", "IDPB"}
HOLDS = {"MOVE", "MOVEM", "AOS", "SOS", "ADDB", "SUBB", "IMULB", "IORB", "ANDB", "XORB", "MOVNB", "SKIPE", "SKIPN", "SKIPL", "SKIPG", "SKIPLE", "SKIPGE", "SKIPA"}
SKIPS = ("CAM", "CAI", "SKIP", "TRN", "TLN", "TDN", "TSN", "AOS", "SOS", "JUMP", "AOJ", "SOJ")
CALLS = {"JSA", "PUSHJ", "POPJ", "JRA", "CALL", "JSP"}
FLOAT_IMM = {"FADRI": "FADR", "FSBRI": "FSBR", "FMPRI": "FMPR", "FDVRI": "FDVR", "HRLZI": "MOVE", "HRLOI": "MOVE"}
BYTEOPS = {"LDB", "DPB", "ILDB", "IDPB", "IBP"}


def oracle(path):
    out = []
    data = False                                    # the word after a %BYTEn: tag is the byte pointer word itself, whatever it looks like
    for l in open(path):
        m = re.match(r"^\s+[0-7]{6}\s+[0-7]{6} [0-7]{6}\s+(\S+)\s*(.*)$", l)
        if m:
            if data:
                data = False
                continue
            mn = m.group(1)
            if not re.fullmatch(r"[0-7]+", mn):     # a number is a word of data the listing took for an instruction
                out.append((mn, m.group(2).strip()))
            continue
        m = re.match(r"^\s{10,}(\S+:)\s*$", l)
        if m:
            if m.group(1).startswith("%BYTE"):
                data = True
            else:
                out.append((m.group(1), ""))
    return out


def ours(path):
    out = []
    for l in open(path):
        m = re.match(r"^\s*\d+\s+SF0\s+(\S+:)", l)
        if m:
            out.append((m.group(1), ""))
            continue
        m = re.match(r"^\s*\d+\s+([A-Z][A-Z0-9.]*)\b\s*(.*)$", l)
        if m and m.group(1) not in PSEUDO:
            out.append((m.group(1), m.group(2).strip()))
    return out


def reduce_oracle(o, log):
    out = []
    for mn, op in o:
        if re.search(r"%TMP\d+", op):
            log["spill"].append((mn, op))
            continue
        out.append((mn, op))
    res, i = [], 0                                  # HRRZI r,A / HRLI r,SP just before a byte operation
    while i < len(out):
        if out[i][0] == "HRRZI" and i + 2 < len(out) and out[i + 1][0] == "HRLI" and out[i + 2][0] in BYTEOPS:
            log["bytepair"] += [out[i], out[i + 1]]
            i += 2
            continue
        res.append(out[i])
        i += 1
    final = []
    for k, (mn, op) in enumerate(res):
        if mn in FLOAT_IMM:
            log["float"].append((mn, op))
            mn = FLOAT_IMM[mn]
        elif mn == "MOVEI" and re.fullmatch(r"\d+,0", op) and k + 1 < len(res) and res[k + 1][0] in ("FADR", "FSBR", "FMPR", "FDVR", "FADRI", "FSBRI", "FMPRI", "FDVRI"):
            log["float"].append((mn, op))              # 0.0 as an immediate zero; ours loads the zero word of the pool
            mn = "MOVE"
        final.append((mn, op))
    return final


def holds(u, j):
    """is the MOVE at u[j] a load of something that a register has already"""
    mn, op = u[j]
    m = re.match(r"^(R\d+),(.+)$", op)
    if mn != "MOVE" or not m:
        return None
    x = m.group(2)
    if re.fullmatch(r"R\d+", x):
        return "copy"
    k = j - 1
    while k >= 0:
        a, b = u[k]
        if a.endswith(":") or a in CALLS:
            return None
        if a == "JRST" and not (k > 0 and u[k - 1][0].startswith(SKIPS)):
            return None
        mm = re.match(r"^(R\d+),(.+)$", b)
        if a in HOLDS and mm and mm.group(2) == x:
            return "reload"
        if a in STORES:
            tgt = mm.group(2) if mm else b
            if tgt == x:                            # (a store through a pointer is not one REGSET zaps: IND)
                return None
        k -= 1
    return None


def compare(a, b, verbose=False):
    log = {"spill": [], "bytepair": [], "float": [], "reload": [], "copy": []}
    o, u = reduce_oracle(oracle(a), log), ours(b)
    sm = difflib.SequenceMatcher(None, [x[0] for x in o], [x[0] for x in u], autojunk=False)
    drop = set()
    for t, i1, i2, j1, j2 in sm.get_opcodes():
        if t == "insert":
            for j in range(j1, j2):
                r, jj = holds(u, j), j
                if not r:                           # the aligner cannot tell which of two neighbouring MOVEs is the extra one
                    for k in (j - 1, j + 1):
                        if 0 <= k < len(u) and k not in drop and holds(u, k):
                            r, jj = holds(u, k), k
                            break
                if r:
                    drop.add(jj)
                    log[r].append(u[jj])
    u2 = [x for j, x in enumerate(u) if j not in drop]
    x, y = [m for m, _ in o], [m for m, _ in u2]
    names = [("spill", "spill code (RMOD, %TMPn)"), ("bytepair", "byte pointers built in registers"), ("float", "PDP-10 float immediates"),
             ("reload", "redundant reloads (AMOD)"), ("copy", "register copies (allocation)")]
    seen = ["%s %d" % (t, len(log[k])) for k, t in names if log[k]]
    if verbose:
        for k, t in names:
            for e in log[k]:
                print("  %s: %s" % (k, " ".join(e)))
    if x == y:
        print("same after: %s (%d instructions and labels)" % (", ".join(seen) if seen else "no rule needed", len(x)))
        return 0
    print("UNEXPLAINED; rules applied: %s" % (", ".join(seen) if seen else "none"))
    for l in difflib.unified_diff(x, y, "oracle", "ours", lineterm="", n=2):
        print(l)
    return 1


if __name__ == "__main__":
    args = [a for a in sys.argv[1:] if a != "-v"]
    sys.exit(compare(args[0], args[1], "-v" in sys.argv))
