# The names of the tape, and of the port

An audit, made by comparing the `SUBR` names of each module of the tape (`tape/*.imc`, the `:=` copy of `decus/`) with those of the module that replaces it in `src/` (the kernel in `src/*.imp`; the code generator and the
semantics in `src/llvm-ir/*.imp`). Nothing has been renamed or removed because of it; this is the list to decide from. Written 2026-11-02 from the tree at the time; to repeat it, list `SUBR NAME` lines of both files and `comm` them.

Not routines, so not in the lists: `FREES`, `FLEN`, `FADD` and the other macros of `impsyn.imc` are syntax, and keep their names in `src/impsyn64.imc`.

## Modules whose routines are the tape's, name for name

`stack`, `lex`, `parse`, `ercor`, `cotree`, `dosem`: no routine added, dropped or renamed.

## (a) A tape routine that is gone, with the reason

| module | routine | why |
|---|---|---|
| `dir`, `graph`, `entree` | `DORGFIX`, `GORGFIX`, `TORGFIX` | They set the origin words of the tables (`DORG`, `DORG1`, …) from the free area, after the area was moved or loaded. The arena of `src/free.imp` is reserved once at one address, so the origins are set where the tables are made. (Reason inferred from the code, not stated on the tape.) |
| `impm` | `CORE`, `TMPCOR`, `TMPFL`, `GETMP`, `PEEKAT`, `RLEAS`, `OPENOUT` | TOPS-10 services by name (the core, the temporary file, the output file); the driver of the port uses the C library instead. `CORE` is not in `src/free.imp` any more: the arena is mapped once. (I have not read `GETMP`, `PEEKAT` and `RLEAS` one by one: they are in the same group of the tape.) |
| `debug` | `PCODE1` | The listing of the code in PDP-10 mnemonics; `src/llvm-ir/code3.imp` has a `PCODE1` of its own that lists the records of the IR. |
| `code` | `RCOCHK` | Walks the chain of PDP-10 code words checking the use of registers; there is no such chain in the IR code generator. |
| `code2` | `ADDCODE`, `OPCODE` | Emission of a PDP-10 instruction word; replaced by `ADDREC` (a record of the IR code generator). |
| `impsem` | `SUBTAG` | Makes a tag's name from a number, with a byte pointer into a PDP-10 word (`<+OP>:=045B`); the IR code generator names its labels with `IXNEWLABEL`. |
| `amod`, `omod`, `rmod` | the whole of them (12, 9, 20 routines) | The assembler, the output module (the `.REL` file) and the register allocator of the PDP-10 compiler. LLVM does these. |

## (b) A routine that is new

| module | routines | what for |
|---|---|---|
| `free` | `FBACK`, `FFWD`, `FLENX`, `FNEW`, `FSETB`, `FSETF`, `FSETL` | Readers and setters of the two halves of a block header (`HL`/`HR` of `FREE(X)`, the length of `FREE(X+2)`), and the allocator of a block by name (`FNEW`). |
| `dir` | `SEMCALL`, `SEMCOUNT`, `SEMNAME`, `SEMREMAP` | `SEMCALL`: the call of a semantic routine by its number, with a frame stack of its own (`SEMSTK`; the locals of IMP are static, so a routine that calls itself through the table needs one). `SEMCOUNT`, `SEMNAME`, `SEMREMAP`: the table of routines for the image (`-I`). |
| `rsyn` | `RSYNI`, `IMPSVDAT`, `IMPSVNUM`, `IMPSVSET` | The state of the syntax tables for the image (`-I`). (Inferred from the names and `src/image.imp`; not read line by line.) |
| `impm` | `COMPIL`, `IMPARGV`, `IMPBYTE`, `IMPCLOSELIST`, `IMPGETC`, `IMPK`, `IMPNOISY`, `IMPOBJ`, `IMPOPENIR`, `IMPOPENLIST`, `LISTOUT`, `MKNAME`, `OUTCS`, `SETSW`, `USAGE` | The command line, the listing and the files of a compiler with options like a C compiler's. (`ERROR` is defined in `impm.imp`; on the tape `ERROR` is in `free`.) |
| `code` | `REGFIELD` | The register field of an operand (the register is in the high half of the second word). |
| `code2` | `ADDREC`, `CJUMP`, `JOP`, `MOVEBLK`, `SETNEXT` | `ADDREC` adds a record to the code of the IR code generator, as `ADDCODE` added a word. The others are named for the jump and the block move: not read one by one. |
| `code3` | `PCODE1` | See (a). |
| `impsem` | `CVAL`, `IINIT`, `ISCVAL`, `PAIRFIRST` | Not read one by one. |

## What to look at

- No tape routine has been *renamed*. The two earlier cases (the `I`-prefixed free routines and `FCORE` for `CORE`) were put back or removed, as the table shows.
- (a) is all PDP-10 or 18-bit machinery. If you want any of it kept as a stub with the tape's name (for instance `CORE` returning 1), say which.
- (b) is the part to read: `FBACK` … `FSETL` and `SEMCALL` … `SEMREMAP` are small and could be folded into the tape's routines (`FREES` with a mask, a table call without the frame stack) if you want fewer new names, at the cost of the 64-bit layout being less explicit.
