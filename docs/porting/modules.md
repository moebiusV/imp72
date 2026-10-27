# The modules and the tape

`src/` is the tape's source (`decus/`, kept as it was) translated to IMP for a 64-bit machine, a module at a time. The routines
keep their names and their text where the machine does not force a change. What changed, everywhere: words are 64 bits,
halves are 32 (the shift by the constant 18 of the tape is a halfword move to the translator, so it is written as 32);
the layout of the tables is `layouts.md`; a name is eight characters to a word; `FREE`, `FREES` and the rest are macros of
`src/impsyn64.imc` over an arena; reals are IEEE 754; `CALLI` is the library's `imp_calli`.

| module | what differs from the tape |
|---|---|
| `free` | the arena is a `realloc`ed block, not the low segment grown with `CORE`; `BLT` is `memmove`; arrays are named by index, so no routine is called after one moves (the tape's `S` of `FRELOT` is unused); a new named array is `FNEW` (`FALLOT` of a name could not be told from `FALLOT` of an array by size); `FMAP` and `FSTATS` print with `PRINT` |
| `dir` | `DIR` takes a name or string constant as it is; `DIRW` makes one from a token held a character to a word; `STCON` names a constant by the octal digits of its 64 bits; `RADI50`, `CONVC` and `NEWNAME` work on bytes |
| `stack` | bit matrices have 64 columns to a word |
| `lex` | the line and the token are arrays with a character in each word (the tape steps byte pointers over packed text); `LEXIN` opens the file with `fopen`; `PLINE` prints with `PRINT` |
| `graph`, `parse`, `ercor` | halves of 32 bits; the parser takes its goal as a name; `PSTATS` is IMP |
| `entree`, `cotree`, `dosem`, `rsyn` | the layouts of `layouts.md`; the semantic routines register themselves with `DSEM`, which enters their addresses in a table in C (`stage0/semtab.c`) where the tape patches the address into a `JSA`; |
| `debug` | the print-outs use `PRINT`; the dumps show the layouts of this machine |
| `impsem`, `code`, `code2`, `code3` | they make chains of records of the pcc IR machine instead of PDP-10 words (`pccir-path.md`); `EXECUTE` runs one instruction of that machine; `HRL` and `HRR` are the operations 66 and 67 |
| `demit` | takes the place of `amod`, `rmod` and `omod`: chains to pcc IR through `stage0/ir.c`; a `GO TO` to a tag of another routine is a `longjmp` |
| `impm` | the driver program: the command line instead of the terminal, `ERROR`, `MSG` and `OUTSTR` writing to standard error; a fatal error returns to the driver through `impfatal` where the tape goes to `FIN` |

Still C: `stage0/ir.c` (the builder of pcc IR), `stage0/sys.c` (arguments, the listing, the terminal), `stage0/semtab.c`
(the table of semantic routines), `stage0/pccirsem.c` (the routines `CCALL` reaches, `FCON`, `LETSYN`, `CVAL`, the `/A`
listing).
