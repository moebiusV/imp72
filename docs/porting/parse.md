# Porting parse.imc (src/parse.imp)

The fourth module of the compiler's own sources on the 64 bit model: `PARSE`, `SYMBOL`, `PFORCE`,
`COPY`, `POUT` and `PARSEF` of `decus/parse.imc`, the Irons parser itself. `stage0/parse_imp.c` has
the by-value C entry points, the hook for COTREE, and `PSTATS`, which prints the statistics that the
module keeps in common variables (`psts`, `pspn`, ...). `ERCOR`, `AMBIG` and `ISNAME` stay in C
(`ercor.c`); the IMP module reaches them, the graph and the other C routines through `impglue.c`,
which takes arguments by reference.

Differences from the original:

- **Halves are 32 bits** (parse entries, the output chain). The value in an output entry is a signed 32
  bit number, so it is read with `ARS 32`, where the original OR-ed in the missing sign bits.
- **Addresses are indexes.** The original works with word addresses (`L_FORG+P+IP; [L]`), here the
  free storage index is used with `FREE` and `FREES`. The block move in `COPY` is a loop.
- **`HRL(x,0)`** is `FREES` of the word with its left half cleared.
- **`PARSE` takes the index of the goal** (the C wrapper looks the name up); `PARSEF` also clears the
  initialised flag, as `parse.c` does.
- **`FLAG'Z'`** is the bit of the switch, `200000000B`.
- `PSTATS` is C: the original prints through `COTREE(1)` timing that the port does not have.
- A string constant is bytes: `ERROR(1,0)` passes the address of a zero word, which `impglue.c` reads as
  "no message".

Stage 1 is built with `stack`, `dir`, `graph` and `parse` replaced, and writes the same IR as stage 0
for every program of the native tests and every module of the tape. The parse trace (`/G`) goes through
the lib's PRINT, not the listing stream; it is not covered.
