# The names of the tape, and of the port

Complete audit, 2026-11-03. Nothing has been renamed or removed because of it; it is the list to decide from.

**Method.** The `SUBR` names of each module of the tape (`decus/*.imc`; the assignment there is `_` and in the port `:=`, which changes no name) were listed and compared with those of the module that
replaces it in `src/` (the kernel in `src/*.imp`; the code generator and the semantics in `src/llvm-ir/*.imp`). Then the names the tape declares `COMMON` were looked up anywhere in `src/` and `lib/`, and
the macros and syntax elements of `impsyn.imc` and `syntax.imc` were compared with `impsyn64.imc` and `syntax64.imc`. Every routine named below was read.

`FREES`, `FLEN`, `FADD` and the other macros of `impsyn.imc` are syntax, not routines, and keep their names in `src/impsyn64.imc`.

**Result.** Six modules have exactly the tape's routines: `stack`, `lex`, `parse`, `ercor`, `cotree`, `dosem`. In the others every difference is one of three kinds: PDP-10 machinery that is gone, a TOPS-10
service that is gone, or a new routine that the 64-bit layout or the Unix driver needs. No tape routine was renamed.

## The rule (decided by the user, 2026-11-03)

A tape routine that is not in the port does not come back as a stub. Either it is gone for good, because what it did has no meaning here, or it comes back **fully functional, in the new idiom**:
64-bit words, Unix, UTF-8, the C library underneath. The example given: file names follow the limits of Unix, not those of TOPS-10 (six characters and three of the extension).

## (a) A tape routine that is not in the port

| module | routine | what it did | disposition |
|---|---|---|---|
| `dir`, `graph`, `entree` | `DORGFIX`, `GORGFIX`, `TORGFIX` | set the origin words of the tables (`DORG`, `DORG1`, `GORG`, `TORG`, …) and build byte pointers into the packed 18-bit fields (`BYTEP [GORG1]<15,18>`, the `DPBYP*`, `GBYP*`, `T*BP` variables) | **gone for good**: the fields are read with shifts and masks (`DPROP`, `TSCR` and the rest in `impsyn64.imc`), and the arena has one address |
| `impm` | `CORE(N)` | monitor call to grow the core | **optional**: the arena is reserved once and `free.imp` already grows the usable part (`IMPCORESET`); it could return as "make N words usable" |
| `impm` | `RLEAS(CH)` | release a channel | **with study 3** (`pre-1.0-studies.md`): it is the `fclose` of a channel |
| `impm` | `TMPCOR`, `TMPFL`, `GETMP` | pass the command line from the monitor through temporary core or a temporary file `I10` | **gone for good**: `argv` (`IMPARGV`, `COMPIL`) |
| `impm` | `OPENOUT` | open the `.REL` output on a channel | **gone for good**: the driver opens the object file (`IMPOBJ`, `IMPOPENIR`) |
| `impm` | `PEEKAT` | tell a "bulldog" watcher program of the monitor which file is compiled | **gone for good** |
| `debug` | `PCODE1` | list the code as PDP-10 instructions | the listing of the records of the IR is `PCODE1` of `code3.imp` (same name, new job) |
| `code` | `RCOCHK` | walk the chain of PDP-10 code words checking the use of registers | **gone for good**: there is no such chain |
| `code2` | `ADDCODE`, `OPCODE` | emit a PDP-10 instruction word | replaced by `ADDREC` |
| `impsem` | `SUBTAG` | make a tag's name from a number, with a byte pointer into a PDP-10 word | **gone for good**: the IR code generator numbers labels (`IXNEWLABEL`) |
| `amod`, `omod`, `rmod` | all (12, 9 and 20 routines) | the PDP-10 assembler, the writer of the `.REL` file and the register allocator | **gone for good**: LLVM does these |

Shared variables of the tape that no longer appear anywhere in `src/` or `lib/`: `DORG`, `DORG1`, `DORG2`, `GORG`, `GORG1`, `TORG` (the origins above); `DPBYPA`, `DPBYPC`, `DPBYPI`, `DPBYPL`,
`DPBYPU`, `GBYPNB`, `GBYPTY`, `TALTBP`, `TNONBP`, `TSCRBP` (the prebuilt byte pointers); `IMPSMD` (the TOPS-10 buffer `TBUF` that held the command line); `RSYNLO` (a label of `rsyn`). Of the syntax
elements, `HRL` and `HRR` (halfword stores) are `HRL`/`HRR` of `syntax64.imc` too, over the 32-bit halves; `FLAG` (a test of the `FINAM` switches) is not defined in the port, which tests the switches in its own way.

