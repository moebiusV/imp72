# C interop: safe, seamless, natural

*Goal (the user, 2026-11-03): C interop as safe, seamless, easy and natural as possible. Decided: `CCALL` stays and is always written (a call to C is visibly different from a call to IMP, whose
arguments are by reference); the work is in four parts, one commit each, in this order. This is study 4 of `pre-1.0-studies.md` widened.*

## Today (checked 2026-11-03)

- Every C function is assumed to be `long f(long, …)`. There are no declarations: a wrong guess is silent, an `int` result has undefined upper bits, an argument-count mismatch is not caught.
- No `double` or `float` by value, in or out (`sqrt`, `strtod`, `printf("%f")`); the variadic count register is set to zero.
- A callback (the comparison of `qsort`, a signal handler) is an IMP `SUBR`, whose parameters are by reference: C passes values, so the routine receives pointers it has to know to use.
- No structures: fields are reached by byte offsets written out by hand.
- No import of C declarations. (What works: a string constant is bytes and passes as a C string with `LOC`; pointers of 32-bit hosts; more than six arguments.)

## Part 1: typed declarations of C functions

A declaration gives the types of a C function, once:

    EXTERNAL INT puts(PTR);
    EXTERNAL DOUBLE sqrt(DOUBLE);
    EXTERNAL PTR malloc(SIZE);
    EXTERNAL VOID free(PTR);
    EXTERNAL INT printf(PTR, ...);          # variadic #
    EXTERNAL LONG write(INT, PTR, SIZE);

The types are C's: `CHAR UCHAR SHORT USHORT INT UINT LONG ULONG LLONG ULLONG SIZE PTR DOUBLE FLOAT` (`VOID` as a result only); `LONG` and `SIZE` have the width of a C `long`, 32 bits on a 32-bit host,
`LLONG` is 64 bits everywhere. The name is the C name as written (case sensitive; `!name!` for one that is not an IMP name). A call is still `CCALL NAME(ARGS)`. For a **declared** function the compiler:

- checks the number of arguments (an error for a wrong number; a variadic function takes at least the declared ones);
- converts each argument to the declared type as C would: an integer is truncated or extended to `CHAR`…`LLONG`; a `PTR` takes an address, **a string constant directly (`CCALL puts('hello')`, no `LOC`)**, a word as its value;
  a `DOUBLE` or `FLOAT` takes a `REAL` (the word is the double's bits), or an integer, which is converted; the extra arguments of a variadic call get C's default promotions (a `REAL` is a double, in the SSE registers,
  and the register count of the call is set);
- turns the result into the word the program sees: signed types sign-extend, unsigned types and `PTR` zero-extend (a 32-bit pointer is not sign-extended), `DOUBLE` and `FLOAT` become a `REAL`, `VOID` gives 0.

An **undeclared** function is called as today (`long`, integer arguments), so no existing program changes. A function declared twice with different types, or called with arguments the declaration cannot take, is an error.
The declaration is a statement of `lib/ffi.imc` (and so in both compilers), kept in the directory with the name; the IR marks a call to a typed function (`# cfunc` today) with its types, and `src/ir2ll.imp` and
`tools/ir2ll.c` (the seed's copy) make the LLVM call with those types. The calls in the library (`lib/*.imp`) are then declared and the hand-written masks (`SCMASK` and friends) can go.

*Tests:* `tests/native/cdecl.imp`, run by the seed and by stage 2 (and on every machine of CI, the 32-bit ones included): `sqrt`, `pow`, `strtod`, `snprintf` with `%f` and `%d`, `strlen`, `labs`, `puts` with a constant,
`malloc`/`free`, an `int` result of -1, the wrong number of arguments (an error), a variadic call with a `REAL`.

## Part 2: callbacks that C can call

A routine that C calls (the comparison of `qsort`, a signal handler, a callback of `dl_iterate_phdr`) takes its arguments by value, with C's types, and returns a C type:

    CSUBR CMP(PTR A, PTR B) INT IS ((CCALL strcmp(A, B)) ...)

Its address is `LOC(CMP)`; inside, `A` and `B` are values (words), not references. (Syntax to be settled when part 1 is done.)

## Part 3: structures

A declaration of the layout of a C structure (the names, types and offsets of its fields), and a natural way to read and write a field of a structure at an address, in place of byte offsets by hand. The layouts come from the
headers (part 4), because they differ by system.

## Part 4: importing C declarations

A tool, in IMP, that reads C prototypes (`int puts(const char *);`) and writes the `EXTERNAL` declarations of part 1, and for structures probes the layout with the C compiler of the machine the way `tools/sysconst.sh`
does for constants, so that a program says `INCLUDE` (or the tool is run by the build) and has the declarations of a header.

## Both compilers

Every part is done in the generator of `src/llvm-ir/*.imp` and in its C port `stage0/gen_*.c`, and in the writers of LLVM IR, `src/ir2ll.imp` and `tools/ir2ll.c`; `tests/seed` requires them to write the same IR.
