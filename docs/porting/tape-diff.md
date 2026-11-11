# The port against the tape, routine by routine

Why: the port corrects the tape's routines for 64 bit words, UTF-8, Unix and IR instead of PDP-10 code, and adds no routine where correcting the original does it. A later tool is to parse `decus/` and `src/` and show a token or semantic difference, the way `wdiff` does for prose; for that a routine keeps its name, its place and its shape. This page is the first look, by routine name and by a rough similarity (the ratio of the tokens of the two texts of a routine, comments and string constants left out, 1.00 the same): it says where to look, it does not say what is wrong. Made on 2026-11-11 from the tree at that day.

## Module by module: routines on one side only

| module | tape | port | only on the tape | only in the port |
|---|---|---|---|---|
| `code` | 15 | 15 | `RCOCHK` | `REGFIELD` |
| `code2` | 20 | 22 | `OPCODE` | `CJUMP` `JOP` `MOVEBLK` `SETNEXT` |
| `code3` | 5 | 30 | - | `C0CALL` `CBADDR` `CBNOCALL` `CPAR` `CPCOUNT` `CSUBCALL` `FLDFIX` `FLDP` `FLDST` `FPBEG` `FPFLD` `STBEG` `STCONST` `STEND` `STFIND` `STFLD` `STINFO` `STOFF` `STP32` `STRESET` `STSIZE` `STTYPE` `TYALIGN` `TYSIZE` `TYSTRUCT` |
| `cotree` | 9 | 9 | - | - |
| `debug` | 7 | 7 | - | - |
| `dir` | 14 | 18 | `DORGFIX` | `BSWAPW` `SEMCALL` `SEMCOUNT` `SEMNAME` `SEMREMAP` |
| `dosem` | 19 | 19 | - | - |
| `entree` | 11 | 10 | `TORGFIX` | - |
| `ercor` | 4 | 4 | - | - |
| `free` | 19 | 22 | `ERROR` `FLEN` `FREES` | `FBACK` `FFWD` `FLENX` `FNEW` `FSETB` `FSETF` `FSETL` |
| `graph` | 23 | 22 | `GORGFIX` | - |
| `impm` | 17 | 28 | `CORE` `PEEKAT` `TMPCOR` `TMPFL` | `COMPIL` `IMPARGV` `IMPBYTE` `IMPCLOSEIRMEM` `IMPCLOSELIST` `IMPGETC` `IMPK` `IMPNOISY` `IMPOBJ` `IMPOPENIR` `IMPOPENIRMEM` `IMPOPENLIST` `LISTOUT` `MKNAME` `OUTCS` `SETSW` `USAGE` |
| `impsem` | 37 | 46 | `SUBTAG` | `CDECL` `CPADD` `CPFIND` `CPHEAD` `CPTYPE` `CTYPEOF` `CVAL` `IINIT` `ISCVAL` `PAIRFIRST` |
| `lex` | 12 | 12 | - | - |
| `parse` | 8 | 8 | - | - |
| `rsyn` | 22 | 25 | - | `IMPSVDAT` `IMPSVNUM` `IMPSVSET` |
| `stack` | 11 | 11 | - | - |
| `amod` | 12 | (none) | all | - |
| `omod` | 9 | (none) | all | - |
| `rmod` | 20 | (none) | all | - |

Modules of the port with no tape module: `driver`, `llvm`, `ir2ll`, `ir2llx`, `image`, `linktab`, `ix`.

## Routines in both, least similar first (below 0.60)

