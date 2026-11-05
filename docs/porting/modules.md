# The modules and the tape

`src/` is the tape's source (`decus/`, which is kept as it is) translated to IMP for a 64 bit machine, a module at a time.
The routines keep their names and their text where the machine does not force a change. What differs everywhere: words are
64 bits and halves are 32 (a shift by the constant 18 on the tape is a half word move to its translator, so it is written
as 32); the layout of the tables is `layouts.md`; a name is eight characters to a word; `FREE`, `FREES` and the rest are
macros of `src/impsyn64.imc` over an arena; reals are IEEE 754; `CALLI` is the library's `impcalli`. The routines that the tape lacks
are in `tape-names.md`.

| module | what differs from the tape |
|---|---|
| `free` | the arena is reserved address space made usable with `CORE` (`lib/imp72sys.imp`), and it does not move while the compiler runs; `BLT` is `memmove`; arrays are named by index, so the tape's `S` of `FRELOT` (a routine to call after a move) is not used; a named array is allocated with `FNEW`, because `FALLOT` of a name cannot be told from `FALLOT` of an array by size; `FMAP` and `FSTATS` print with `PRINT` |
| `dir` | `DIR` takes a name or string constant as it is; `STCON` names a constant by the octal digits of its 64 bits; `RADI50`, `CONVC` and `NEWNAME` work on bytes with the macros of `impend-*.imc`; the table of semantic routines is here: `DSEM` enters a routine by name and `SEMCALL` calls it |
| `stack` | bit matrices have 64 columns to a word |
| `lex` | the line and the token are packed eight characters to a word and stepped with byte pointers, as on the tape; `LEXIN` opens the file with `fopen`; `PLINE` prints with `PRINT` |
| `graph`, `parse`, `ercor` | halves of 32 bits; the parser takes its goal as a name |
| `entree`, `cotree`, `dosem`, `rsyn` | the layouts of `layouts.md`; the semantic routines register themselves with `DSEM`, which enters their addresses in a table (the tape patches the address into a `JSA`) |
| `debug` | the print-outs use `PRINT`; the dumps show the layouts of this machine; `PCODE1` is in `code3.imp` |
| `impsem`, `code`, `code2`, `code3` | they make chains of records of the IR machine instead of PDP-10 words (`llvm-ir-path.md`); `EXECUTE` runs one instruction of that machine; `HRL` and `HRR` are the operations 66 and 67 |
| `demit` | takes the place of `amod`, `rmod` and `omod`: chains to IR; a `GO TO` to a tag of another routine is a `longjmp` |
| `impm` | the driver program: the command line (`IMPK`) as well as the tape's commands from the terminal (`-i`: `GETTY`, `READCMD`, the loop `AGAINSAM`); `ERROR`, `MSG` and `OUTSTR` write to standard error; a fatal error returns from `IMPK` where the tape goes to `FIN` |

`PCODE` and `PCODE1` (the `/A` listing) are in `src/llvm-ir/code3.imp` and list the records of the IR chain, not PDP-10
instructions. `HELPD`, `GPRINT`, `TPRINT` and `PARTRA` call the debugging routines directly (the tape tests whether the
debugging module is loaded; it always is here). `FORMARG` is in `src/rsyn.imp`; nothing calls it, on the tape either.

## Byte pointers

