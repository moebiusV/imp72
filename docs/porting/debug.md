# debug

`src/debug.imp` is `TPRIN1`, `SEMPR1`, `GPRIN1`, `PSTAC1` and `PARTR1` of `decus/debug.imc`, the diagnostic print-outs
(`-s T`, `S`, `X`/`N`, `P` and `G`), written with the original's `PRINT` statements. `stage0/debug_imp.c` is the C interface and
`HELPD`; `stage0/debug.c` and `stage0/pr.c` are the all-C versions stage 0 uses; `PCODE1` (the listing of PDP-10 code) is
`stage0/pcode.c` in both.

## Why it is worth having in IMP

In the C version the dumps are written to the file directly, while the IMP modules (`parse`, `entree`, …) use `PRINT`, which keeps
the line it is building in a buffer until `/`. A trace line that a IMP module starts and a C printer finishes came out in the wrong
order (`PARSE #1` after the rest of the line). With the printers in IMP all of them go through the one buffer, as in the
original, where `PNAME` is also a `PRINT`.

## What differs

- The tables the printers read (`CT`, `NCT`, `NODEN`, `SBITM`, `SEMS`, `PROSEM`, `GRNODES`, `GRBITM`, …) are shared `COMMON`
  variables (`src/shared.txt`), so the routines take none of the arguments the original passed; `debug_imp.c` ignores them.
- **Octal.** `PRINT OCT n` of the runtime writes the whole 64 bit word, and the original's, 36 bits: the printers take the bits
  with `U36`. The runtime now blanks the leftmost `n-12` positions of an `OCT n` over 12 wide, as the manual says (it used to zero
  fill them).
- **The matrix NBITM** is printed as the masks the original's `BIT` macro gave, bit `j` of a 36 bit word, `1 LS (j MOD 36)`.
- **Columns past the end of a bit matrix** read the next row, or whatever follows the matrix, so the printouts of the code tree's
  matrix (36 times the number of 36 bit words that would hold a row, at least 5, columns) gave different numbers in the two stages;
  they print 0 now, in both.
- Every expression is fully parenthesised: IMP has no operator precedence (`docs/porting/free.md`).

## What the port found in `dir`

Comparing the directory dump (`-s I`) of the two stages showed `DIRECTORY HASHED INTO 3 BOXES. OVERFLOW IS 384.` in stage 1: the
hash function of `src/dir.imp` kept the hash in a variable called `HSH`, which is also the macro of `impsyn64.imc` for the width of
a half (32), so the hash was a constant and all names went into chains. Nothing that compares IR could see it.
It is `HV` now.

## Test

`tests/self/dumps.sh`: seven sets of switches on three programs, stage 0 against stage 1. The `P` and `M` traces print absolute free
storage indices and are left out: the IMP modules allocate `PSHMI` and `ARRYS` earlier than the C ones, so the arrays are
elsewhere.
