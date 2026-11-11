# The seed, part 3: semantics, the code tree, RSYN and the driver

`stage0/dosem.c`, `entree.c`, `cotree.c`, `rsyn.c`, `semtab.c` and `debug.c` correspond to `src/dosem.imp`,
`src/entree.imp`, `src/cotree.imp`, `src/rsyn.imp`, `src/dir.imp` (the table of semantic routines) and `src/debug.imp`.
`imps0.c` is the driver. The code generating routines are `stage0/gen_*.c` (`code-generator.md`).

## What runs

    stage0/imps0 [-s LETTERS] [-o FILE] [-c IRFILE] [-T TARGET] SYNTAX [FILE ...]

`imps0 src/llvm-ir/syntax.imc` reads the syntax file of the language. `rsyn` reads the syntax for syntax with the
lexer alone; the parser, the code tree and the semantic stack, driven by the productions just defined, read the remaining
statements. The result is the syntax graph, the semantics array and the code tree, with 177 productions entered (the
number of syntax statements in the file). `tests/stage0/test_syntax.sh` (run by `make -C stage0 check`) checks the
count and the two `DUPLICATES ONE ALREADY` advisories the file produces, runs the dump switches `S`, `T`, `X` and `N`, and
loads `src/impsyn.imc` after the syntax file.

Every further file is parsed with the syntax so far: `stinit`, `cotree(0)`, `parse("#PG")`, `ckdone`. `-c IRFILE` writes the
IR of the program (`-` is standard output); `-T` names the machine the IR is lowered for (`ir_settarget`: `amd64`,
`aarch64`, `i386`, `arm`); `-o FILE` receives the listing and the dumps. The switches of `-s` are the original's
`/A /B ...`, one letter each (`finam[5]`, `stage0/imp.h`): `A` assembly listing, `E` trace productions entered, `G` trace
the parser and error corrector, `I` print the directory, `L` source listing, `M` trace tree matching, `N` print the
connectivity matrices, `P` trace semantics execution, `S` print the semantics array, `T` print the code tree, `X` print
the graph, `Y` list the source on the terminal, `Z` statistics.

The same binary is the compiler `impstage0` when it is called by a name that begins `impstage`. It reads
`src/llvm-ir/syntax.imc` and `lib/ffi.imc`, runs the kernel with `-c` into a temporary directory, and makes an
executable (default), an object (`-c`), LLVM IR (`-S`) or a run (`--run`) with the LLVM back end in the process
(`stage0/llvmorc.c`) and the linker in the process (`stage0/impld.cpp`). Where its parts are is found relative to
the executable, in the source tree or installed; `IMPSYNTAX`, `IMPFFI` and `IMPLIB` override them.

## The words

`SEMS`, `PROSEM`, the code tree, the code stack and the semantic stack have the 64 bit formats of the IMP modules
(`docs/porting/layouts.md`); the comment at the head of `dosem.c`, `entree.c` and `cotree.c` gives each. `debug.c`
prints raw words in octal as the 64 bit numbers they are (`OCT 17` for a word of `SEMS`, `OCT 22` for a stuff word of
the code tree), and prints the bit matrix `NBITM` as the masks that the 64 bit `BIT` gives.

## Semantic routines

A semantic routine is registered by name with `dsem(name, fn)`; the id stored as the `SEM` property of the name is 1
plus its slot in the table of `semtab.c`. `dosem` calls it through `semcall(id, pars)` with `word fn(const word *p)`:
`p` is the array `PARS` (`MARGS` = 10 words), so that arguments beyond those passed hold what earlier calls left
there. `dsemi` registers a routine written in IMP, which takes its arguments by reference. `IMP_SLOG` in the
environment logs each call.

The routines registered are:

- `dosem.c`: `PAR`, `STACK`.
- `cotree.c`: `MAXWELL`, `MAXEND`, `SETPRI`.
- `rsyn.c`: `CASE`, `DEFCASE`, `QUOSEM`, `EQUOSE`, `SEMBIT`, `SYTRM`, `SYNTS`, `SEMANTICS`, `SYNTAX`, `SEMR`, `SEMP`,
  `SYNT`, `ENSTACK`, `VALU`, `REPVAL`. The bootstrapped syntax calls them for the rest of the syntax file.
- `gen_*.c`: every routine of the code generator, entered by `gen_init` in the order of `IMPSEM`, `CODEI`, `CODE2I`,
  `CODE3I` of the IMP modules.

## Differences from the original

- `CODEID` takes an identifier like `Q0REG0115` apart: the name `Q`, then the indicator `REG` with the numeric
  arguments `01` and `15`. The characters are the bytes of the name, the first in the low byte of a word.
- `MATCH` does not call `RMATCH` for a register item: a register matches whatever its number. A pattern that asks for
  a word of code (`WRD`) matches only entries for which `g_oneword` holds, as `ONEWORD` is 0 for an entry without
  code.
- `MAXWELL(N)` makes N+1 phony entries (`FOR J FROM N`).
- `TPRIN1` prints the bit matrix at least 5 words wide.
- The chains of code that `cotree.c` copies (`g_copyco`) and compares (`sameas` through `g_mcode`) are the records of the
  IR machine (`docs/porting/llvm-ir-machine.md`).
