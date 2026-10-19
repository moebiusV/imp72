# Calling C from IMP72, and IMP72 from C

## IMP72 from C

A `SUBR` is a C function. IMP72 passes arguments by reference, as FORTRAN
does, so each parameter is a pointer to a 64-bit word (`long *`), and the
result is a `long`. `SUBR ADD2(A,B) IS A+B` is `long add2(long *, long *)`.
Names are lower case. More than six parameters go on the stack as the C ABI
puts pointers. Variables of the IMP program are common symbols (`.comm`), so
C can declare them `extern long x;`. See `tests/native/*_main.c`.

## C from IMP72, arguments by value

`lib/ffi.imc` (loaded by `imp72stage0` after the compiler's syntax) adds

    CCALL NAME(A, B, ...)

which calls the C function `name` passing the *values* of its arguments in
the C registers (up to six) and returns its `long` result. `LOC(X)` is the
address of X, to pass pointers. The `%al` count for variadic functions is set
to zero. A plain `NAME(A, B)` call remains by reference.

Not yet: `REAL` arguments and results by value (SSE registers), more than six
by-value arguments, strings as C strings, structures.
