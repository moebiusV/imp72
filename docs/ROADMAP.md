# Roadmap: what is not done

Each item says what it is, what is proposed, and the effect on the code. None
is implemented; `docs/LANGUAGE-DIFFERENCES.md`, `docs/FFI.md` and
`docs/BUILDING.md` describe what exists. Every change to code generation is
made in both languages of the generator (`src/llvm-ir/*.imp` and `stage0/gen_*.c`) and in both IR writers
(`src/ir2ll.imp`, `src/ir2llx.imp` and `tools/ir2ll.c`), and `tests/seed`
requires the seed and stage 2 to keep writing the same IR.

A call to C stays visibly different from a call to IMP, whose arguments are
by reference: `CCALL` is always written.

## Language

### 1. UTF-8 operator symbols (`≤ ≥ ≠ ← ∧ ∨ ¬`)

`src/lex.imp` makes every byte of 128 and up a letter of names, so that a name
may be UTF-8 text; a symbol in an operator position would be read as the start
of a name.

Proposed: spellings of the existing operators, not additional syntax, defined in an
extension file loaded after `syntax.imc` that repeats the rule of `LE` and the
others (`<ATOM> ::= '≤' ::= ...`), the way `lib/ffi.imc` extends the language.
Names keep all bytes of 128 and up except those that a loaded extension makes
into symbols.

*Effect on the code.* The lexer tests a table of symbols before it takes a high
byte for a letter (one routine in `lex.imp`, and in `stage0/lex.c`); the rest
is a file of rules. A name that used one of those characters would change
meaning.

### 2. C string syntax `"text\n"`

`'text'` is the string constant, two apostrophes are one inside it, and
`!name!` is an exact identifier.

Proposed: `"..."` with C's escapes (`\n \t \\ \" \0 \xHH`, and `\u` as UTF-8
bytes) as a second spelling of the same string constant. `'...'` stays. A
one-character `'a'` stays a string of one character; giving it the value of a
character would change `STG 0,'A'` and every program of the original.

*Effect on the code.* The quoting code of the lexer and the rule for the
constant in `syntax.imc`. The code generators see a constant either way.

### 3. The empty string `''`

`''` is a syntax error. The original manual says the string `''` is the
same as 0: the empty ASCIZ string is its terminating zero, which as a value is
the number 0. A quote inside a string stays written doubled.

Proposed: `''` is the constant 0.

*Effect on the code.* One case in `lex.imp` and `stage0/lex.c` (two apostrophes
with no third following: the constant 0) and a test. As a `STG` argument, `''`
is 0, like any other zero word.

### 4. File names with Unix limits

`MKNAME` (`src/impm.imp`) builds a file name from two words (15 characters)
and an extension of one word (7); the original had six and three. The library
cuts the names of `FILE` items in `PRINT` and `READ` to 7 characters
(`lib/impio.imp`, label `CM6`).

Proposed: keep the form `FILE NAME.EXT`, but let a name be as long as the
string given (up to `NAME_MAX`), since the library takes it from the string. The
compiler's own `MKNAME` is for names on its command line, which a shell has
limited by a shell: raise it to `PATH_MAX`.

*Effect on the code.* `MKNAME` and its buffer `NMB`; the `%.7s` and the 64-byte
file items of the library take the length of the buffer. A test with a name of
200 characters.

### 5. The original's channel I/O

`PRINT`, `READ` and `PRINT FILE` are in `lib/impio.imp`. The channel
operations of the TOPS-10 library (`INIT`, `LOOKUP`, `ENTER`, `INPUT`,
`OUTPUT`, the buffer header) are not in `decus/syntax.imc` or `decus/impsyn.imc`;
the original reaches them through its libraries, which the port does not have.

Proposed: statements with the original's meaning over `fopen`, `getc` and
`putc`. A channel is an index into a table of `FILE *`; the buffer header is a
small record that the program may read (count, pointer), kept in step by the
library. Programs of the original that call them then compile. `RLEAS` is the
`fclose` of a channel.

*Effect on the code.* A new part of the library, about the size of the file part
of `impio.imp`, and the statements in an extension file. How many routines of
`decus/` use the buffer header words directly decides whether the buffer
emulation is cheap; it has not been counted.

### 6. Counted strings

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

*Effect on the code.* A new part of the library and its declarations. The
lexer, the code generators and the existing library are untouched, and no
existing program changes. It depends on item 2.

### 7. A shared declarations file

A declaration (`EXTERNAL`, `CALLBACK`, `FNPTR`, `STRUCT`, `LET`) holds to the end
of the compilation unit, one file ending at `%%`. A function that several
files call is declared in each. `imp-cimport` writes a fragment, and the
fragment goes in front of every source with `cat`.

