# Stage 0, part 1: free storage, stack, directory, lexer

(Part 2, the parser, is in `stage0-parser.md`.)

Ports of `free.imc`, `stack.imc`, `dir.imc` and `lex.imc` to C99 in `stage0/`.
`make -C stage0 check` runs the unit tests in `tests/stage0/`.

## Reading the original

Two facts about IMP72 matter in every function:

- **There is no operator precedence.** `A+B*C` is `A+(B*C)`, and so is every
  chain of binary operators; relational operators (`=`, `LE`, ...) and `=>`
  bind looser. `FREE(ALF+J+FREE(PRP+DI) AND 777777B)` is
  `ALF+(J+(FREE(PRP+DI) AND 777777B))`. `syntax.imc` is the authority.
- **`FOR I TO N` includes N.** `FOR I TO N` is `I` from 0 to `N`;
  `FOR I FROM N` counts down from `N` to 0; `FOR I IN A,B,C` runs from `A` in
  steps of `B` up to and including `C`.

Parameters are by reference. `FADD(A,N,V)` and `FADDEX` therefore take the
address of the variable in C.

## Word model

`word` is `int64_t`. A halfword is 32 bits (`HL`, `HR`, `HW` in `imp.h`),
not 18. Everything the original addressed through `FORG` is an index into
one array, `fmem`.

## free.c (free.imc)

Doubly linked arrays with a four word header (back, forward; name; increment,
length; owner). `fallot` resizes or frees; a new array is `fallot_new`.
Relocatable arrays (`frelot`) have the owning C variable's address in a
registry slot (the original stored `LOC(A),,S` in the header, which does not
fit a halfword). `fpack` slides them over gaps; fixed arrays stay.
`FALLOT`'s search order is kept: grow in place, move into the gap before it,
best-fit gap elsewhere (`fgap`), grow core (`ftop`), pack and try again,
else "FREE STORAGE EXHAUSTED".

- The first dummy header is at index 8, not 0: `fgap` returns 0 for "none".
- `fgap` has no 64000 word ceiling.
- Trace output (`/F`) goes to stderr and is not formatted like the original.

## stack.c (stack.imc)

Unchanged in structure. Bit matrices hold 64 bits per word, so
`bit(i,j)` is bit `j%64` of word `i + rows*(j/64)`. `bit` checks its
indices and reads 0 outside the matrix (the version in `stack.imc`, which is
commented out; the one the compiler uses, from `impsyn.imc`, does not check and
reads whatever lies beyond).

## dir.c (dir.imc)

The hash is the original's, computed over the first five characters packed
as on the PDP-10 (7 bits each, left justified in 36 bits), so a name's
directory index is the number the PDP-10 compiler computes. The tests check
names that collide in the first bucket against indices worked out by hand
(`A`=780; `CDQ`, `CHR` in alternate buckets 786, 789; `CLS`, `CPT` chained at
1536, 1539).

- Names are kept packed five characters to a word in `alf`, as the original
  keeps them, so `dname(i,j)` returns the same words. `gname` gives a C
  string. Everything above the directory uses C strings.
- `dprop`/`dprops` replace the `DPROP`/`DPROPS` syntax of `impsyn.imc`; the
  small fields (`ATYPE`, `COM`, `ISNM`, `LXTY`, `USE`) sit at bits 32 and up of
  node word 2, the original's bits 18 and up moved to the 32 bit halfword.
- `convc` reads decimal, `dddB` octal, `dddBr` radix r. Constants have 64
  bits; a 12 digit octal constant with bit 35 set (`777777777777B`) is read as
  the negative 36 bit number it denotes, and `stcon` writes small negative
  numbers the same way, so names agree with the PDP-10's.
- A string constant (`'ABC`) is returned as a free array of 5 character words,
  as before.

## lex.c (lex.imc)

Port of `lexin`, `lex`, `nextchar`, `pline1`, `pline`. The input file is read
whole. `readcmd` and friends (the TOPS-10 command line) are not ported; the
driver reads `argv`. Line and token buffers are C buffers instead of the free
arrays `LEXL` and `LEXT`.

## Not yet checked against the oracle

Nothing here has been compared with output from the original compiler (that is
Phase 1). The hash and chaining behaviour are checked by hand-computed
values, not by the oracle.
