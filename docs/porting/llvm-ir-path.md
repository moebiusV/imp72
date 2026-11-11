# The IR code generator in src/llvm-ir

`src/llvm-ir/` holds the code generator of the compiler: the tape's `code.imc`, `code2.imc`, `code3.imc`, `impsem.imc` and
`syntax.imc`, with the operations of the IR machine (`llvm-ir-machine.md`) in place of PDP-10 instructions, and
`demit.imp`, which takes the place of `amod`, `rmod` and `omod`. How it works is in `docs/internals/code-generator.md`.

| file | contents |
|---|---|
| `code.imp`, `code2.imp`, `code3.imp` | `code.imc`, `code2.imc`, `code3.imc` |
| `impsem.imp` | the register and declaration routines of `impsem.imc`, `ASSEMC`, `FCON`, `CVAL` |
| `demit.imp` | the chains of records into IR trees |
| `ix.imp` | the IR tree builders and the IR text; the names of directory entries as the IR has them |
| `irsyn.imc` | macros: the records, the operations, the IR operator and type codes |
| `syntax64.imc` | `decus/syntax.imc` with the operations of the machine; no immediate, self or both modes, half words, skips or register pairs |

`decus/` is never edited. `shared.txt` lists the variables that the C of the seed shares with these modules; the rest of
a module's symbols are renamed `impl_NAME` when the modules are linked, so that they call one another by the tape's names.

## What is kept from the tape

The parser, the graph, the code tree, `dosem`, `rsyn`, `entree` and `cotree` are unchanged in design: the extensible
syntax machinery, the stack, the pattern matcher, `CASE ... OF`, `LOCAL ... IN "..."`. The stack entry has the tape's
three words (`layouts.md`). The routines keep the tape's names and structure; they differ in the instructions they make.

## What differs from the tape

`syntax64.imc` differs from `decus/syntax.imc` in:

- the opcodes are operation numbers of the machine (`ADDOP(6,A,B)`);
- `<L>` and `<R>` are 32 bit halves, and `HRL`/`HRR` are the operations 66 and 67;
- the byte rules go through the descriptors of `BYTEP`;
- `REAL` is the double's own bits, and a real constant is `FCON`'s double;
- constants are 64 bit.

The routines of `code*.imp` and `impsem.imp` differ from the tape's in:

- they add records (`ADDREC`, where the tape has `ADDCODE`);
- `RCOCHK`, `OCHK` and `OPCODE` do not exist; they read PDP-10 instruction words;
- `DEWOP` has no immediate, self or both modes and no protection of user registers;
- `SUBARG` and `SUBSCRIPT` copy a register before adding to it, because a temporary of the IR is not a register that may
  be changed;
- `BYTEP`, `PRINPAR` (`FLT`) and `MOVE ... THROUGH` build their operations from the machine's;
- `FLOAT`, `FIX` and the negation of a constant use the double's bits;
- `CONOP` does not divide by zero, and `ADDREC` copies its arguments before the array `CO` can move (an argument such as
  `FREE(R)` is passed by reference).

`demit.imp` has no counterpart on the tape.

## The PDP-10 constructs

`XWD A,B` is `(A AND HMASK) LS 32 OR (B AND HMASK)` and `IOWD A,B` is `XWD -A,B-1`; `CALLI(A,B)` calls the library routine
`impcalli` with the code and the AC; `EXECUTE A` does the one operation of the machine that the word holds (`impxct`;
`llvm-ir-machine.md`). `tests/direct/progs/pdp10.imp` and `execute.imp` test them.

## Building and testing

`tools/llvm-ir.sh` builds stage 1 with the seed from `src/llvm-ir` and the compiler's other modules, and stage 2 with stage
1 (`docs/BOOTSTRAP.md`, `docs/BUILDING.md`). `tools/direct.sh` is the step that compiles the modules, renames their symbols
and links them with the LLVM glue and the library; it writes `impd` (the kernel) and `impstage-llvm-ir` (the same
program with the driver). By hand:

    tools/direct.sh
    IMPSYNTAX=src/llvm-ir/syntax64.imc IMPLIB=build/lib/libimp.a build/direct/impstage-llvm-ir -o hello tests/native/hello.imp

- `tests/direct/run.sh` runs the native tests, the oracle goldens, the IEEE fuzz and `tests/direct/progs` through the
  compiler built by the seed.
- `tests/direct/stage2.sh` requires stage 2 to write the same IR as stage 1 for the native tests, the oracle programs and
  the modules of the sources.
- `tests/seed/run.sh` requires the seed and the stage compiler to write the same LLVM IR, byte for byte.
- `tests/golden/llvm-ir/` holds expected outputs of constructs of this machine (`constshifts.out`).

## Optimization

LLVM optimizes the module in the process with `default<O1>` (`stage0/llvmorc.c`, `src/llvm.imp`). The IR keeps two
properties it has from pass 2 of pcc: each function has a label after the argument moves (`PROLOG, LABEL, argument
moves, LABEL, code, EPILOG`), and a double is never held in a temporary: the word is stored in a scratch word
(`imp_fs0`, `imp_fs1`), read as a double where it is used, and the result is stored in `imp_fsr` and read back as a word.

## Writing IMP for this compiler

The IMP of the compiler's own modules is compiled by the seed and by the compiler itself. `docs/LANGUAGE-NOTES.md` has the
traps of the language; those that bear on this code are:

- Locals are static, so a routine that calls itself, directly or through the table of semantic routines, keeps nothing in
  its locals across the call (`SEMCALL` uses a frame stack of its own).
- The arguments of a call are passed by reference, and an argument such as `FREE(X)` is a pointer into an array that can
  move when the callee allocates; copy it first.
- A constant added inside brackets counts words and a variable counts bytes (`[P+8]` is `P+64`, `[P+K]` is the word at
  `P+K`); compute the address in a variable when bytes are meant.