Proposed: a way to name a file of declarations that every unit reads, so that
a declaration is written once. Two forms are possible: a statement in the source
that reads a file (`INCLUDE 'decls.imp'`), or a file on the command line that
the driver reads before each source, as it reads a file named with `-C`.

*Effect on the code.* The lexer (a stack of input files) or the driver. The
declaration statements of `lib/ffi.imc` stay as they are. A test with two units
that share one file.

### 8. A generated header for exported routines

A `SUBR` is a C function whose parameters are pointers to 64-bit words
(`docs/FFI.md`). C, Zig and Rust code that calls one writes the prototype by
hand.

Proposed: a tool, in the manner of `imp-cimport` and written in IMP, that reads
the routines of a source (or of the object's symbols) and writes a header: a C
header (`int64_t NAME(int64_t *A, int64_t *B);`, `extern int64_t X;` for a
`COMMON` variable), a Zig declaration file and a Rust `extern "C"` block. The
parameters stay by reference, which is what they are: each is a word pointer.
A typed export (`EXPORT INT F(INT)` with C's argument passing) is not planned.

*Effect on the code.* A tool in `tools/` and its tests; no change to the
compilers.

### 9. Initialising the runtime from a C program

The `main` of the runtime library (`lib/impmain.imp`) keeps `argc` and `argv`
for `impargc` and `impargvp`, and calls `impmain`. A C program with a `main` of
its own does not link it. The command line is then not kept for the IMP
routines, and the only way to start the main program of an IMP source is to
call `impmain`.

Proposed: split that `main` into `imp_init(argc, argv)` and `imp_run()`,
declared in `lib/imp.h`. A C program calls `imp_init` first, so that the
IMP routines find the runtime initialised, and `imp_run` where it wants the
main program of an IMP source. The library's `main` becomes the two calls.

*Effect on the code.* `lib/impmain.imp`, `lib/imp.h`, and a test with a C
`main` that prints through `PRINT`.

### 10. Parity of the seed's `-S` and `-Sir` with `imp`

`imp -S` writes the target's assembly and `-Sir` writes LLVM IR text. The
seed's `-S` writes LLVM IR text and it has no `-Sir` and no `--target`
(`docs/BUILDING.md`, The seed and `imp`).

Proposed: the seed's `-S` writes assembly and `-Sir` the IR, with the same file
names as `imp`. `--target` stays in `imp`: the seed makes code for the
machine it runs on, because it is the compiler that exists before the
other.

*Effect on the code.* `stage0/imps0.c` and `stage0/llvmorc.c`; `tests/seed/run.sh`,
which takes the seed's IR with `-S`, takes it with `-Sir`.

### 11. A runtime library for Windows and Cygwin

`imp --target=x86_64-w64-mingw32` and the Cygwin triples write objects and
link a program that needs no runtime (`docs/BUILDING.md`). There is no
`libimp.a` for them: the host layer (`lib/imphost.imp`, `lib/impsys.imp`)
uses `mmap` for the arena, `dl_iterate_phdr`, `fork` and `readlink`, and
`tools/sysconst.sh` describes POSIX systems.

Proposed: a COFF variant of the host layer (`lib/imphostx-*.imp`,
`lib/impvarsx-*.imp`) and of the arena (`VirtualAlloc` for Windows, `mmap` for
Cygwin), the constants of both systems in `tools/sysconst.sh`, and a test run on
a Windows or Cygwin machine; none is available for the tests.

*Effect on the code.* New files in `lib/`, a case in `tools/mkrt.sh`, and the
link tables in `src/driver.imp` when a run shows them wrong.

### 12. The result type of an IMP routine, seen from C

A `SUBR` returns a word, and C declares it `int64_t` (`docs/FFI.md`): the word
is 64 bits on every host, so a C `long` would be wrong on a 32-bit one. No
typedef for it exists (`lib/imp.h` has none); a C caller writes `int64_t`.

Proposed: a study, before 1.0, of whether C callers need a name for the word
(a `typedef` in `lib/imp.h`) or a way for a routine to declare the C type of its
result, which would belong with item 8, the generated header. Until the study
is made the word stays `int64_t`.

*Effect on the code.* None until the study decides; a typedef is one line of
`lib/imp.h`, a declared result type for exports is part of item 8.

## Open question: tape routines

`docs/porting/tape-names.md` compares the routine names of the tape with the
port's. Two of the tape's routines are not present: `CORE(N)`, a monitor call
to grow the core (the arena is reserved once and `free.imp` grows its usable
part, so `CORE` could return as "make N words usable"), and `RLEAS(CH)`,
which released a channel. Whether either is restored is to be examined
together with item 5, since `RLEAS` is the `fclose` of a channel and `CORE`
may depend on it.
