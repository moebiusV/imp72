# The helpers of src/impsyn64.imc

The syntax file that the ported modules are compiled with defines names for the things that were
written as numbers in the original, so that a module reads like the original and the 64 bit model is in
one place.

- **Halves.** `HSH` (32, the width of a half), `HMASK` (the right half), `HL(w)`, `HLS(w)` (signed), `HR(w)` and `HW(l,r)`,
  as in `imp.h`. Never write a shift by the constant 18: `syntax.imc` turns `A LS 18` and `A LS -18` into the
  halfword instructions, which here move 32 bits (`GTSH`, `NBMASK` name the fields of a graph node).
- **The 36 bit formats** of the semantics (SEMS, the code tree, the code stack, the semantic stack): a
  word holds a 36 bit two's complement number, sign extended. `U36(w)` takes the bits (use it before a
  right shift or a comparison of the high bits), `SX36(x)` makes the word (use it when storing),
  `L18(w)`, `R18(w)` are the 18 bit halves and `W18(l,r)` makes a word of two.
- **Free storage and the directory.** `FREE`, `FREES`, `FLEN`, `FADD`, `FORG`, `DPROP`/`DPROPS` and the graph
  accessors `SCR`, `ALT`, `GTYPE`, `GVAL`, `GNB`.

`dir`, `graph`, `parse` and `ercor` use the first group; the modules that keep the 36 bit formats will use the second.
