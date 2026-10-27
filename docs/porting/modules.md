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

## Departures from the tape

Every routine the port added or dropped, and the one thing the tape had that differs, with the reason. A departure is
either **forced** (the 64-bit word, the IR machine, a Unix instead of a TOPS-10 service) or **provisional** (done for
convenience; the tape's way is to be restored). Nothing is renamed: where a tape name appears, it means what it meant.

**Added**

| routine | module | why | |
|---|---|---|---|
| `FNEW` | `free` | `FALLOT(A,N)` tells a name from an array by `A RS 18`; an eight-character name in a 64-bit word cannot be told from an index. `FALLOT` and `FRELOT` keep the tape's meaning and call it | forced |
| `FPLACE`, `FRELEASE`, `FCOUNT`, `FALOTN` | `free` | pieces of the tape's `FALLOT` and `FFIXER` that are routines here (the tape has them in line, with `GO TO` into the middle) | provisional: inline them as the tape has them |
| `FBACK`, `FFWD`, `FLENX`, `FSETB`, `FSETF`, `FSETL` | `free` | the tape's `[X]<L>`, `[X]<R>`, `[X+2]<R>` and assignments to them; the IR machine has no halfword operations | forced |
| `DIRW` | `dir` | the lexer keeps a token a character to a word; `DIR` takes eight characters to a word | provisional, with the lexer below |
| `PUTC` | `dir` | `NEWNAME` and `STCON` stepped a byte pointer (`<+P>`) over the packed name | provisional: use the byte pointer once it is a byte address |
| `LEXRES`'s state | `lex` | see the lexer below | |
| `IMPK`, `USAGE`, `SETSW` | `impm` | the tape's driver reads commands from the terminal; this one takes the command line | forced (Unix) |
| `IINIT`, `PAIRFIRST` | `pccir/impsem` | `IINIT` is the initialisation that the tape does where the routines are defined (the 64-bit module sets its tables when first called); `PAIRFIRST` tests whether the register of a pair comes first, which `HISREG` did by looking at the code | forced: no register pairs |
| `REGFIELD` | `pccir/code` | the register field of an entry word, written in the tape as a byte-pointer field | forced |
| `ADDREC`, `SETNEXT`, `JOP`, `CJUMP`, `MOVEBLK` | `pccir/code2` | `ADDCODE` of the tape adds a PDP-10 instruction word; `ADDREC` adds a record of the IR machine. The others are the tape's jump and block-move code with the machine's operations | forced |
| `demit.imp` | | replaces `amod`, `rmod` and `omod` (the PDP-10 assembler): the chains become pcc IR | forced |

**Dropped**

| routine | why |
|---|---|
| `DORGFIX`, `GORGFIX`, `TORGFIX` | they re-aimed byte pointers into `PRP`, `GA` and `CT` after the low segment moved; fields here are macros over an index |
| `GETTY`, `READCMD`, `LEXSAV` and the terminal handling in `impm`: `DATE`, `TIME`, `TMPCOR`, `GETMP`, `INCHWL`, `OPENIN`, `OPENOUT`, `PEEKAT`, `RLEAS`, `RUNPGM`, `TMPFL`, `PUTN`, `PUTO`, `PUTSW` | the TOPS-10 command loop and its files, replaced by the command line and `fopen` (Unix) |
| `OCHK`, `OPCODE`, `RCOCHK` | the PDP-10 assembler and the search of the code for register use; pcc allocates the registers |
| `SUBTAG`, `PARAMETERS` | nothing on the tape calls them |

**Not yet translated (still C)**: `ERRSRT`, `LETSYN`, `CVAL`, `FCON`, `PCODE`, `PCODE1`, `HELPD`, `PARTRA`, `GPRINT`, `TPRINT`, `FORMARG`.

**The byte pointer is a byte address.** `BYTEP A<7,36>` (the tape's pointer to the characters of A; `A<8,64>` also) is the byte
address of the byte before the first of A, `<+P>` adds 1 and then reads or writes the byte there, and a character is a byte
(UTF-8, never masked to 7 bits). Any other `BYTEP A<S,P>` names a bit field and keeps the layout of the PDP-10's pointer in one
word (size, position, word address), because no byte address can name it; the two are told apart by the size bits, which a byte
address (below 2**47) never has. `lib/imp72rt.c` has `imp_ibp`, `imp_ldb` and `imp_dpb`, which both compilers call.

**Provisional: the lexer.** The tape's lexer keeps the line and the token packed (`<+PC>`, `<+K>`) and passes the packed
token to `DIR`. Here they are one character to a word, and `DIRW` packs the name. With the byte pointer a byte address the
tape's way is available (a token is `FREE(TOKE)` read byte by byte, and `DIR` takes it as it is); the lexer, `DIRW` and `PUTC`
are to go back to it, and the class table `ISTAB` (128 entries on the tape) must take the bytes of UTF-8.
