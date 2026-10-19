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
