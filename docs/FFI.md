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

What a function returns: an IMP routine returns a word (`int64_t`; on a 32-bit host the word is a `long long`), so a C caller declares it
`int64_t`, never `long`. A C function called with `CCALL` is assumed to return a `long`, the integer of a pointer's size: a pointer or a
`long` comes back right, an `int` may have undefined upper bits on a 64-bit host and a `double` is not in the integer register, so call
those through a small C function that returns a `long` (a real passes as its bits, `imp_double_to_real`).

Examples that run in the suite (`tests/native/ffi2.imp`): `CCALL STRLEN(P)` with a pointer
from C, `CCALL QSORT(LOC(ARR),N,8,LOC(CMP))` with an IMP `SUBR` as the comparison (its parameters
are pointers to the elements, which is what `qsort` passes), `CCALL PRINTF(LOC('...'),A,B)` with a
string constant (a string constant is bytes, so its address is a C string), and a `COMMON` array that
the C side sorts and reads.

A `REAL` shared with C (a `COMMON REAL`, an array of them) is an IEEE 754 double in a word that is not the double's own
bits (the sign and magnitude, with the magnitude negated for a negative number, as on the PDP-10): C code converts with
`imp_real_to_double` and `imp_double_to_real` from `lib/imp72.h` (`tests/native/realffi.imp`).

Not yet: `REAL` arguments and results by value (SSE registers), more than six
by-value arguments, strings as C strings, structures.
