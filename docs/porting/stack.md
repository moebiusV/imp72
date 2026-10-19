# Porting stack.imc (src/stack.imp)

The first module of the compiler's own sources that runs on the 64 bit model.
`src/stack.imp` differs from `decus/stack.imc` in two places only:

- the bit matrix package packs 64 columns to a word, not 36 (`J/64`,
  `J-64*COL`);
- nothing else: word offsets such as `[STX+1]` keep their meaning, because an
  offset from a register that holds a pointer is a number of words (`trans.c`
  `effaddr` scales it by 8).

`src/impsyn64.imc` replaces `impsyn.imc` for the ported modules: free storage
is an arena whose base is the variable `FB`, `FREE(A)` is the word `A` of it,
`FORG+A` is its address, `BIT` is written with 64 bit words. The fields `<L>`
and `<R>` are still 18 bits, so free storage indices stay below 2^18.

`tests/self/` links the module with C stand-ins for the free storage package
(`fallot`, `frelot`, `faddex`, `error`) and checks the stack, `pushmi` /
`pullyu` and the bit matrix (set, read, growth in both directions, `bitor`).
The stand-in allocator gives the arrays slack: `BITS` indexes past the length
it asks for, as in the original, which relied on the allocator's rounding.

Running real compiler code found three faults in the translator, all fixed:

- an offset from an index register was taken as bytes, not words;
- `IDIV` wrote its remainder to AC+1 even when the compiler had not reserved
  that register, clobbering a live value (the original's `IDIVI R,64` has the
  same shape; its register allocator treated AC+1 as free);
- a stale object file hid the first fix, which is why `make check` rebuilds.

## Stage 1

`tools/stage1.sh` builds `build/stage1/imp72s1`: stage 0 with `stack.o`
replaced by `src/stack.imp` compiled by `imp72c`. Three things make that work:

- **Names.** IMP has one flat name space and C has functions called `val` and
  `pstack`, so every symbol the IMP module defines is renamed `impl_NAME`
  (`objcopy`), except the ones listed in `src/shared.txt` (`st`, `stp`, `fb`),
  which the C side uses too.
- **Interface.** `stage0/stack_imp.c` has the C interface of `stack.c` and
  calls the IMP routines with the addresses of its arguments (IMP passes by
  reference). It sets `fb`, the base of free storage that `FREE` reads, before
  every call, as the block moves when it grows.
- **Free storage.** The module still uses the C `free.c`, through
  `impfallot`, `impfrelot`, `impfaddex` and `imperror` (pointer-passing entry
  points in the same file). The syntax layer renames `FALLOT` and the others to
  those. `free.imc` itself is not ported: its executed `BLT` and `JSA`
  words and its code addresses in 18 bit halves have no 64 bit equivalent,
  and `free.c` does the same job on the C heap.

`tests/self/stage1.sh` checks that `imp72s1` writes the same IR as `imp72s0`
for the native tests and for 14 modules of the tape, 22 programs in all.
