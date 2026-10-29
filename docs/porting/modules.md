# The modules and the tape

`src/` is the tape's source (`decus/`, kept as it was) translated to IMP for a 64-bit machine, a module at a time. The routines
keep their names and their text where the machine does not force a change. What changed, everywhere: words are 64 bits,
halves are 32 (the shift by the constant 18 of the tape is a halfword move to the translator, so it is written as 32);
the layout of the tables is `layouts.md`; a name is eight characters to a word; `FREE`, `FREES` and the rest are macros of
`src/impsyn64.imc` over an arena; reals are IEEE 754; `CALLI` is the library's `imp_calli`.

| module | what differs from the tape |
|---|---|
| `free` | the arena is a `realloc`ed block, not the low segment grown with `CORE`; `BLT` is `memmove`; arrays are named by index, so no routine is called after one moves (the tape's `S` of `FRELOT` is unused); a new named array is `FNEW` (`FALLOT` of a name could not be told from `FALLOT` of an array by size); `FMAP` and `FSTATS` print with `PRINT` |
| `dir` | `DIR` takes a name or string constant as it is; `STCON` names a constant by the octal digits of its 64 bits; `RADI50`, `CONVC` and `NEWNAME` work on bytes with byte pointers, as the tape's do on 7-bit characters |
| `stack` | bit matrices have 64 columns to a word |
| `lex` | the line and the token are arrays with a character in each word (the tape steps byte pointers over packed text); `LEXIN` opens the file with `fopen`; `PLINE` prints with `PRINT` |
| `graph`, `parse`, `ercor` | halves of 32 bits; the parser takes its goal as a name; `PSTATS` is IMP |
| `entree`, `cotree`, `dosem`, `rsyn` | the layouts of `layouts.md`; the semantic routines register themselves with `DSEM`, which enters their addresses in a table in C (`stage0/semtab.c`) where the tape patches the address into a `JSA`; |
| `debug` | the print-outs use `PRINT`; the dumps show the layouts of this machine |
| `impsem`, `code`, `code2`, `code3` | they make chains of records of the IR machine instead of PDP-10 words (`pccir-path.md`); `EXECUTE` runs one instruction of that machine; `HRL` and `HRR` are the operations 66 and 67 |
| `demit` | takes the place of `amod`, `rmod` and `omod`: chains to IR through `stage0/ir.c`; a `GO TO` to a tag of another routine is a `longjmp` |
| `impm` | the driver program: the command line (`IMPK`) as well as the tape's commands from the terminal (`-i`: `GETTY`, `READCMD`, the loop `AGAINSAM`); `ERROR`, `MSG` and `OUTSTR` writing to standard error; a fatal error returns to the driver through `impfatal` where the tape goes to `FIN` |

Still C: `stage0/ir.c` (the builder of IR), `stage0/sys.c` (arguments, the listing, the terminal), `stage0/semtab.c`
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
| `FBACK`, `FFWD`, `FLENX`, `FSETB`, `FSETF`, `FSETL` | `free` | the tape's `[X]<L>`, `[X]<R>`, `[X+2]<R>` and assignments to them; the IR machine has no halfword operations | forced |
| `IMPK`, `USAGE`, `SETSW` | `impm` | the command line: the files are named as arguments and the switches are `-s LETTERS`; the tape reads one command from the terminal. Both are kept: `IMPK` is the one-shot mode and `-i` is the tape's | forced (Unix) |
| `COMPIL`, `MKNAME` | `impm` | the compile that the tape has inline in the main program, which the two modes share; `MKNAME` builds a file name string from the name and extension words (`OPENIN` of the tape does it in its library) | provisional: inline `COMPIL` |
| `IINIT`, `PAIRFIRST` | `pccir/impsem` | `IINIT` is the initialisation that the tape does where the routines are defined (the 64-bit module sets its tables when first called); `PAIRFIRST` tests whether the register of a pair comes first, which `HISREG` did by looking at the code | forced: no register pairs |
| `REGFIELD` | `pccir/code` | the register field of an entry word, written in the tape as a byte-pointer field | forced |
| `ADDREC`, `SETNEXT`, `JOP`, `CJUMP`, `MOVEBLK` | `pccir/code2` | `ADDCODE` of the tape adds a PDP-10 instruction word; `ADDREC` adds a record of the IR machine. The others are the tape's jump and block-move code with the machine's operations | forced |
| `demit.imp` | | replaces `amod`, `rmod` and `omod` (the PDP-10 assembler): the chains become IR | forced |

**Dropped**

| routine | why |
|---|---|
| `DORGFIX`, `GORGFIX`, `TORGFIX` | they re-aimed byte pointers into `PRP`, `GA` and `CT` after the low segment moved; fields here are macros over an index |
| `TMPCOR`, `GETMP`, `OPENOUT`, `PEEKAT`, `RLEAS`, `TMPFL` | the TOPS-10 core files and chaining of the command loop (a program leaves its command line in core for the next one), and the files of its log; Unix has no counterpart |
| `OCHK`, `OPCODE`, `RCOCHK` | the PDP-10 assembler and the search of the code for register use; LLVM allocates the registers |
| `SUBTAG`, `PARAMETERS` | nothing on the tape calls them |

**Still C**: nothing of the tape's routines is only in C in the compiler `imp72` (the seed `imp72stage0` is the C kernel, `stage0/`, by design). `PCODE` and `PCODE1` (the `/A` listing) are in `src/pccir/code3.imp` and list the records of the IR chain, not PDP-10 instructions; `HELPD`, `GPRINT`, `TPRINT` and `PARTRA` call the tape's debugging routines directly (the tape tests whether the debugging module is loaded; it always is here); `FORMARG` is in `src/rsyn.imp` (nothing calls it, on the tape either).

**The byte pointer is a byte address.** `BYTEP A<7,36>` (the tape's pointer to the characters of A; `A<8,64>` also) is the byte
address of the byte before the first of A, `<+P>` adds 1 and then reads or writes the byte there, and a character is a byte
(UTF-8, never masked to 7 bits). Any other `BYTEP A<S,P>` names a bit field and keeps the layout of the PDP-10's pointer in one
word (size, position, word address), because no byte address can name it; the two are told apart by the size bits, which a byte
address (below 2**47) never has. `lib/imp72rt.imp` has `impibp`, `impldb` and `impdpb`, which both compilers call.

**The lexer is the tape's.** The line and the token are packed, a character to a byte, and stepped with byte pointers (`<+PC>`, `<+K>`),
and the token is passed to `DIR` as it is. What differs, and why: `LEXIN(NAME)` opens the file with `fopen` and `NEXTCHAR` reads it with
`getc` where the tape reads the buffers of a channel (Unix); the arrays hold eight characters to a word, so the growth is by `(17+L)/8`
words where the tape's is `(10+L)/5`; a quote or parenthesis is written `'('` where the tape has `'(' RS 29` (its string constant is
left justified in a 36-bit word); `ISTAB` is a table of 32 characters to a word (the tape's has 36 bits to the word and 128 characters;
the seed's literals wrap at 36 bits, so a word of 64 characters cannot be written in it yet) and takes the bytes of UTF-8 as letters;
`C` is an ordinary variable of the module, not a reserved register. `LEXSAV`, `LEXRES`, `GETTY`, `READCMD`, `PUTN`, `PUTO` and `PUTSW` are the tape's, for the commands of the interactive mode.

**The arena does not move.** The tape's low segment grows in place, so a byte pointer into an array stays good. `CORE` reserves the
address space once (`stage0/sys.c`) and makes more of it usable; `realloc` would move the arena and every pointer with it.

**The interactive mode** (`imp72d -i SYNTAX [FILE ...]`, the tape's driver). The syntax and any files named are read first; then
the prompt `*` and a command `REL,LST_FILE.EXT[PJ,PG]/A/B...(AB...)PGM\`, as the tape's `READCMD` takes it. As on the tape one
compilation ends the session unless the switch `C` is given, and a fatal error ends it. What differs: the file name is 15
characters and the extension 7 (the tape's are 6 and 3); `FILE` is tried as given and then with `.imp` and `.imc`; the output
`REL` is the IR (`REL.ir`) and `LST` a listing (`LST.lst`, written when `LST` is given); the device and the project and
programmer number are read and ignored; `\PGM` runs `PGM` with the shell and returns to the prompt where the tape chains to
it; the line editing of the terminal is the system's, so `GETTY` ends a line at a newline and has no echo of its own; a line is
at most 136 characters (the tape's `BF` is 17 words, of 5 characters there and 8 here, and had no check; `BF` has a word more for the `%` and the end). The banner has the date, from
`DATE` of the tape written over byte pointers (`CALLI(14B)` gives the date as the monitor does). `R` is accepted and is always so:
the code is read-only in the text segment and the data are apart, which is what the tape's two segments are for. `U` and `V` save the
compiler with the syntax and declarations it has compiled (see the image below). `imp72 -i` runs the same session from the installed driver. `INCHWL` and `RUNPGM` are routines of the tape's
library, written here over `getchar` and `system`.

**The image (`U`, `V`, `-I`).** On TOPS-10 `/U` exits to the monitor, whose `SSAVE` writes the core image of the compiler; the next run
starts with the language that was compiled. Here `NAME_/U` (or `/V`, the same: the image has no code, so there is no high segment to
keep) writes `NAME.img` and ends the session, and `imp72d -I NAME.img [FILE ...]` (or `imp72 -I NAME.img FILE.imp`) starts from it,
with no syntax file. The image is the state of the language, not of the process: the free storage (the directory, the syntax graph, the
code tree, the semantics array, all the compiler's tables), the variables of the compiler's modules, the names called with values, and the
order of the semantic routines. The modules are compiled with `IMP72MODULE` set (`tools/direct.sh`), and each variable leaves a record
(address, size, `module:name`) in the section `impvars` (`ir_varrec`); `stage0/image.c` writes and reads those, so an image is for the build
that made it and says so if it is not (a hash of the list). The arena is always at the same address (`stage0/sys.c`), so the pointers into it
that the lexer keeps stay good. Not in the image: the driver module (`impm`), the file the lexer has open, and the registry of the arrays
that move, which is made again from the names of the variables. Started from an image, a run does what the head of `RSYN` and the first
`COTREE` do (`RSYNI`), so that the routines they enter are in the table; the table is put in the image's order by name, and a name that this
run has not entered is an error. ELF only (the section and `dl_iterate_phdr`).

**Byte order.** The compiler builds and compiles itself on little- and big-endian hosts. A word is native, so `LOC` of an array of
integers means the same to C everywhere. A name, and a text constant, is a string in the bytes of a word, the first character at
the first address: the low byte on a little-endian host, the high byte on a big-endian one. The modules say what they mean with
macros (`CHR0`, `CHR`, `REST`, `PREP`, `LASTC`, `FRONTLAST`, `PFX4`, `CHAT`, `CODE`, `CHFIRST`, `CHSTEP`, `CHEND`, `CHBEYOND`,
`CHPAST`) that `src/impend-le.imc` and `src/impend-be.imc` define, and `tools/endian.sh` names the file for the host the compiler is
built on (`tools/direct.sh` puts it in front of every module). `CODE('(')` is the character code of a text constant of one character
(the tape's `'(' RS 29`). The seed's `strvalue` (`stage0/trans.c`) and `CONVC` of `dir.imp` make a text constant's word in memory
order. Where the IMP source gave a character code to a name (`DIR(CHAT(<+K>,0))` in `READCMD`) or took the first character out of
a name word (`CHR0(DNAME(N,0))`), it says so with those macros; a byte pointer needs none. An image (`U`, `V`) is for the byte
order and the build that made it.

**Back ends.** The code generator writes IR (lowered for amd64: the arguments of a call in rdi, rsi, ..., the result in rax)
whatever the host, and `tools/ir2ll.c` reads it (LLVM IR; LLVM is the only back end). The compiler links `libLLVM` and does the `llvm` back end in the process, with no clang (`llvm_object`, `llvm_run` in
`stage0/llvmorc.c`, `ir2ll_run`): `imp72 --run` compiles a program with ORC and calls its `impmain`. The records that name the compiler's own variables for the image are a section `impvars` (`ir_varrec`), written as data by the LLVM back end.