## (b) A routine that is new in the port

| module | routines | what for |
|---|---|---|
| `free` | `FBACK`, `FFWD`, `FLENX`, `FSETB`, `FSETF`, `FSETL` | readers and setters of the halves of a block header (`HL`/`HR` of `FREE(X)`, the length in `FREE(X+2)`) |
| `free` | `FNEW` | allocate a block by name |
| `dir` | `SEMCALL` | call a semantic routine by number, with a frame stack of its own (`SEMSTK`; the locals of IMP are static, so a routine that calls itself through the table needs one) |
| `dir` | `SEMCOUNT`, `SEMNAME`, `SEMREMAP` | the table of routines for the image (`-I`): how many, the name of the I-th, the table made again in the order the image has |
| `rsyn` | `RSYNI` | the head of `RSYN`: sets up its tables and enters its semantic routines; a run from an image does not read the syntax again |
| `rsyn` | `IMPSVDAT`, `IMPSVNUM`, `IMPSVSET` | the save count and date for the banner, kept in `rsyn` because the image leaves the variables of `impm` out |
| `impm` | `IMPK` | the driver program: reads the syntax with `RSYN`, compiles the files on the command line, returns 0 if all went well |
| `impm` | `COMPIL` | compile the file the lexer has open and print what the switches ask for (inline in the main program on the tape; marked provisional in `modules.md`) |
| `impm` | `IMPARGV`, `IMPOPENLIST`, `IMPCLOSELIST`, `IMPBYTE`, `IMPGETC`, `MKNAME`, `SETSW`, `USAGE` | the command line and the files through the C library, where the original asks the monitor (`INIT`, `LOOKUP`, `ENTER`); `MKNAME` makes a file name (15 characters and 7 of extension, the tape's six and three), `SETSW` sets the switches from letters |
| `impm` | `IMPOBJ`, `IMPOPENIR` | the object file of the system (or of `--target`) made from the IR just written, where the tape wrote a `.REL`; the file of IR (`-` is standard output) |
| `impm` | `LISTOUT`, `OUTCS`, `IMPNOISY` | where `PRINT` goes (the listing, else standard output); print a C string; whether `ERROR` says anything |
| `code` | `REGFIELD` | the register field of an operand (the register is in the high half of the second word) |
| `code2` | `ADDREC` | add a record to the code of the IR generator, as `ADDCODE` added a word |
| `code2` | `CJUMP` | a conditional jump; a real operand makes it the floating jump (`OP+OFJ`) |
| `code2` | `JOP` | the jump operation of a comparison code |
| `code2` | `MOVEBLK` | a block move, through the runtime routine `impblk` |
| `code2` | `SETNEXT` | set the link to the next record of the chain |
| `code3` | `PCODE1` | the listing of the records of the IR (the tape's was in `debug`) |
| `impsem` | `CVAL`, `ISCVAL` | a name is a C function (declared by `EXTERNAL`/`CCALL`): its table `CVT` |
| `impsem` | `IINIT` | the first use of the register and list tables (the tape initialised them inline) |
| `impsem` | `PAIRFIRST` | is register J the first of a pair (flag `40B` of the register table) |

## Decided (the user, 2026-11-03)

The recommendations below are accepted, to be **examined again when channel I/O is back** (study 3 of `pre-1.0-studies.md`), because `RLEAS` and possibly `CORE` depend on it:

1. **`CORE`**: not brought back; `IMPCORESET` does the work.
2. **`RLEAS`**: not brought back now; decided again with channel I/O, as the `fclose` of a channel if channels return.
3. **(b), the new routines**: kept as they are (the header accessors `FBACK` … `FSETL`, `SEMCALL` and the rest).

`CCALL` keeps its name (asked 2026-11-03: it says "by value, as C does", which a bare `CALL` would not).
