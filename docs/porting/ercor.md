# Porting ercor.imc (src/ercor.imp)

The fifth module of the compiler's own sources on the 64 bit model: `ERCOR` (the parser's error
corrector), `FSUB`, `ISNAME` and `AMBIG` of `decus/ercor.imc`. `stage0/ercor_imp.c` has the by-value C
entry points. With it, `src/parse.imp` calls only IMP and glue; stage 1 builds with `stack`, `dir`,
`graph`, `parse` and `ercor` replaced and writes the same IR as stage 0 for every program.

Differences from the original:

- **32 bit halves.** The stack of the corrector holds `NODE OR TYPE LS 48` and goal and width as
  `GOAL OR WIDTH LS 32`; the parse entries are read with the 32 bit halves of `parse.md`.
- **Indexes.** `AMBIG` works with free storage indexes where the original used word addresses
  (`I_FORG+P; [I]`); the `BLT` executed with `XCT` that closes up the array after a parse is
  discarded is a loop. XCT is still not translated, and no longer needed.
- **`ISNAME`.** The test of the first character and "more than one character" is written out for the
  packed name (`NM AND 3777777777B`). The original's first test, `NM=R''''`, is kept: it compares what
  the previous call left in `NM` with 47B and never succeeds (`ercor.c` says so as well).
- Names are printed with `IMPPNAME`, a PRINT of the name through the print module's line buffer, as
  the original's `PNAME` is. A C `pname` writes to the listing at once and came out ahead of the
  text before it in the same line. The default stream of the print module is the listing (a hook that
  `impglue.c` sets), so a message printed by an IMP module and one by a C module are in order.