`BYTEP A<7,36>` (the tape's pointer to the characters of A; `A<8,64>` also) is the byte address of the byte before the
first of A, `<+P>` adds 1 and then reads or writes the byte there, and a character is a byte (UTF-8, never masked to 7
bits). Any other `BYTEP A<S,P>` names a bit field and keeps the layout of the PDP-10's pointer in one word: size in bits
63-58, position in bits 57-52, the byte address of the word in the low 52 bits. The two are told apart by the size
bits, which a byte address (below 2**47) never has. `lib/imp72rt.imp` has `impibp`, `impldb` and `impdpb`, which both
compilers call.

## The lexer

The line and the token are packed, a character to a byte, and stepped with byte pointers (`<+PC>`, `<+K>`), and the token is
passed to `DIR` as it is. What differs from the tape:

- `LEXIN(NAME)` opens the file with `fopen` and `NEXTCHAR` reads it with `IMPGETC`, where the tape reads the buffers of a
  channel.
- The arrays hold eight characters to a word, so the growth is by `(17+L)/8` words where the tape's is `(10+L)/5`.
- A quote or parenthesis is written `CODE('(')` where the tape has `'(' RS 29`.
- `ISTAB` is a table of 32 characters to a word (the tape's has 36 bits to the word and 128 characters) and takes the bytes
  of 128 and up (UTF-8) as letters, and `_` as a letter.
- `C` is an ordinary variable of the module, not a reserved register.
- `LEXSAV`, `LEXRES`, `GETTY`, `READCMD`, `PUTN`, `PUTO` and `PUTSW` are the tape's, for the commands of the interactive mode.

## The arena

The tape's low segment grows in place, so a byte pointer into an array stays good until the array itself moves. The arena
is the same: `IMPCOREBASE` (`lib/imp72sys.imp`) reserves the address space once, without access, at the address `SCHINT`
(or `IMP72COREBASE`) if the system gives it and elsewhere if not, and `CORE` makes more of it usable with `mprotect`. The seed's block is a `realloc`ed
heap block instead (`docs/internals/stage0-foundation.md`).

## The interactive mode

`imp72d -i SYNTAX [FILE ...]` and `imp72 -i` run the tape's driver. The syntax and any files named are read first; then
the prompt `*` and a command `REL,LST_FILE.EXT[PJ,PG]/A/B...(AB...)PGM\`, as the tape's `READCMD` takes it. As on the
tape one compilation ends the session unless the switch `C` is given, and a fatal error ends it. What differs:

- The file name is 15 characters and the extension 7 (the tape's are 6 and 3). `FILE` is tried as given and then with
  `.imp` and `.imc`.
- The output `REL` is the IR (`REL.ir`) and `LST` a listing (`LST.lst`, written when `LST` is given).
- `\PGM` runs `PGM` with the shell and returns to the prompt, where the tape chains to it.
- The line editing of the terminal is the system's, so `GETTY` ends a line at a newline and has no echo of its own. A line
  is at most 136 characters (the tape's `BF` is 17 words of 5 characters; here `BF` is 17 words of 8, and one word more
  for the `%` and the end).
- The banner has the date, from `DATE` over byte pointers (`CALLI(14B)` gives the date as the monitor does).
- `U` and `V` save the compiler with the syntax and declarations it has compiled (below). `INCHWL` and `RUNPGM` are
  written over `getchar` and `system`.

## The image (`U`, `V`, `-I`)

On TOPS-10 `/U` exits to the monitor, whose `SSAVE` writes the core image of the compiler; the next run starts with the
language it had compiled. Here `NAME_/U` (or `/V`, the same: the image has no code, so there is no high segment to keep)
writes `NAME.img` and ends the session, and `imp72d -I NAME.img [FILE ...]` (or `imp72 -I NAME.img FILE.imp`) starts
from it, with no syntax file.

The image is the state of the language, not of the process: the free storage (the directory, the syntax graph, the code
tree, the semantics array, all the compiler's tables), the variables of the compiler's modules, the names of the
semantic routines in the order of their ids, and the registry of relocatable arrays. The modules are compiled with
`IMP72MODULE` set (`tools/direct.sh`), and each variable leaves a record (address, size, `module:name`) in the section
`impvars` (`IRVARREC`); `src/image.imp` writes and reads them, and `lib/impvars.imp` finds the section
(`lib/impvarsx-elf.imp`, `lib/impvarsx-macho.imp`). An image is for the build that made it and says so if it is not (a
hash of the list). The arena is restored at any address (format version 6): everything in it is an index from `FB`, and the load adds the
difference of the bases to the five byte pointers of the lexer that point into it (`docs/internals/modules.md`). `IMGSKIP` in `src/image.imp` lists what the image
leaves out: the variables of `impm`, `ix`, `image`, `driver`, `llvm` and `llvmgen`, of the IR writer (`ir2ll`, `ir2llx`),
the table of semantic routines (written by name), the file the lexer has open, and the registry itself. A run from an image
does what the head of `RSYN` and the first `COTREE` do (`RSYNI`), so that the routines they enter are in the table; the
table is put in the image's order by name, and a name that this run has not entered is an error.

## Byte order

The compiler builds and compiles itself on little and big endian hosts. A word is native, so `LOC` of an array of integers
means the same to C everywhere. A name, and a text constant, is a string in the bytes of a word, the first character at the
first address: the low byte on a little-endian host, the high byte on a big-endian one. The modules say what they mean
with macros (`CHR0`, `CHR`, `REST`, `PREP`, `LASTC`, `FRONTLAST`, `PFX4`, `CHAT`, `CODE`, `CHFIRST`, `CHSTEP`, `CHEND`,
`CHBEYOND`, `CHPAST`) that `src/impend-le.imc` and `src/impend-be.imc` define, and `tools/endian.sh` names the file for
the host the compiler is built on (`tools/direct.sh` puts it in front of every module). `CODE('(')` is the character code
of a text constant of one character. `IRSTRVAL` (`ix.imp`, `g_irstrval` in the seed) and `CONVC` make a text constant's
word in memory order of the target. The macros describe the machine the compiler runs on; the
byte order of the program being compiled is separate: `LLVMTARGET` (`src/llvm.imp`) sets `TGTSWAP` to 1 from the data layout of
the target machine when it is not the host's, and `CONVC` (`dir.c`: `memorder`), `IRSTRVAL` and the data writer of `ir2ll`
(the pair `{ i32, i32 }` of a 32-bit address) read it; the seed compiles for its host, so its flag is 0. A byte pointer needs no macro. An image is for the byte order and the build that made it.

## The back end

The code generator writes IR (lowered for the target, `amd64` by default) whatever the host, and `src/ir2ll.imp` (the
seed: `tools/ir2ll.c`) turns it into LLVM IR; LLVM is the only back end. The compiler links `libLLVM` and makes the
object in the process, with no clang (`src/llvm.imp`; the seed: `stage0/llvmorc.c`). `imp72 --run` compiles a program with
ORC and calls its `impmain`. The records that name the compiler's own variables for the image are the section `impvars`
(`IRVARREC`), written as data.
