# The routines of the tape and of this implementation

How the routines of this implementation relate to those of the original (`decus/*.imc`). The page compares the `SUBR`
names of each module of the tape with those of the module that corresponds to it in `src/` (the kernel in `src/*.imc`, the
code generator and the semantics in `src/*.imc`), the names the tape declares `COMMON` with `src/` and `lib/`, and
the macros and syntax elements of `decus/impsyn.imc` and `decus/syntax.imc` with `src/impsyn.imc`, `src/irsyn.imc` and `src/syntax.imc`.

`FREES`, `FLEN`, `FADD` and the other macros of `decus/impsyn.imc` are syntax, not routines, and keep their names in
`src/impsyn.imc`. The assignment is `_` in the tape and `:=` here, which changes no name.

Six modules have exactly the tape's routines: `stack`, `lex`, `parse`, `ercor`, `cotree`, `dosem`. In the others every
difference is one of three kinds: PDP-10 machinery that has no counterpart, a TOPS-10 service that has none, or a routine
that the 64 bit layout or the Unix driver needs.

## (a) A tape routine that is not in this implementation

| module | routine | what it does | disposition |
|---|---|---|---|
| `dir`, `graph`, `entree` | `DORGFIX`, `GORGFIX`, `TORGFIX` | set the origin words of the tables (`DORG`, `DORG1`, `GORG`, `TORG`, ...) and build byte pointers into the packed 18 bit fields (`BYTEP [GORG1]<15,18>`, the `DPBYP*`, `GBYP*`, `T*BP` variables) | not needed: the fields are read with shifts and masks (`DPROP`, `TSCR` and the rest in `impsyn.imc`), and the arena has one address |
| `impm` | `CORE(N)` | monitor call to grow the core | not provided in `impm`. `free.imc` has its own `CORE(K)` (as `free.imc` has), which makes the first `K+1` words of the reserved arena usable (`IMPCORESET`) |
| `impm` | `RLEAS(CH)` | release a channel | not provided: no channel is opened |
| `impm` | `TMPCOR`, `TMPFL` | the monitor's temporary core and the name of the job's temporary file `I10`, through which the command line reached `GETMP` | not needed: `GETMP` reads the command line from `argv` (`IMPARGV`) |
| `impm` | `PEEKAT` | tell a "bulldog" watcher program of the monitor which file is compiled | not needed |
| `code` | `RCOCHK` | walk the chain of PDP-10 code words checking the use of registers | no counterpart: there is no such chain |
| `code2` | `OPCODE` | the assembler's table of mnemonics | no counterpart (`ADDCODE` is kept, and adds an IR record where the tape's adds a PDP-10 word) |
| `impsem` | `SUBTAG` | make the name of the tag of a subroutine's entry, `%` and the name, for the re-entrant code of `SUBBEG` (switch `R`: a jump in the low segment to the routine in the high one) | no counterpart, and `SUBBEG` has no `RNTRNT` branch: there is one segment, and the text of a Unix process is shared and read-only whatever the switch (the help says `R` is always so); labels of the IR are numbered by `demit.imc` (`IXNEWLABEL`, `TAGLABEL`) |
| `amod`, `omod`, `rmod` | all (12, 9 and 20 routines) | the PDP-10 assembler, the writer of the `.REL` file and the register allocator | no counterpart: `demit.imc` writes IR and LLVM does this work |

`CORE` and `RLEAS` are the two tape routines whose absence depends on the file I/O of the implementation. If channel
I/O is implemented, `RLEAS` (the `fclose` of a channel) and the monitor `CORE` are reconsidered.

Shared variables of the tape that do not appear in `src/` or `lib/`: `DORG`, `DORG1`, `DORG2`, `GORG`, `GORG1`, `TORG` (the
origins above); `DPBYPA`, `DPBYPC`, `DPBYPI`, `DPBYPL`, `DPBYPU`, `GBYPNB`, `GBYPTY`, `TALTBP`, `TNONBP`, `TSCRBP` (the
prebuilt byte pointers); `IMPSMD` (the TOPS-10 buffer `TBUF` that held the command line); `RSYNLO` (a label of `rsyn`).
Of the syntax elements, `HRL` and `HRR` (half word stores) are defined in `irsyn.imc` over the 32 bit halves; `FLAG` (a test
of the `FINAM` switches) is not defined in `decus/impsyn.imc`, and the modules test the switches directly.