| similarity | routine | tokens on the tape | tokens in the port |
|---|---|---|---|
| 0.18 | `code2:ADDR` | 79 | 8 |
| 0.19 | `impsem:TSTRN` | 86 | 137 |
| 0.19 | `dir:DSEM` | 29 | 222 |
| 0.21 | `free:FINIT` | 132 | 146 |
| 0.22 | `code:STORE` | 540 | 80 |
| 0.24 | `code:COPYCO` | 342 | 297 |
| 0.24 | `dosem:SEMPER` | 46 | 12 |
| 0.24 | `impsem:FCON` | 309 | 492 |
| 0.25 | `free:FRELOT` | 58 | 195 |
| 0.26 | `code:DEWOP` | 414 | 113 |
| 0.27 | `impm:DATE` | 134 | 237 |
| 0.27 | `free:CORE` | 21 | 67 |
| 0.27 | `graph:GPRINT` | 56 | 10 |
| 0.29 | `code2:FREEZE` | 40 | 8 |
| 0.31 | `code:MCODE` | 342 | 400 |
| 0.32 | `code:NEXCO` | 47 | 10 |
| 0.32 | `impm:OPENIN` | 129 | 88 |
| 0.34 | `impm:OUTCHR` | 15 | 44 |
| 0.36 | `code3:BYTEP` | 491 | 213 |
| 0.36 | `free:FCLOSE` | 98 | 85 |
| 0.36 | `impsem:SUBEND` | 9 | 35 |
| 0.37 | `entree:TPRINT` | 48 | 12 |
| 0.37 | `impsem:SUBBEG` | 202 | 282 |
| 0.37 | `debug:SEMPR1` | 276 | 193 |
| 0.39 | `code2:FIX` | 152 | 193 |
| 0.39 | `rsyn:SYTERM` | 62 | 167 |
| 0.40 | `code3:CODE3I` | 29 | 116 |
| 0.40 | `impm:RUNPGM` | 53 | 22 |
| 0.41 | `free:FALLOT` | 582 | 655 |
| 0.41 | `free:FPACK` | 287 | 346 |
| 0.42 | `free:FTOP` | 148 | 148 |
| 0.43 | `impsem:REMOT` | 28 | 98 |
| 0.44 | `dir:GNAME` | 45 | 110 |
| 0.45 | `code2:ONEWORD` | 97 | 59 |
| 0.46 | `impm:TIME` | 102 | 134 |
| 0.46 | `lex:LEXSAV` | 19 | 37 |
| 0.47 | `lex:PUTN` | 32 | 54 |
| 0.47 | `impsem:ASSEMC` | 125 | 71 |
| 0.47 | `graph:NEWNO` | 92 | 195 |
| 0.49 | `impm:OUTSTR` | 15 | 26 |
| 0.49 | `lex:LEXRES` | 17 | 32 |
| 0.49 | `code:TAG` | 28 | 33 |
| 0.49 | `code3:PCODE` | 46 | 15 |
| 0.49 | `dir:STCON` | 89 | 130 |
| 0.51 | `code:UNPCK` | 38 | 61 |
| 0.51 | `dir:NEWNAME` | 146 | 153 |
| 0.51 | `impsem:REG2S` | 42 | 72 |
| 0.51 | `impm:INCHWL` | 15 | 32 |
| 0.51 | `code2:PRINPAR` | 432 | 577 |
| 0.52 | `cotree:PSTACK` | 46 | 16 |
| 0.52 | `lex:NEXTCHAR` | 239 | 343 |
| 0.53 | `impsem:ENLIST` | 50 | 78 |
| 0.53 | `entree:ENTRCA` | 184 | 245 |
| 0.53 | `code:SUBARG` | 264 | 160 |
| 0.54 | `debug:GPRIN1` | 273 | 201 |
| 0.56 | `code2:DEWFUN` | 36 | 64 |
| 0.57 | `free:FFIXER` | 67 | 70 |
| 0.58 | `code2:SUBRCALL` | 97 | 124 |
| 0.59 | `free:FINCSET` | 37 | 48 |
| 0.59 | `code2:SUBRPAR` | 77 | 58 |

235 routines are in both; 175 of them are 0.60 or more alike.

## Corrections made after this list was first made

