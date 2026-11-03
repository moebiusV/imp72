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

The order is: typed declarations, callbacks, structure layouts, import of
declarations. Each is a step of its own, with its tests.

### 1. Typed declarations of C functions

Today every C function is taken as `long f(long, ...)`. A wrong guess is
silent, an `int` result has undefined upper bits, an argument count mismatch is
not caught, and no `double` or `float` crosses a call by value (`sqrt`,
`strtod`, `printf("%f")`).

A declaration gives the types of a C function once:

    EXTERNAL INT puts(PTR);
    EXTERNAL DOUBLE sqrt(DOUBLE);
    EXTERNAL PTR malloc(SIZE);
    EXTERNAL VOID free(PTR);
    EXTERNAL INT printf(PTR, ...);          # variadic #
    EXTERNAL LONG write(INT, PTR, SIZE);

The types are C's: `CHAR UCHAR SHORT USHORT INT UINT LONG ULONG LLONG ULLONG
SIZE PTR DOUBLE FLOAT`, and `VOID` as a result only. `LONG` and `SIZE` have the
width of a C `long` (32 bits on a 32-bit host); `LLONG` is 64 bits everywhere.
The name is the C name as written. A call is still `CCALL NAME(ARGS)`. For a
declared function the compiler:

- checks the number of arguments (an error for a wrong number; a variadic
  function takes at least the declared ones);
- converts each argument to the declared type as C would: an integer is
  truncated or extended; a `PTR` takes an address, a word as its value, or a
  string constant directly (`CCALL puts('hello')`, no `LOC`); a `DOUBLE` or
  `FLOAT` takes a `REAL` (the word is the double's bits) or an integer, which is
  converted; the extra arguments of a variadic call get C's default promotions
  (a `REAL` is a double, in the floating-point registers, and the register count
  of the call is set);
- turns the result into the word the program sees: signed types sign-extend,
  unsigned types and `PTR` zero-extend, `DOUBLE` and `FLOAT` become a `REAL`,
  `VOID` gives 0.

An undeclared function is called as today, so no existing program changes. A
function declared twice with different types, or called with arguments its
declaration cannot take, is an error.

*Effect on the code.* The declaration is a statement of `lib/ffi.imc`, kept in
the directory with the name. The IR marks a call to a typed function with its
types, and both IR writers make the LLVM call with those types. The calls in
`lib/*.imp` can then be declared, and the hand-written masks of C `int`
results (`SCMASK` and the like) go.

*Tests.* A program in `tests/native`, run by the seed and by stage 2 on every CI
machine, the 32-bit ones included, covering `sqrt`, `pow`, `strtod`, `snprintf` with
`%f` and `%d`, `strlen`, `labs`, `puts` with a constant, `malloc` and `free`, an
`int` result of -1, the wrong number of arguments, a variadic call with a
`REAL`.

### 2. Callbacks that C can call

A routine that C calls (the comparison of `qsort`, a signal handler, a callback
of `dl_iterate_phdr`) takes its arguments by value, with C's types, and returns
a C type:

    CSUBR CMP(PTR A, PTR B) INT IS ((CCALL strcmp(A, B)) ...)

Its address is `LOC(CMP)`; inside, `A` and `B` are values, not references. Today
such a callback is an IMP `SUBR` whose parameters are pointers to the values C
passed. The exact syntax is settled when item 1 exists.

*Effect on the code.* A second kind of routine in the syntax file, with a
prologue in the code generator that takes C-convention arguments.

### 3. Structure layouts

A declaration of the layout of a C structure (the names, types and offsets of
its fields), and a way to read and write a field of a structure at an address,
in place of byte offsets written by hand. Layouts differ by system, so they
come from the headers (item 4).

*Effect on the code.* Declarations in `lib/ffi.imc` and a field access that the
generator turns into a typed load or store at an offset.

### 4. Import of C declarations from headers

A tool, written in IMP, that reads C prototypes (`int puts(const char *);`) and
writes the `EXTERNAL` declarations of item 1. For structures it probes the
layout with the C compiler of the machine, as `tools/sysconst.sh` does for
constants, so that a program says `INCLUDE`, or the build runs the tool, and has
the declarations of a header.

## Language

### 5. UTF-8 operator symbols (`≤ ≥ ≠ ← ∧ ∨ ¬`)

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

### 6. C string syntax `"text\n"`

`'text'` is the string constant today, two apostrophes are one inside it, and
`!name!` is an exact identifier.

Proposed: `"..."` with C's escapes (`\n \t \\ \" \0 \xHH`, and `\u` as UTF-8
bytes) as a second spelling of the same string constant. `'...'` stays. A
one-character `'a'` stays a string of one character; giving it the value of a
character would change `STG 0,'A'` and every program of the original.

*Effect on the code.* The quoting code of the lexer and the rule for the
constant in `syntax64.imc`. The code generators see a constant either way.

### 7. The empty string `''`

`''` is a syntax error today. The original manual says the string `''` is the
same as 0: the empty ASCIZ string is its terminating zero, which as a value is
the number 0. A quote inside a string stays written doubled.

Proposed: `''` is the constant 0.

*Effect on the code.* One case in `lex.imp` and `stage0/lex.c` (two apostrophes
with no third following: the constant 0) and a test. As a `STG` argument, `''`
is 0, like any other zero word.

### 8. File names with Unix limits

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

### 9. The original's channel I/O

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

### 10. Counted strings

String constants are ASCIZ, as in the original: the bytes, then a zero. They
pass to C unchanged, and `STG` and the library scan to the zero. This stays.

Proposed: a counted string as a separate object type beside it, with its own
handler functions. It is a record: a length word (bytes, the NUL not counted),
the bytes, and a zero byte after them, so that the address of its bytes can be
given to C. It is made and handled only by routines of its own: create from a
constant or a C string, length, copy, concatenate, compare, slice, search,
append, print. A routine that takes a counted string and one that takes an
ASCIZ string are different routines, which the declarations of item 1 tell
apart.

*Effect on the code.* An added part of the library and its declarations. The
lexer, the code generators and the existing library are untouched, and no
existing program changes. It depends on items 1 and 6.

## Open question: tape routines

`docs/porting/tape-names.md` compares the routine names of the tape with the
port's. Two of the tape's routines are not present: `CORE(N)`, a monitor call
to grow the core (the arena is reserved once and `free.imp` grows its usable
part, so `CORE` could return as "make N words usable"), and `RLEAS(CH)`,
which released a channel. Whether either is restored is to be examined
together with item 9, since `RLEAS` is the `fclose` of a channel and `CORE`
may depend on it.
