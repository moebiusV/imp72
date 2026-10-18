# Code generation and translation to pcc IR

`imp72c prog.imp` compiles an IMP72 program to a native executable:

    stage 0  (imp72s0)   parse, run the semantics of decus/syntax.imc
       |                  -> PDP-10 code chains          (codegen.c, as code.imc)
    trans.c               -> pcc IR text                  (ir.c)
       |
    cc1                   -> amd64 assembly               (pass 2 of pcc)
    cc                    -> object; linked with build/lib/libimp72.a

## The code generator is the original's

`codegen.c` ports `code.imc`, `code2.imc`, `code3.imc` (and the subroutine
routines of `impsem.imc` in `impsem.c`). `syntax.imc` is unmodified: its
semantics call `DEWOP`, `ADDOP`, `STORE`, `HOOK`, `SUBCALL` ... with PDP-10
opcodes as before, and make a chain of instruction words in free array `CO`
for each stack entry. `imp72s0 -s A` lists the chains (`pcode.c`), in PDP-10
terms; `tests/stage0/golden/*.lst` holds listings read by hand. They are what
`amod`, `rmod` and `omod` would assemble, so they can be compared with the
original compiler's `/A` listings once the oracle exists.

This is why the code generator is ported and not rewritten for IR: loops,
conditionals and calls are encoded in the semantics of `syntax.imc` as skip
and jump instruction idioms (`CAMG R,X; JRST L`), and keeping them keeps the
syntax file as it is.

Changes to the original routines: no halfword constant idioms in `DEWOP`
(`HRROI`, `HRLZI` and `HRLOI` load constants by 18 bit halves; constants go
through the pool); `FIX` takes an argument of no arithmetic type as an integer
(the original calls `IFIX`; there is no floating point yet); `HOOK`,
`MATCH` as noted in `stage0-semantics.md`.

## Translation (`trans.c`)

The chain of the whole program is read as a PDP-10 program over virtual
registers (an index into the register array `RG` is a pcc `TEMP`; pcc
allocates). A subroutine starts at a tag followed by a zero word naming it
(`SF0 ADD2:` / `000 ADD2`); the statements before the first subroutine are the
main program, the function `imp_main`.

**Words, addresses.** A word is 64 bits, a halfword (`HRRZ`, `<L>`, `<R>`)
32. The address of a variable is its byte address. In an instruction that
names a variable, the constant and the index register count *words* (`V[I]` is
`&V + 8*I`); with no variable they count *bytes* (`[P+3]` is `P+3`). Where the
code generator adds a pointer register and an element index
(`SUBSCRIPT`), it scales the index. Constants are 36 bit values (±2^35); text
constants are 8 characters to a word, first character in the low byte, so the
word *is* the string in memory.

**Subroutines.** IMP, like FORTRAN, passes arguments by reference: after
`JSA R15,NAME` come argument words (`JUMP` instructions), each the address of
an argument (a variable, a constant in the pool, or a temporary the value was
stored in). A subroutine is a C function taking those pointers (`@k(R15)` in
its body is parameter k, a `TEMP` of the k-th argument register), returning the
value in `rax`: that of the register moved to just before the `JRA`. Up to 6
parameters. Variables are common symbols (`.comm`), in lower case; arrays
(`V IS 100 LONG`) are common blocks of 8*n bytes. So C calls IMP and the other
way round, by passing pointers.

**Skips.** `CAMG R,X` skips the next instruction if R>X: it becomes a
conditional branch to a label after the next instruction.

**IR.** `ir.c` writes the text `mip/reader.c` of pcc reads: a node to a line,
preorder; `!` prolog, `^` label, `%` epilog, `*` text for the assembler.
`cc1` is pass 2 of the fork built with `--enable-twopass` (the top-level
`make` does it, needing no yacc or lex); the writer of that fork's pass 1 is out
of date, which does not matter: ours writes the format its reader reads.

## PRINT and the runtime

`PRINT` makes a call of `PRN.` followed by the print list as constant words,
each `N << 24 | offset` plus the address of the item's operand (`PRINPAR`,
`PRINCAL`, ported). `trans.c` turns the list into a call of
`imp_pr(n, p)` per item, which `lib/imp72rt.c` implements: IGR, OCT, STG
formats, `/`, TAB, FILL, kept between items and statements as in the manual.
`FINI(0)` is `fini`. A computed operand's address goes through a `%CALn` word
(an `HRRM` into it stores the whole address).

## Not done yet

Floating point (`REAL`, `FLOAT`/`FIX` of non-constants); byte pointers
(`<s,p>` fields, `<+P>`, `BYTEP`); `BLT` (`MOVE ... THROUGH`); `XCT`; `DATA`
and `REMOTE`; more than 6 parameters; calling C functions with arguments by
value; `LET`, `CALLI` and the other UUOs; `FILE`, `DEVICE`, `FLT` in `PRINT`;
computed jumps (`GO TO (L1,L2) N`).