- `RSYNI` is gone: `RSYN` is the tape's again, and returns after its head when its (unused on the tape) argument is 1, the run from an image (2026-11-11).
- `ADDREC` is `ADDCODE` again: the tape's routine that adds code to the chain of a stack entry keeps its name and adds an IR record where it added a PDP-10 word (2026-11-11).
- A helper that makes the code simpler and clearer is fine when it is defined next to its use and does not change the shape of the code (`SETNEXT` before `ADDCODE` and `HOOK`, `REGFIELD` before `COPYCO`); a routine that stands in for a tape routine is not.
- `GETMP` and `OPENOUT` are the tape's routines again, corrected: `GETMP` puts the next file of the command line in the command buffer as a command (its name between `!` marks, the `-s` letters as switches, the `%` that ends a command), and the main program takes it through `READCMD` and `OPENIN` as it takes a typed command; `OPENOUT` opens the output of a compilation (the file of IR, where the tape's opens the `.REL` file). `BF` holds a Unix path (2026-11-11).
- `--run` and the ORC JIT are gone: that was an addition that was not asked for.
- `IMPSVDAT`, `IMPSVNUM` and `IMPSVSET` stay: they look redundant (two variables could be `COMMON` in `impm`), and the test of the saved compiler's banner (`(A)`, `(B)`) showed why they are not: the image leaves the variables of `impm` out and carries those of `rsyn`, so the save count and date live in `rsyn` and `impm` reaches them through the routines (tried 2026-11-11).
- `free`: the six accessors `FBACK`, `FFWD`, `FLENX`, `FSETB`, `FSETF` and `FSETL` are helpers at the head of the module for the tape's `[X]<L>`, `[X]<R>`, `[X+2]<R>` and their assignments (the IR machine has no halfword operations); they stay. `FRELOT` and `FFIXER` are the tape's, corrected in place: the header word that held the address of the pointer variable in 18 bits holds an index into a table of the addresses, which are 64 bits. `FNEW` is gone: `FALLOT` is the tape's single routine again, taking a name or an index. The tape tells them by `A RS 18` (a SIXBIT name has bits in the left half); here the indices are moved out of the way instead of the names being changed: the arena starts at the index `FIORG` (2**31; `FB`, the address of index 0, is `FIORG` words below the arena, as the tape's `FORG` is its origin), an index is from 2**31 to 2**32-1, and a name of ASCII bytes is below 2**31 (up to four characters) or above 2**32 (five or more), so `A RS 31` equal to 1 is an index and anything else a name, with no padding of names and no change of how a text constant is laid out; `CORE` refuses to grow the arena past an index of 2**32-1. The branch for a new array is the old `FNEW` in place (the tape's fake header sits in static memory and has no index here).

## The order of the routines

The routines of a module are in the order of the tape and in the module of the tape (2026-11-11, the author: "keep functions in the same files, in the same order unless there is a really good reason to change"): `code`, `code2`, `code3`, `debug`, `dosem`, `free`, `impm` and `impsem` were reordered to it, and `PCODE1` is in `debug` again (it lists the IR records where the tape's lists PDP-10 instructions; the seed's C twin keeps `g_pcode1` in `gen_code3.c`, which has the generator's variables). A routine that the port adds stands before the first routine of the tape that uses it, or after the port's routine that uses it, so that it is next to its use. `CORE` is in `free` as on the tape (the tape has another in `impm`, a monitor call, which is not provided).
- `RLEAS` is the tape's again (it executed the monitor's `RELEAS`; it calls `RELEAS` of `lib/impchan.iml`), now that channel input and output is back as a single buffer emulation: `INIT`, `LOOKUP`, `ENTER`, `INPUT`, `OUTPUT`, `CLOSE`, `RENAME`, `RELEAS` over `FILE *`, new in the library because the tape's are the monitor's own calls (TOPS-10 convention, not IMP's). The only tape routines that touch the buffer header are `NEXTCHAR` of `lex` (reads), `IASSEM` and `OUTBLK` of `omod` (writes) and `OPENIN`, `OPENOUT` and the main program of `impm` (the ring); the library of the tape (`tbin`, `tprin`, `traed`) uses the calls with dump mode lists and not the header.
