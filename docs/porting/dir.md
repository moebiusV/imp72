# Porting dir.imc (src/dir.imp)

The second module of the compiler's own sources that runs on the 64 bit model: `HASH`, `DIR`, `DSEQ`
and `DNAME` of `decus/dir.imc`. The rest of the original (`PNAME`, `GNAME`, `DSTATS`, `DSPILL`,
`RADI50`, `CONVC`, `STCON`, `NEWNAME`, `DPROP`) stays in C (`stage0/dir_imp.c`), which also owns
the C interface; it reads the IMP module's variables `prp`, `alf`, `dm` and `dn` (`src/shared.txt`).

What had to change from the original:

- **Halves are 32 bits.** `RS 18` of the original, the left half of a node word, is `RS 32`.
  The translator turns `A LS 18` / `A LS -18` into the halfword instructions (syntax.imc), so
  with 32 bit halves it is a shift by 32 anyway. The true shift by 18 that `HASH` needs is
  written `((NM RS 9) RS 9)` (and `RS` associates to the right: the parentheses are needed).
- **`[ALFX+K]` is a word address plus a count.** In the original `ALFX` holds a word address
  and `K` a number of words. Here the address is bytes, and the translator scales only
  constant offsets, so the name array is addressed by index with `FREE(ALF+J+K)` and
  `FREES(ALF+AX+K,V)`.
- **FINCSET** is mapped like the free storage routines (`src/impsyn64.imc`).

The port found a bug in the translator, old and not shown by any test: a **variable subscript
of a parameter array** was not scaled to bytes. `MOVEI R,@0(R15)` loads the pointer that the
caller passed (bytes), `ADD R,I` adds a number of words. `trans.c` now scales the number added
to an address register that the instruction before loaded (`lastaddr`). `tests/oracle/parr.imp`
has it, with a golden from the original.

`tools/stage1.sh` builds the compiler with both modules replaced; `tests/self/stage1.sh`
requires the same IR as stage 0 from it for every program (the native tests and the modules of
the tape), and `tests/stage0/test_dir.c` passes on it.
