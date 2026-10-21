# Porting graph.imc (src/graph.imp)

The third module of the compiler's own sources on the 64 bit model: all of `decus/graph.imc` except
`GPRINT` (C: `gprin1` of `debug.c`) and the node accessors, which the C side also needs as functions.
`stage0/graph_imp.c` has the by-value C interface; the IMP module calls the C side through
`stage0/impglue.c` (`IMPBITS`, `IMPBITOR`, `IMPBITZ`, `IMPPUSHMI`, `IMPPULLYU`, `IMPPOUT`, `IMPPNAME`,
`IMPDNAME`, `IMPMSEC`, and the free storage routines), which takes every argument by reference.

Differences from the original:

- **Node words.** Halves are 32 bits (`SCR` is the right half of word 0, `ALT` the left); word 1 has
  the type at bit 48, the row of `NBITM` at bit 32 and the name (or production number) in the low
  32 bits. `src/impsyn64.imc` has `SCR`, `ALT`, `GTYPE`, `GVAL`, `GNB` and `DPROP`/`DPROPS` for these.
- **Names.** An underscore is the assignment operator, so the C side's `gr_bitm` is `GRBITM` here
  (`stage0/imp.h` maps the old names). `FINAM` is an array of the C side (`finam[5]` is `finam5`).
- **A string constant is bytes**, not packed five to a word: `'#'` is 35, not 35 shifted to the left of
  the word. The test for a nonterminal's number sign compares with `43B LS 29`.
- **Words of the bit matrices are 64 columns**, so the speeded up `GNT2TX` and the bit tests of
  `GPOUT1` and `GPOUT2` take the fast path for numbers below 64 (the C module does the same).

Two faults in the translator came out of this port:

- **Variables were all global.** Every undeclared name was a common symbol, so the `INIT` of one module was
  the `INIT` of all of them: `STINIT` ran, and `GRAPH` believed it had set itself up. In the original a name that
  is not `COMMON` belongs to its module (the loader links only the COMMON ones). They are now local
  (`.local` in the assembly, `static` in the C output); what the C side shares is declared `COMMON`
  in the IMP source (`src/shared.txt` lists the names). On Mach-O (`__APPLE__`) the translator writes `.lcomm` (untested: no Mac here).
  `tests/native/io.imp` read a line into a one word variable and had only been working because of the
  layout of the commons: it has room now.
- A parameter array with a variable subscript (see `dir.md`).