## (b) A routine that is not on the tape

| module | routines | what for |
|---|---|---|
| `free` | `FBACK`, `FFWD`, `FLENX`, `FSETB`, `FSETF`, `FSETL` | readers and setters of the halves of a block header (`HL`/`HR` of `FREE(X)`, the length in `FREE(X+2)`) |
| `free` | `FNEW` | allocate a block by name |
| `dir` | `SEMCALL` | call a semantic routine by number, with a frame stack of its own (`SEMSTK`; the locals of IMP are static, so a routine that calls itself through the table needs one) |
| `dir` | `SEMCOUNT`, `SEMNAME`, `SEMREMAP` | the table of routines for the image (`-I`): how many, the name of the I-th, the table made again in the order the image has |
| `rsyn` | `IMPSVDAT`, `IMPSVNUM`, `IMPSVSET` | the save count and date for the banner, kept in `rsyn` because the image leaves the variables of `impm` out |
| `impm` | `IMPK` | the driver program: reads the syntax with `RSYN`, compiles the files on the command line, returns 0 if all went well |
| `impm` | `COMPIL` | compile the file the lexer has open and print what the switches ask for (inline in the main program on the tape) |
| `impm` | `IMPARGV`, `IMPOPENLIST`, `IMPCLOSELIST`, `IMPBYTE`, `IMPGETC`, `MKNAME`, `SETSW`, `USAGE` | the command line and the files through the C library, where the original asks the monitor (`INIT`, `LOOKUP`, `ENTER`); `MKNAME` makes a file name (up to 4095 bytes, from the string `PUTN` of `lex` keeps, and 7 of extension; the tape's are six and three), `SETSW` sets the switches from letters |
| `impm` | `IMPOBJ`, `IMPOPENIR` | the object file of the system (or of `--target`) made from the IR just written, where the tape writes a `.REL`; the file of IR (`-` is standard output) |
| `impm` | `LISTOUT`, `OUTCS`, `IMPNOISY` | where `PRINT` goes (the listing, else standard output); print a C string; whether `ERROR` says anything |
| `code` | `REGFIELD` | the register field of an operand (the register is in bits 32-42 of the second word) |
| `code2` | `CJUMP` | a conditional jump; a real operand makes it the floating jump (`OP+OFJ`) |
| `code2` | `JOP` | the jump operation of a comparison code |
| `code2` | `MOVEBLK` | a block move, through the runtime routine `impblk` |
| `code2` | `SETNEXT` | set the link to the next record of the chain |
| `impsem` | `CVAL`, `ISCVAL` | a name is a C function (called with `CCALL`): its table `CVT` |
| `impsem` | `CDECL`, `CTYPEOF`, `CPFIND`, `CPADD`, `CPHEAD`, `CPTYPE` | the typed declaration of a C function (`EXTERNAL INT puts(PTR);`) and its table `CPR` (`docs/FFI.md`) |
| `code3` | `CSUBCALL`, `CCALL0`, `CPCOUNT`, `CPAR` | the `CCALL` of a declared function: the count of the arguments and their conversion to the declared types |
| `code3` | `CBADDR`, `CBNOCALL` | `CALLBACK(NAME)`, the address of the C entry of a subroutine, and the error of a `CCALL` of a callback (`docs/FFI.md`) |
| `demit` | `ECBADDR`, `ECALLT`, `CTLET` | the symbol `NAME@SIGNATURE` of a callback and of a typed call; `ECALLT` also names a call through a pointer `impcallt@SIGNATURE` |
| `impsem` | `IINIT` | the first use of the register and list tables (the tape initialises them inline) |
| `impsem` | `PAIRFIRST` | is register J the first of a pair (flag `40B` of the register table) |

`CCALL` (`lib/ffi.imc`) is not a routine of the tape. It calls a C function with arguments by value, as C does, where a
call of the tape passes addresses.
