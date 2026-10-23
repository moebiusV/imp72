# free

`src/free.imp` is the allocation core of `decus/free.imc` (`FINIT`, `FALLOT`, `FRELOT`, `FADDEX`, `FADD`, `FINCSET`, `FGAP`,
`FPACK`, `FTOP`, `FFIXER`, `FCLOSE`), the last module the others stand on. `stage0/free_imp.c` is the C interface
(`fallot`, `fallot_new`, `frelot`, `fadd`, …) over it, and holds the diagnostic print-outs (`fmap`, `fstats`, the trace) and the
debugging aid `fcheck`. `stage0/free.c` is the all-C version stage 0 uses.

## What differs from free.imc

- **The arena is at the word `fb`** (`imp.h`; `fmem` is a macro over it), which the IMP modules read as `FB` and the C modules as
  `fmem`; before this module was ported the two were kept in step by a sync at every entry. It is a block of 64 bit words grown with
  `realloc` (`CCALL REALLOC`), not the low segment grown with `CORE`; new words are zeroed with `memset`.
- **The halves of a word are 32 bits** (`HL`, `HR`, `HW` of `impsyn64.imc`), so the length of an array is not limited to 18 bits. Never
  use `FLEN` of `impsyn64.imc` inside this module: its `<R>` is 18 bits.
- **BLT and EXECUTE are `memmove`** (`CCALL MEMMOVE`). `FALLOT(name,N)`, which the original told from `FALLOT(A,N)` by the value of A
  being over 18 bits, is a separate routine, `IFNEW`, taking the packed name.
- **The registry of relocatable arrays** holds, for each, the address of the C variable that points at the array and the address of
  the C routine to call after it moves (the original packed `LOC(A),,S` in a halfword). It is a `realloc`ed table of 16 byte entries.
  `IFFIXER` stores through the variable's address (`[[RELP+16*(Y-1)]]_X+4`) and calls the routine through `impcallfix` in C.
- **The routines are named IFALLOT, IFRELOT, …** because `impsyn64.imc` already turns `FALLOT(...)` into a call of the glue
  (`IMPFALLOT`, which calls `fallot`, which calls `IFALLOT`).
- **The diagnostics stay in C**, called with `CCALL`: the name of an array is eight bytes with the first character in the top byte,
  and `FMAP` writes to standard error as it did before.

## Things the port found

- **IMP has no operator precedence**: all binary operators are equal and go right to left, so `RELP+16*(Y-1)+8` is
  `RELP+16*((Y-1)+8)`. The slot of the registry was read from the wrong place, and the first relocation crashed in `impcallfix`. Write
  `(RELP+16*(Y-1))+8`. (`FB+8*(A)` is right: the product is done first.)
- **Relational operators in a condition** need their own parentheses: `A NE 0 OR B NE 0` is not two comparisons.
- **IMP is not re-entrant**: `LOCAL` only renames, so a variable is one location per subroutine (`LOCAL` is used here so that the
  routines do not share `K` and `X`). Nothing in the allocator is called again from inside itself: the routines the registry calls
  after a move (`dorgfix`, `gorgfix`, `torgfix`) only recompute pointers.
- **The module calls C with values**, so it needs `lib/ffi.imc` when it is compiled (`tools/stage1.sh` uses the driver, which loads it;
  `tests/self/stage1.sh` adds it for this module), and the glue lint accepts `CCALL`.
- **The shared names** (`forgn`, `fend`, `nreloc`, `falln`, `fb`) are in `src/shared.txt`, so that stage 1's renaming leaves them alone.

## Checks

The stage 1 and stage 2 tests (IR and C identical with the module in IMP), the whole suite, and `IMP_FCHECK=1`, which walks the list of
arrays after every glue call and finds nothing.
