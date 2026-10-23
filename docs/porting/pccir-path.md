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

## Optimization

Pass 2 of pcc runs with all its optimizations by default (`-xtemps -xdeljumps -xdce -xssa -xccp -xscp -xtailcall`; `IMP72CC1FLAGS=` sets other flags, an empty value
none). Two things in the IR make that work: pcc's `deljumps` wants the layout `PROLOG, LABEL, argument moves, LABEL, code, EPILOG`, so each function has a label
after the argument moves; and a double is never held in a temp (on the i386 `-xtemps` mishandles the x87 stack for them): the word is stored in a scratch word
`imp_fs0`/`imp_fs1`, read as a double where it is used, and the result is stored in `imp_fsr` and read back as a word. The seed's translator (`trans.c`) still holds doubles in temps,
so the seed driver leaves out `-xtemps` and `-xssa`.

## What differs from the original, and why

The rule is that the IMP text stays the original's, and changes only (1) where it emitted PDP-10 instructions, (2) where it relied on 36 bits, (3) for floating point.
`src/pccir/syntax64.imc` differs from `decus/syntax.imc` in: the opcodes (now operation numbers); the rules that made PDP-10 only forms (self and both
mode stores, `AOS`/`SOS`, `CAI`/`SKIP`/`JUMP` skips over one-word statements, register pairs for `IDIV`, `HRRZ`/`HLLZ` for `AND 777777B`, `BLT` setup, the
`CON18` immediate tests, which are `CON` here); `<L>` and `<R>` are 32 bit halves; the byte rules go through `BYTEP`'s descriptors. The routines of `code*.imp` and
`impsem.imp` differ from `code*.imc` and `impsem.imc` in: the records they add instead of instruction words (`ADDREC` for `ADDCODE`); no `RCOCHK`, `OCHK`, `OPCODE`
(they read PDP-10 instruction words); `DEWOP` has no immediate/self/both modes or user register protection; `SUBARG` and `SUBSCRIPT` make a copy of a register before
adding to it (a temporary of the IR is not a register that may be changed); `BYTEP`, `PRINPAR` (FLT) and `MOVE ... THROUGH` build their operations from the IR's; the
halves and fields of a stack entry are taken with `L18`/`R18`/`SX36` (it keeps the 36 bit layout); `FLOAT`, `FIX` and the constant negation use the IEEE double's bits.
Two small things are not of those three kinds, and are here for the 64 bit machine: `CONOP` does not divide by zero, and `ADDREC` copies its arguments before the arena can
move. Where the code is written around a bug of the seed compiler (`demit.imp`: expressions built in steps), it is the new module `demit.imp`, which the original has no
counterpart of (`amod`, `rmod`, `omod`).

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

## The PDP-10 constructs

`XWD A,B` is `(A AND HMASK) LS 32 OR (B AND HMASK)` (the halves are 32 bits) and `IOWD A,B` is `XWD -A,B-1`; `CALLI(A,B)` calls the library routine `imp_calli` with the
code and the AC (an operation of the machine, as the instruction was); `EXECUTE A` (the PDP-10's XCT, the operation 65 of the machine) does the instruction in the word A: the library's `imp_xct` decodes it in the 36 bit format, with the
registers 0R to 15R as its accumulators (the code stores the ones it uses in `imp_ac` before and loads them after). It does MOVE, MOVEI, MOVEM, MOVES, MOVN, MOVM, EXCH, ASH, LSH,
ADD, SUB, the boolean and halfword groups, CALLI, TTCALL (OUTCHR, OUTSTR) and RELEAS; another instruction is an error at run time, for the rest work on 18 bit addresses (the
`BLT` and `JSA` that the tape's free storage routines execute are `memmove` and a call here). `HRL` and `HRR` of impsyn.imc are the operations 66 and 67. `tests/direct/progs/pdp10.imp` and `execute.imp` test them.

## What remains

The seed (`stage0/`: `codegen.c`, `trans.c`, `pcode.c`, `impsem.c` and the rest of the C kernel) stays as the trusted stage 0. Tried: x86_64 and i386 (the primary targets), and aarch64 (in a container). Not done: armv7 and the others (`make` skips the pcc IR compiler there). On a 32 bit machine a call cannot carry a 64 bit word, so the helpers that IMP calls with `CCALL` (`stage0/pccirsem.c`) take `long`s and a 64 bit value crosses as two halves, and the runtime routines for shifts take addresses, `PRINT` of the text constants over 8 characters in `LOC`, 
