# Code generation and translation to IR

`imp72stage0 prog.imp` compiles an IMP72 program to a native executable:

    stage 0  (imp72s0)   parse, run the semantics of decus/syntax.imc
       |                  -> PDP-10 code chains          (codegen.c, as code.imc)
    trans.c               -> IR text                  (ir.c)
       |
    ir2ll + LLVM          -> object                       (in the process)
    cc                    -> linked with build/lib/libimp72.a

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
(the original calls `IFIX`); `FIX` of a real calls the runtime `ifix`; `HOOK`,
`MATCH` as noted in `stage0-semantics.md`.

## Translation (`trans.c`)

The chain of the whole program is read as a PDP-10 program over virtual
registers (an index into the register array `RG` is an IR `TEMP`; LLVM
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

**IR.** `ir.c` writes the IR as text: a node to a line, preorder; `!` prolog,
`^` label, `%` epilog, `*` text. The format is that of a compiler's pass 2 (pcc's,
which is where it comes from); `tools/ir2ll.c` reads it and writes LLVM IR, and
LLVM makes the object in the process.

## PRINT and the runtime

`PRINT` makes a call of `PRN.` followed by the print list as constant words,
each `N << 24 | offset` plus the address of the item's operand (`PRINPAR`,
`PRINCAL`, ported). `trans.c` turns the list into a call of
`imp_pr(n, p)` per item, which `lib/imp72io.imp` implements: IGR, OCT, STG
formats, `/`, TAB, FILL, kept between items and statements as in the manual.
`FINI(0)` is `fini`. A computed operand's address goes through a `%CALn` word
(an `HRRM` into it stores the whole address).

## Language features beyond the basics

- **Byte fields** `A<S,P>` (size S, P bits from the right): a shift and mask,
  with S and P constants or variables. `BYTEP` makes a descriptor
  (`bytedescs` in `codegen.c`); read as a value it is a one-word byte pointer:
  bits 63-58 size, 57-52 position, low 52 bits the byte address. `<X>`, `<+X>`,
  `LDB`/`DPB`/`IBP`/`ILDB`/`IDPB` work through such a word. Stepping moves to the
  next lower field, or to the next word (address + 8) when there is no room, as
  on the PDP-10 but with 64 bits in the word.
- **Variable shifts:** the shift count of a variable shift is an unsigned char
  (a restriction that came from pcc's amd64 table and is kept), in a tree with one variable shift and a temp as the
  operand, so `shiftop` (`trans.c`) goes through temps.
- **Computed `GO TO (L0,L1,...) N`:** `JRST table(N)` followed by the table of
  `JRST` words becomes a chain of compares.
- **`BLT`** (`MOVE B THROUGH N TO A`): `HRLI`/`HRRI` also leave the whole
  addresses in shadow temps, as a 32-bit half cannot hold a pointer; `BLT`
  calls `impblt`, which copies a word at a time upward as the PDP-10 does (with the ranges overlapping and the destination above the source the words copied are the words already copied: a word stored and BLT'd through an array fills it; `memmove` would not).
- **`DATA` words** (a tag followed by constants in the code) become initialised
  data named for the tag.
- **More than 6 arguments:** the extra pointers go in the outgoing stack area
  (an assignment to `0(%rsp)+8k` in the call's list, the outgoing stack area), and
  the callee reads them at `16(%rbp)+8k`.
- **Reals** are IEEE doubles in a word: the double's own bits. `FLOAT` is `FSC ac,233` (int to double),
  `FADR`/`FSBR`/`FMPR`/`FDVR` go through the scratch word `imp_fs`, floating
  constants (`FCON`, which uses `strtod` where the original sums digits in REAL
  arithmetic) are pooled whole. Results are not rounded to the original's 27 bit mantissa: a REAL is IEEE 754
  binary64. The unmodified `syntax.imc` applies integer compares (`CAM`, `JUMP`, `SKIP`) and `MOVN`/`MOVM` to any value, reals
  included, which the PDP-10's format allows and raw IEEE bits do not. `dewop` (codegen.c) knows the type of its operand and sets
  `REALOP` (bit 23 of the instruction word, free in the PDP-10 layout) on such an instruction when the operand is real; `trans.c`
  compares the two as doubles, negates by the sign bit and takes the absolute value by clearing it, and `conop` folds real
  constants as doubles. C converts with `lib/imp72.h` (the identity).
- **`CALLI`** goes to `imp_calli` in the runtime (a few calls: exit, time).

## Survey

`tools/survey.sh` runs every module of the tape through stage 0. All but
`impm` (the system dependent module, which writes TOPS-10 UUOs as inline
`DATA`) translate.

## Not done yet

`XCT`; `REMOTE` as separate code; immediate floating operands; calling C functions with arguments by value; `LET`, the other
UUOs; `FILE`, `DEVICE`, `FLT` in `PRINT`; the `(A[I]_B[I]) FOR I TO N` copy
loop is not turned into `BLT`.
