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

### 5. What is left of the original's channel I/O

`INIT`, `LOOKUP`, `ENTER`, `INPUT`, `OUTPUT`, `CLOSE`, `RENAME` and `RELEAS` are in `lib/impchan.iml`: the monitor calls of TOPS-10 as routines over
`FILE *`, one buffer of 1024 bytes to a channel, a header of three words (the buffer, the byte pointer, the count). Added since: `USETI` and `USETO`
(an `fseek` to a block of 1024 bytes), the status of a channel (`GETSTS`, `SETSTS`, `STATZ`, `STATO`, with the bits of TOPS-10 for the end of the
file and a failed transfer), and dump mode (mode `17B`: `INPUT` and `OUTPUT` take a list of pairs, count and address, ended by a count of 0, and
move that many words of 8 bytes). `RLEAS` is back and the other `CORE` is in `free.imc`. The test is `tests/native/chan.imp` and `chan2.imp`.

Not provided: binary mode and its checksums (a block of 36 bit words with a count and a checksum word has no file format on a machine with 64
bit words that a program of the tape could read), the ring of buffers (its links are 18 bit addresses), the devices but `DSK` and `TTY`, and the
date, protection and project-programmer words of `LOOKUP`, `ENTER` and `RENAME` (a Unix file has none of them that a program could set).
None of the tape's own programs uses any of these.

Decided (2026-11-11): the rest as far as a program of the tape needs it; the programs of the tape (the compiler's modules) need none of what is
not provided, so it stays so until a program that does is found.

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
existing program changes. Item 2 (a string in double quotes) is done.

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

### 11. A runtime library for Windows

`imp --target=x86_64-w64-mingw32` writes objects and links a program that needs no runtime (`docs/BUILDING.md`). There is no `libimp.a` for it: the host
layer (`lib/imphost.iml`, `lib/impsys.iml`) uses `mmap` for the arena, `fork`, `waitpid` and `readlink`, and `tools/sysconst.sh` describes POSIX
systems. Cygwin has the library, built but not run (`lib/imphostx-coff.iml`, `lib/impvarsx-coff.iml`; no machine to run it on).

Decided (2026-11-05: Windows is supported where LLVM and mingw can do it; the code is pending): a Windows variant of the arena (`VirtualAlloc` to reserve
the address space and to commit it as `CORE` asks), of the process routines (`CreateProcess` and `WaitForSingleObject` where the host layer forks) and the
constants of `tools/sysconst.sh`, and a test run on a Windows machine; none is available for the tests, and the code of a layer that cannot be run is
not written.

*Effect on the code.* New files in `lib/`, a case in `tools/mkrt.sh`, and the link tables in `src/driver.imc` when a run shows them wrong.

## Open question: tape routines

`docs/porting/tape-names.md` compares the routine names of the tape with the
port's. Two of the tape's routines are not present: `CORE(N)`, a monitor call
to grow the core (the arena is reserved once and `free.imc` grows its usable
part, so `CORE` could return as "make N words usable"), and `RLEAS(CH)`,
which released a channel. Whether either is restored is to be examined
together with item 5, since `RLEAS` is the `fclose` of a channel and `CORE`
may depend on it.
