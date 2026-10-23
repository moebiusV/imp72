# The pcc IR path: the code generator that compiles to pcc IR

`src/pccir/` holds the translation of the tape's code generator into 64 bit IMP, for the machine that is pcc IR (`pccir-machine.md`; the design is in
`pccir-direct.md`). Files: `code.imp`, `code2.imp`, `code3.imp` (`code.imc`, `code2.imc`, `code3.imc`), `impsem.imp` (the register and declaration
routines of `impsem.imc`), `demit.imp` (what `amod`, `rmod` and `omod` did: the chains of records to IR, through `stage0/ir.c`), `irsyn.imc` (macros),
`syntax64.imc` (`decus/syntax.imc` with the operations of that machine; no immediate, self or both modes, halfwords, skips or register pairs).
`stage0/pccirsem.c` registers the routines (the table of semantic routines is a table of C function pointers) and has the C helpers the IMP code calls
by value (`CCALL`).

## Building and using it

`make` builds it (`tools/pccir.sh`): stage 1 is made by the seed (`stage0/`, the C code generator) from `src/pccir` and the compiler's own IMP
modules, stage 2 by stage 1, and `make install` installs stage 2 as `bin/imp72` (with `share/imp72/syntax64.imc` and `lib/imp72/libimp72raw.a`);
the seed stays as `bin/imp72stage0`. Where pcc, an ELF `objcopy` or x86_64 is missing, `imp72` is the seed, as before. By hand:

    make
    tools/direct.sh                    # build/direct/imp72d, build/direct/imp72stage-pccir (the driver)
    DIRECT_OUT=build/direct3 DIRECT_DRIVER=build/direct/imp72stage-pccir DIRECT_LIB=build/lib/libimp72raw.a \
        SRC_MODULES="free debug stack dir graph parse ercor entree cotree dosem rsyn lex" tools/direct.sh   # the compiler's own modules, too
    IMP72SYNTAX=src/pccir/syntax64.imc IMP72LIB=build/lib/libimp72raw.a build/direct/imp72stage-pccir -o hello tests/native/hello.imp

A REAL word is the bits of the IEEE double (no order preserving encoding), so programs link `libimp72raw.a`. `tests/direct/run.sh` runs the native
tests, the oracle goldens, the IEEE fuzz and a differential fuzz (the old path against this one) with it; `tests/direct/stage2.sh` has it compile
itself and requires the second generation to write the same IR as the first for every program. Both are part of `make check` (x86_64).

## What it found (and the rules for writing IMP that this compiler compiles)

- **The old compiler compiles this code, and it has its own limits.** A routine's call arguments are in static temporaries; an expression like
  `CCALL F(A,G(H(B)),C)` (a call in the argument of a call in the argument of a call) can overwrite one of them: build such expressions step by
  step in locals. A constant expression is folded in 36 bits: `(1 LS 52)-1` is -1, `NOT (63 LS 52)` is wrong; compute wide masks as the
  program runs. `X AND NOT 777777B` and `AND 777777B` are halfword moves (32 bits here): use `XOR R18(X)` and `R18`.
- **`[X+8]` is 8 words or 8 bytes depending on the form of X.** With a variable or a register `I` the rule `[I+J0CON]` makes it `[J][I]`, and the
  offset J counts words (`[P+8]` is 64 bytes after P); the old code generator folds the sum of `[(P+16)+1]` into an add of bytes, and the IR
  path (whose rules lost the `CON18` alternatives) makes it words. `src/free.imp` wrote `[(RELP+16*SL)+8]` for the second word of a 16 byte entry,
  which is bytes on the old path and 64 bytes on the new one, and the registry broke only on the new layout of memory. It now computes the address
  in a variable (`QQ_RELP+16*SL; QQ_QQ+8; [QQ]_S`), which means the same on both. Write `[X]` with a pointer variable when you mean bytes.
- **An argument `FREE(X)` is passed by reference, as a pointer into the arena, and the arena moves** (`realloc`) when a routine that allocates is
  called. `ADDREC` copies its arguments first.
- **Cross-module calls**: the modules of `src/pccir` call one another by name (all their symbols are renamed `impl_NAME` together); a module of `src/`
  is renamed by itself, so what it calls and does not define is the C side's (`DIR('X')` from `parse.imp` is `dir()` of C, which is given the
  string's address). The union rename made `dir.imp` and `parse.imp` talk to each other instead, and the grammar failed to load.
- **The halves of a word are 32 bits, constants written in octal are 36 bits** (`765432107654B` is negative: 36 bit two's complement), a LS count of
  64 or more gives 0, and `X RS 30` of a negative number is a 64 bit logical shift (`tests/golden/pccir/constshifts.out`).

## What remains

The seed (`stage0/`: `codegen.c`, `trans.c`, `pcode.c`, `impsem.c` and the rest of the C kernel) stays as the trusted stage 0. Tried: x86_64 and aarch64 (in a container). Not done: 32 bit machines (i386, armv7: the build of `src/pccir` by the seed crashes there; `make` skips the pcc IR compiler on a 32 bit userland), `PRINT` of the text constants over 8 characters in `LOC`, `EXECUTE`/`CALLI`/`XWD`/`IOWD`
(PDP-10 only: they are in `syntax64.imc` as the original wrote them and fail with "UNKNOWN OPERATION IN THE CODE").
