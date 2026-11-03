# Calling C from IMP72, and IMP72 from C

## IMP72 from C

A `SUBR` is a C function. IMP72 passes arguments by reference, so each
parameter is a pointer to a 64-bit word and the result is a 64-bit word.
`SUBR ADD2(A,B) IS A+B` is `int64_t ADD2(int64_t *, int64_t *)`. A caller
declares the result `int64_t`, never `long`: on a 32-bit host the word is a
`long long`. More than six parameters go on the stack as the C ABI puts
pointers.

A symbol is the name exactly as written. IMP72 names are case sensitive, as the
original's are, and nothing is folded: `ADD2` is `ADD2`, and a C caller says
`ADD2`. An IMP program calls a C function by its own name: `CCALL strlen(P)`,
`CCALL LLVMGetHostCPUName(0)`. A name may contain `_`.

A variable declared `COMMON` is a global symbol, so C declares it
`extern int64_t X;` (`X IS COMMON;`, or `A IS COMMON,8 LONG;` for an array).
Any other variable belongs to its module, as in the original, and C cannot see
it. `tests/native/*_main.c` are C programs that call IMP.

## C from IMP72, arguments by value

`lib/ffi.imc` (read by both compilers after the language's syntax) adds

    CCALL NAME(A, B, ...)

which calls the C function `NAME` with the values of its arguments and
returns its result as a word. Arguments are integers or addresses; `LOC(X)` is the
address of X. Registers and stack are used as the target's C ABI uses them. A plain `NAME(A, B)` is a call by reference, to an IMP
routine.

`X IS EXTERNAL;` declares a variable defined elsewhere, in the C library for
example (`!environ! IS EXTERNAL;`); nothing is defined here, and `LOC(X)` is its
address. `!name!` writes a name that is not an IMP name.

`CCALL impcallp(F, A, ...)` and `CCALL impcallw(F, A, ...)` call the function
at address F with arguments by value. The result of `impcallp` is a C `long`;
that of `impcallw` is a 64-bit word. `CCALL impcallw(LOC(TWICE), LOC(Y))`
calls an IMP routine through its address; its arguments are references, so
they are addresses.

What a C function returns: `CCALL` takes the result as a `long`, the integer of
a pointer's size. A pointer or a `long` comes back right. An `int` result
arrives in half a register with undefined upper bits on a 64-bit host: mask it
(`AND 4294967295`) before comparing it with a negative number or a 64-bit
value. A `double` is not in the integer register, so call such a function
through a small C function that returns a `long`.

Examples that run in the suite:

- `tests/native/ffi2.imp`: `CCALL strlen(P)` with a pointer from C;
  `CCALL qsort(LOC(ARR),N,8,LOC(CMP))` with an IMP `SUBR` as the comparison (its
  parameters are pointers to the elements, which is what `qsort` passes);
  `CCALL printf(LOC('...'),A,B)` with a string constant (a string constant is
  bytes, so its address is a C string); a `COMMON` array that the C side sorts
  and reads.
- `tests/native/extcall.imp`: `EXTERNAL`, `impcallp`, `impcallw`.

A `REAL` shared with C (a `COMMON REAL`, an array of them) is an IEEE 754
double in a word that is the double's own bits. C code converts with
`imp_real_to_double` and `imp_double_to_real` from `lib/imp72.h`
(`tests/native/realffi.imp`).

## Limits

Every C function is taken as `long f(long, ...)`. Arguments are integers or
addresses; a string is passed as the address of a string constant.
