# Porting entree.imc (src/entree.imp)

The sixth module of the compiler's own sources on the 64 bit model, and the first with the 36 bit formats of the semantics:
the code tree builder and matcher (`ENTREE`, `ENTMARK`, `INSNO`, `NUNO`, `COMPNO`, `CODEID`, `MATCH`, `ENTRCA`,
`ENTRC1`). `stage0/entree_imp.c` has the by-value C entry points, the tree accessors and `TPRINT`; the module reaches
`CONVC`, `SNARG`, `SNODEN`, `NOTEPROD`, `ENPROD`, `MATCHP`, `SAMEAS` and `ONEWORD` through `impglue.c`.

Conventions for a module with 36 bit formats (`helpers.md`): the stuff words of the tree are 36 bit numbers sign extended
(`SX36` when storing, `U36` before a shift or a test of the high bits); the links word of a node has three fields of 20
bits (`TSCR`, `TALT`, `TNODEN`, as `entree.c` has it; the original packs 12 bit fields in 36 bits).

The port is from `decus/entree.imc` for the structure and from `stage0/entree.c` for the 64 bit decisions. Differences:

- `CODEID` packs the characters of a name five to a word as the original does, and calls `DIR` through `IMPDIRP`; the table of
  the special identifiers (`REG`, `CON`, ...) is built from numbers, since a string constant is bytes here. The original's
  `GO TO SWTB[NSW-1]` is a computed `GO TO`.
- `MATCH` does not ask `RMATCH` for a register (as `entree.c`); `TN` is a reference parameter and is left at the node that
  matched, which `cotree` reads.
- `ENTRCA` marks nodes in bit 35 of the stuff word; the shift of the test is written `ARS 9` twice.

What the port found, and fixed in the translator and the code generator:

- **A constant shift was folded in 36 bits**, so `7 LS 48` was 0. The earlier modules had such constants (`2 LS 48` in
  the error corrector's stack, the type of the free node of the graph) and were silently wrong; `CONOP` now folds `LS` in 64 bits
  and the code generator takes a constant for an immediate only when it is below 2^18 (`dewop`).
- **A twelve digit octal constant with the top bit set is negative** (`777777777777B` is -1, as on the PDP-10), so the
  helpers write the 36 bit masks with a leading zero (`0777777777777B`).
- **`X AND 777777B` is `HRRZ`**, which here takes 32 bits: `R18` is written without that constant.
- **`RETURN -1`** returned garbage: the return value is the register of the last move, and `-1` is loaded by `SETO`. `SETO`,
  `SETZ`, `MOVN`, `MOVM` and `MOVS` count now.
- Every glue call that can allocate free storage must refresh `FB` afterwards (`impglue.c` does).

Debugging aids: `IMP_ELOG=1` logs every call of `ENTREE` (and `MATCH`) on stderr, `IMP_ETREE=1` adds the tree;
`IMP_FCHECK=1` checks the free storage headers after each glue call (`IMP_FCHECK_ABORT=1` aborts at the first bad one).
Comparing the logs of the C and IMP versions found all of the above.
