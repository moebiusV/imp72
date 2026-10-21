# Calling C from IMP72, and IMP72 from C

## IMP72 from C

A `SUBR` is a C function. IMP72 passes arguments by reference, as FORTRAN
does, so each parameter is a pointer to a 64-bit word (`long *`), and the
result is a `long`. `SUBR ADD2(A,B) IS A+B` is `long add2(long *, long *)`.
Names are lower case. More than six parameters go on the stack as the C ABI
puts pointers. A variable declared `COMMON` is a global symbol, so C can declare it
`extern long x;` (`X IS COMMON;`, or `A IS COMMON,8 LONG;` for an array); any other variable belongs
to its module, as in the original, and C cannot see it. See `tests/native/*_main.c`.

## C from IMP72, arguments by value

`lib/ffi.imc` (loaded by `imp72stage0` after the compiler's syntax) adds

    CCALL NAME(A, B, ...)

which calls the C function `name` passing the *values* of its arguments in
the C registers (up to six) and returns its `long` result. `LOC(X)` is the
address of X, to pass pointers. The `%al` count for variadic functions is set
to zero. A plain `NAME(A, B)` call remains by reference.

Examples that run in the suite (`tests/native/ffi2.imp`): `CCALL STRLEN(P)` with a pointer
from C, `CCALL QSORT(LOC(ARR),N,8,LOC(CMP))` with an IMP `SUBR` as the comparison (its parameters
are pointers to the elements, which is what `qsort` passes), `CCALL PRINTF(LOC('...'),A,B)` with a
string constant (a string constant is bytes, so its address is a C string), and a `COMMON` array that
the C side sorts and reads.

Not yet: `REAL` arguments and results by value (SSE registers), more than six
by-value arguments, strings as C strings, structures.
