# Roadmap: planned language and interop work

Each item says what it is, the syntax proposed, and the effect on the code.
None is implemented; `docs/LANGUAGE-DIFFERENCES.md` and `docs/FFI.md` describe
what exists. Every change to code generation is made in both languages of the
generator (`src/llvm-ir/*.imp` and `stage0/gen_*.c`) and in both IR writers
(`src/ir2ll.imp`, `src/ir2llx.imp` and `tools/ir2ll.c`), and `tests/seed`
requires the seed and stage 2 to keep writing the same IR.

A call to C stays visibly different from a call to IMP, whose arguments are
by reference: `CCALL` is always written.

## C interop

The next step is the import of declarations. The typed declarations of C
functions (`EXTERNAL INT puts(PTR);`), the callbacks
(`CALLBACK INT CMP(PTR, PTR);`), the calls through a pointer
(`FNPTR INT F(PTR);`) and the structure layouts (`STRUCT`, `FIELD`) that it
builds on are described in `docs/FFI.md`.

### 1. Import of C declarations from headers

A tool, written in IMP, that reads C prototypes (`int puts(const char *);`) and
writes the `EXTERNAL` declarations of `docs/FFI.md`. For structures it probes the
layout with the C compiler of the machine and writes the `STRUCT` statements, as `tools/sysconst.sh` does for
constants, so that a program says `INCLUDE`, or the build runs the tool, and has
the declarations of a header.

## Language

### 2. UTF-8 operator symbols (`≤ ≥ ≠ ← ∧ ∨ ¬`)

`src/lex.imp` makes every byte of 128 and up a letter of names, so that a name
may be UTF-8 text; a symbol in an operator position would be read as the start
of a name.

Proposed: spellings of the existing operators, not additional syntax, defined in an
extension file loaded after `syntax64.imc` that repeats the rule of `LE` and the
others (`<ATOM> ::= '≤' ::= ...`), the way `lib/ffi.imc` extends the language.
Names keep all bytes of 128 and up except those that a loaded extension makes
into symbols.

*Effect on the code.* The lexer tests a table of symbols before it takes a high
byte for a letter (one routine in `lex.imp`, and in `stage0/lex.c`); the rest
is a file of rules. A name that used one of those characters would change
meaning.

### 3. C string syntax `"text\n"`

`'text'` is the string constant today, two apostrophes are one inside it, and
`!name!` is an exact identifier.

Proposed: `"..."` with C's escapes (`\n \t \\ \" \0 \xHH`, and `\u` as UTF-8
bytes) as a second spelling of the same string constant. `'...'` stays. A
one-character `'a'` stays a string of one character; giving it the value of a
character would change `STG 0,'A'` and every program of the original.

*Effect on the code.* The quoting code of the lexer and the rule for the
constant in `syntax64.imc`. The code generators see a constant either way.

### 4. The empty string `''`

`''` is a syntax error today. The original manual says the string `''` is the
same as 0: the empty ASCIZ string is its terminating zero, which as a value is
the number 0. A quote inside a string stays written doubled.

Proposed: `''` is the constant 0.

*Effect on the code.* One case in `lex.imp` and `stage0/lex.c` (two apostrophes
with no third following: the constant 0) and a test. As a `STG` argument, `''`
is 0, like any other zero word.

### 5. File names with Unix limits

`MKNAME` (`src/impm.imp`) builds a file name from two words (15 characters)
and an extension of one word (7); the original had six and three. The library
cuts the names of `FILE` items in `PRINT` and `READ` to 7 characters
(`lib/imp72io.imp`, label `CM6`).

Proposed: keep the form `FILE NAME.EXT`, but let a name be as long as the
string given (up to `NAME_MAX`), since the library takes it from the string. The
compiler's own `MKNAME` is for names on its command line, which a shell has
already limited: raise it to `PATH_MAX`.

*Effect on the code.* `MKNAME` and its buffer `NMB`; the `%.7s` and the 64-byte
file items of the library take the length of the buffer. A test with a name of
200 characters.

### 6. The original's channel I/O

`PRINT`, `READ` and `PRINT FILE` are in `lib/imp72io.imp`. The channel
operations of the TOPS-10 library (`INIT`, `LOOKUP`, `ENTER`, `INPUT`,
`OUTPUT`, the buffer header) are not in `decus/syntax.imc` or `decus/impsyn.imc`;
the original reaches them through its libraries, which the port does not have.

Proposed: statements with the original's meaning over `fopen`, `getc` and
`putc`. A channel is an index into a table of `FILE *`; the buffer header is a
small record that the program may read (count, pointer), kept in step by the
library. Programs of the original that call them then compile. `RLEAS` is the
`fclose` of a channel.

*Effect on the code.* An added part of the library, about the size of the file part
of `imp72io.imp`, and the statements in an extension file. How many routines of
`decus/` use the buffer header words directly decides whether the buffer
emulation is cheap; it has not been counted.

### 7. Counted strings

String constants are ASCIZ, as in the original: the bytes, then a zero. They
pass to C unchanged, and `STG` and the library scan to the zero. This stays.

Proposed: a counted string as a separate object type beside it, with its own
handler functions. It is a record: a length word (bytes, the NUL not counted),
the bytes, and a zero byte after them, so that the address of its bytes can be
given to C. It is made and handled only by routines of its own: create from a
constant or a C string, length, copy, concatenate, compare, slice, search,
append, print. A routine that takes a counted string and one that takes an
ASCIZ string are different routines, which the typed declarations of `docs/FFI.md`
tell apart.

*Effect on the code.* An added part of the library and its declarations. The
lexer, the code generators and the existing library are untouched, and no
existing program changes. It depends on item 3.

## Open question: tape routines

`docs/porting/tape-names.md` compares the routine names of the tape with the
port's. Two of the tape's routines are not present: `CORE(N)`, a monitor call
to grow the core (the arena is reserved once and `free.imp` grows its usable
part, so `CORE` could return as "make N words usable"), and `RLEAS(CH)`,
which released a channel. Whether either is restored is to be examined
together with item 6, since `RLEAS` is the `fclose` of a channel and `CORE`
may depend on it.
