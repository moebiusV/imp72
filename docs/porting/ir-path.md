# The IR path: the code generator that compiles to pcc IR

`src/ir/` holds the translation of the tape's code generator into 64 bit IMP, for the machine that is pcc IR (`ir-machine.md`; the design is in
`ir-direct.md`). Files: `code.imp`, `code2.imp`, `code3.imp` (`code.imc`, `code2.imc`, `code3.imc`), `impsem.imp` (the register and declaration
routines of `impsem.imc`), `demit.imp` (what `amod`, `rmod` and `omod` did: the chains of records to IR, through `stage0/ir.c`), `irsyn.imc` (macros),
`syntax64.imc` (`decus/syntax.imc` with the operations of that machine; no immediate, self or both modes, halfwords, skips or register pairs).
`stage0/irsem.c` registers the routines (the table of semantic routines is a table of C function pointers) and has the C helpers the IMP code calls
by value (`CCALL`).

## Building and using it

    make
    tools/direct.sh                    # build/direct/imp72d, build/direct/imp72stage-ir (the driver)
    DIRECT_OUT=build/direct3 DIRECT_DRIVER=build/direct/imp72stage-ir DIRECT_LIB=build/lib/libimp72raw.a \
        SRC_MODULES="free debug stack dir graph parse ercor entree cotree dosem rsyn lex" tools/direct.sh   # the compiler's own modules, too
    IMP72SYNTAX=src/ir/syntax64.imc IMP72LIB=build/lib/libimp72raw.a build/direct/imp72stage-ir -o hello tests/native/hello.imp

A REAL word is the bits of the IEEE double (no order preserving encoding), so programs link `libimp72raw.a`. `tests/direct/run.sh` runs the native
tests, the oracle goldens, the IEEE fuzz and a differential fuzz (the old path against this one) with it; `tests/direct/stage2.sh` has it compile
itself and requires the second generation to write the same IR as the first for every program. Both are part of `make check` (x86_64).

## What it found (and the rules for writing IMP that this compiler compiles)

- **The old compiler compiles this code, and it has its own limits.** A routine's call arguments are in static temporaries; an expression like
  `CCALL F(A,G(H(B)),C)` (a call in the argument of a call in the argument of a call) can overwrite one of them: build such expressions step by
  step in locals. A constant expression is folded in 36 bits: `(1 LS 52)-1` is -1, `NOT (63 LS 52)` is wrong; compute wide masks as the
  program runs. `X AND NOT 777777B` and `AND 777777B` are halfword moves (32 bits here): use `XOR R18(X)` and `R18`.
- **`[P+8]` is the word 8 WORDS after P, not 8 bytes.** `src/free.imp` meant bytes and had been writing its registry 64 bytes off: harmless until
  the layout of memory changed. The offset of an address with a register counts words (`[P+1]` is the next word); an address without a variable or a
  register is absolute.
- **An argument `FREE(X)` is passed by reference, as a pointer into the arena, and the arena moves** (`realloc`) when a routine that allocates is
  called. `ADDREC` copies its arguments first.
- **Cross-module calls**: the modules of `src/ir` call one another by name (all their symbols are renamed `impl_NAME` together); a module of `src/`
  is renamed by itself, so what it calls and does not define is the C side's (`DIR('X')` from `parse.imp` is `dir()` of C, which is given the
  string's address). The union rename made `dir.imp` and `parse.imp` talk to each other instead, and the grammar failed to load.
- **The halves of a word are 32 bits, constants written in octal are 36 bits** (`765432107654B` is negative: 36 bit two's complement), a LS count of
  64 or more gives 0, and `X RS 30` of a negative number is a 64 bit logical shift (`tests/golden/ir/constshifts.out`).

## What remains before the old path can go

The old path (`stage0/codegen.c`, `trans.c`, `pcode.c`, `impsem.c`) is the seed that compiles `src/ir` the first time. The options are to keep it as
the trusted stage 0 (the original plan: stage 0 in C, stage 1 compiled by it, stage 2 by stage 1) and make stage 1 the compiler that `make`
installs, or to commit the IR of `src/ir` and `src/*.imp` as a seed and drop the C code generator. Not done: `aarch64` and `i386` (the IR path
uses the same `ir.c`, so it should work; untried), `PRINT` of the text constants over 8 characters in `LOC`, `EXECUTE`/`CALLI`/`XWD`/`IOWD`
(PDP-10 only: they are in `syntax64.imc` as the original wrote them and fail with "UNKNOWN OPERATION IN THE CODE").
