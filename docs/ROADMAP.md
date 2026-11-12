# Roadmap: what is not done

Each item says what it is, what is proposed, and the effect on the code. None
is implemented; `docs/LANGUAGE-DIFFERENCES.md`, `docs/FFI.md` and
`docs/BUILDING.md` describe what exists. Every change to code generation is
made in both languages of the generator (`src/*.imc` and `stage0/gen_*.c`) and in both IR writers
(`src/ir2ll.imc`, `src/ir2llx.imc` and `tools/ir2ll.c`), and `tests/seed`
requires the seed and stage 2 to keep writing the same IR.

A call to C stays visibly different from a call to IMP, whose arguments are
by reference: `CCALL` is always written.

## Language

### 1. UTF-8 operator symbols (`≤ ≥ ≠ ← ∧ ∨ ¬`)

`src/lex.imc` makes every byte of 128 and up a letter of names, so that a name
may be UTF-8 text; a symbol in an operator position would be read as the start
of a name.

Proposed: spellings of the existing operators, not additional syntax, defined in an
extension file loaded after `syntax.imc` that repeats the rule of `LE` and the
others (`<ATOM> ::= '≤' ::= ...`), the way `lib/ffi.imc` extends the language.
Names keep all bytes of 128 and up except those that a loaded extension makes
into symbols.

*Effect on the code.* The lexer tests a table of symbols before it takes a high
byte for a letter (one routine in `lex.imc`, and in `stage0/lex.c`); the rest
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

Decided (2026-11-03, "`''` keeps its original meaning"): `''` is the constant 0. Details pending: the lexer case and a test.

*Effect on the code.* One case in `lex.imc` and `stage0/lex.c` (two apostrophes
with no third following: the constant 0) and a test. As a `STG` argument, `''`
is 0, like any other zero word.

### 5. The rest of the original's channel I/O

`INIT`, `LOOKUP`, `ENTER`, `INPUT`, `OUTPUT`, `CLOSE`, `RENAME` and `RELEAS` are in `lib/impchan.iml`: the monitor calls of TOPS-10 as routines over
`FILE *`, one buffer of 1024 bytes to a channel, a header of three words (the buffer, the byte pointer, the count). Not provided: the ring of buffers
(its links are 18 bit addresses), dump mode (a command list of `IOWD` words), binary mode and its checksums, the devices but `DSK` and `TTY`,
`USETI` and `USETO` (random access, `fseek`), the status bits of `STATZ` and `STATO`, and the date, protection and project-programmer words of
`LOOKUP`, `ENTER` and `RENAME`.

Decided (2026-11-11: channel I/O comes back; the single buffer is done): the rest as far as a program of the tape needs it; details pending.

*Effect on the code.* `lib/impchan.iml` and its test `tests/native/chan.imp`; `USETI` and `USETO` are an `fseek` and the count; dump mode is a
list of word pairs (count, address) in place of `IOWD`.

### 6. Counted strings

String constants are ASCIZ, as in the original: the bytes, then a zero. They
pass to C unchanged, and `STG` and the library scan to the zero. This stays.

Decided (2026-11-03: a separate object type with its own handler functions, carrying its length and also NUL terminated): a counted string beside it, with its own
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

Decided (2026-11-11: shared declarations and modules are in scope; the mechanism is pending): a way to name a file of declarations that every unit reads, so that
a declaration is written once. Two forms are possible: a statement in the source
that reads a file (`INCLUDE 'decls.imp'`), or a file on the command line that
the driver puts in front of each source. A file named with `-C` does not do it: it is a
unit of its own (it ends at `%%`, and a declaration ends with its unit), where the
declarations are to reach the unit that follows.

*Effect on the code.* The lexer (a stack of input files) or the driver. The
declaration statements of `lib/ffi.imc` stay as they are. A test with two units
that share one file.

### 8. A generated header for exported routines

A `SUBR` is a C function whose parameters are pointers to 64-bit words
(`docs/FFI.md`). C, Zig and Rust code that calls one writes the prototype by
hand.

Decided (2026-11-11: in scope; details pending): a tool, in the manner of `imp-cimport` and written in IMP, that reads
the routines of a source (or of the object's symbols) and writes a header: a C
header (`int64_t NAME(int64_t *A, int64_t *B);`, `extern int64_t X;` for a
`COMMON` variable), a Zig declaration file and a Rust `extern "C"` block. The
parameters stay by reference, which is what they are: each is a word pointer.
A typed export (`EXPORT INT F(INT)` with C's argument passing) is not planned.

*Effect on the code.* A tool in `tools/` and its tests; no change to the
compilers.

### 11. A runtime library for Windows and Cygwin

`imp --target=x86_64-w64-mingw32` and the Cygwin triples write objects and
link a program that needs no runtime (`docs/BUILDING.md`). There is no
`libimp.a` for them: the host layer (`lib/imphost.iml`, `lib/impsys.iml`)
uses `mmap` for the arena, `dl_iterate_phdr`, `fork` and `readlink`, and
`tools/sysconst.sh` describes POSIX systems.

Decided (2026-11-05: Windows is supported where LLVM and mingw can do it, and Cygwin; the code is pending): a COFF variant of the host layer (`lib/imphostx-*.iml`,
`lib/impvarsx-*.iml`) and of the arena (`VirtualAlloc` for Windows, `mmap` for
Cygwin), the constants of both systems in `tools/sysconst.sh`, and a test run on
a Windows or Cygwin machine; none is available for the tests.

*Effect on the code.* New files in `lib/`, a case in `tools/mkrt.sh`, and the
link tables in `src/driver.imc` when a run shows them wrong.

## Open question: tape routines

`docs/porting/tape-names.md` compares the routine names of the tape with the
port's. Two of the tape's routines are not present: `CORE(N)`, a monitor call
to grow the core (the arena is reserved once and `free.imc` grows its usable
part, so `CORE` could return as "make N words usable"), and `RLEAS(CH)`,
which released a channel. Whether either is restored is to be examined
together with item 5, since `RLEAS` is the `fclose` of a channel and `CORE`
may depend on it.
