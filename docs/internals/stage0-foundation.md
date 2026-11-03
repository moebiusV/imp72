# The seed, part 1: free storage, stack, directory, lexer

The seed in `stage0/` is the compiler's kernel in C99. This page covers `free.c`, `stack.c`, `dir.c` and `lex.c`, which
correspond to `src/free.imp`, `src/stack.imp`, `src/dir.imp` and `src/lex.imp` (and to `decus/free.imc`, `stack.imc`,
`dir.imc`, `lex.imc` of the tape). The parser is in `stage0-parser.md`, the semantic machinery in `stage0-semantics.md`.
`make -C stage0 check` runs the unit tests in `tests/stage0/` (`test_free`, `test_stack`, `test_dir`, `test_lex`,
`test_parse`, `test_debug`) and `tests/stage0/test_syntax.sh`.

## Reading the original

IMP has no operator precedence: `A+B*C` is `A+(B*C)`, and so is every chain of binary operators. `FOR I TO N` includes
`N`. Parameters are passed by reference, so the C functions that change an argument (`fadd`, `faddex`, `frelot`) take
the address of the variable. `docs/LANGUAGE-NOTES.md` lists more traps.

## Word model

`word` is `int64_t`. A half word is 32 bits (`HL`, `HR`, `HW` in `stage0/imp.h`). A pointer into free storage is an index
into one array, `fmem`, as in the original; index 0 means none. `fb` holds the address of `fmem` as a word, so that
the IMP modules and the C ones share it.

## free.c

Free storage is a doubly linked list of arrays, each with a four word header: back and forward links (halves), the
name (up to eight characters, the first in the top byte), the increment (left half) and the length (right half), and a
word that is 0 for a fixed array, -1 while the array is being packed, and otherwise 1 plus a slot in the registry of
relocatable arrays. An array is named by the index of its first data word. The list runs from a dummy header at
`forgn` (index 8) to one at `fend`.

A relocatable array (`frelot`) has a registry slot holding the address of the C variable that points to it and an
optional routine to call after it moves; every move updates the variable (`ffixer`). `fpack` slides relocatable
arrays down over gaps and fills a gap left by a fixed array with the largest relocatable array that fits.
`fallot(a,n)` tries, in order: grow in place; move into the gap before the array; move to the smallest gap that fits
(`fgap`); grow core (`ftop`); pack and try again; otherwise "FREE STORAGE EXHAUSTED".

- `fmem` is a block grown with `realloc`, in multiples of 1024 words.
- A new array is `fallot_new(name,n)`; `fallot(a,n)` resizes or, for `n <= 0`, frees.
- The first dummy header is at index 8 because `fgap` returns 0 for "none".
- `fgap` has no ceiling on the size of a gap.
- `FREE` and `FREES` are functions (`static inline`), not macros: free storage moves during a call that allocates, and
  a macro would store through the `fmem` loaded before the call.
- Trace output (`ftrace`, `fmap`, `fstats`) goes to standard error. `fcheck` walks the headers and reports a bad link or
  registry slot; it is a debugging aid for the IMP modules, which write into free storage themselves
  (`IMP_FCHECK`, `IMP_FCHECK_ABORT`).

## stack.c

The semantic stack is `STACK`, three words an entry; `stack(n)`, `stpush`, `stpop`, `setstk` and `stinit` return free
storage indices. `pushmi` and `pullyu` are a pushdown stack (`PSHMI`).

A bit matrix is a three word descriptor `a[3]` (free index, rows, words per row less one) and one relocatable array;
bit `(i,j)` is bit `j%64` of word `a[0] + i + a[1]*(j/64)`. `bit` returns 0 outside the matrix. The `BIT` that the
compiler uses (`impsyn64.imc`) does not check its indices and reads whatever lies beyond the matrix.

## dir.c

A name is packed eight characters (bytes) to a word, the first in the low byte, ended by a NUL that takes a word of its
own when the length is a multiple of eight. Free array `PRP` is 512 buckets of three words followed by overflow nodes;
free array `ALF` holds the names one after another. `hash` folds the first word of the name to 9 bits
(`h ^ h>>32`, `^ >>16`, `^ >>9`, `& 0777`); a collision tries two alternate buckets (`h` xor 2, then xor 1) and then the
chain of overflow nodes. A directory index is the index of the name's node in `PRP`.

    node word 0   left: next node of the chain   right: index of the name in ALF
    node word 1   left: ENTRY                    right: NODE
    node word 2   SEM bits 0-31, ATYPE 32-34, COM 35-36, ISNM 37-38, LXTY 39-40, USE 41-43

- `dprop(p,i)` and `dprops(p,i,v)` read and write the properties; they are the `DPROP`/`DPROPS` forms of
  `impsyn64.imc`. `DP_NAME` is read only.
- `dname(i,j)` is the jth word of a name; `gname(i)` is the first 24 characters as a C string; `dname_str` copies the
  whole name; `dseq` steps through the entries; `dspill` prints the directory; `radi50` is the Radix-50 value of
  the first six characters.
- `convc(c)` returns 0 for a name that is not a constant, -1 for a number (the value in `convc_const`), or the index of
  a free array for a string constant: word 0 is the count of words and the following words hold the characters, eight to
  a word in memory order, without the opening quote. Numbers are decimal, `dddB` octal, `dddBr` radix `r`; the value is
  a 64 bit number that wraps.
- `stcon(v)` names a constant by the octal digits of its 64 bits followed by `B`. `newname(s)` makes `%` plus the first
  four characters of `s` plus a serial number.

## lex.c

`lexin` reads the whole file; `lex(&s)` sets `s` to the directory index of the next token and returns the column
reached. A token is a run of letters, digits and `_`; an apostrophe string (the token keeps its opening apostrophe and
loses the closing one, `''` is one apostrophe); the text between two exclamation marks (the marks are dropped); or one
other non-blank character. `:=` is one token and `::=` is not. Blanks and control characters separate tokens and
`#...#` is a comment. Letters are ASCII only here; `src/lex.imp` also takes bytes of 128 and up as letters.

The line and the token are C buffers where the original keeps them in free arrays (`LEXL`, `LEXT`). The routines of
the TOPS-10 command line (`GETTY`, `READCMD`, `PUTN`, `PUTO`, `PUTSW`, `LEXSAV`, `LEXRES`) are not in `lex.c`; the
driver reads `argv`.
