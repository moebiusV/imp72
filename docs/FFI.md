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
returns its result as a word. `CCALL` is always written for a call to C; a plain
`NAME(A, B)` is a call by reference, to an IMP routine. At most 16 arguments.
`LOC(X)` is the address of X. Registers and stack are used as the target's C ABI
uses them.

A function that is not declared is taken as `long NAME(long, ...)`: the
arguments are integers or addresses, a string constant is passed as the
address `LOC('text')`, and the result is the integer of a pointer's size. A
pointer or a `long` comes back right. An `int` result arrives in half a
register with undefined upper bits on a 64-bit host: mask it (`AND 4294967295`)
before comparing it with a negative number or a 64-bit value. A `double` or
a `float` cannot cross such a call. A declaration (below) removes these
limits.

### Declarations of C functions

A statement gives the types of a C function once:

    EXTERNAL INT puts(PTR);
    EXTERNAL DOUBLE ldexp(DOUBLE, INT);
    EXTERNAL PTR malloc(SIZE);
    EXTERNAL VOID free(PTR);
    EXTERNAL INT printf(PTR, ...);          # variadic #
    EXTERNAL LONG write(INT, PTR, SIZE);

The grammar is

    EXTERNAL result name ( )
    EXTERNAL result name ( type , type ... )
    EXTERNAL result name ( type , type , ... )
    EXTERNAL result name ( ... )

The types are C's, written in capitals (names are case sensitive):

| type | C type | the word the program sees |
|---|---|---|
| `CHAR`, `SHORT`, `INT` | `signed char`, `short`, `int` | sign-extended |
| `UCHAR`, `USHORT`, `UINT` | the unsigned ones | zero-extended |
| `LONG`, `ULONG`, `SIZE` | `long`, `unsigned long`, `size_t` | sign- or zero-extended from the width of a C `long`, 32 bits where the pointers are 32 bits |
| `LLONG`, `ULLONG` | `long long`, `unsigned long long` | 64 bits everywhere |
| `PTR` | any pointer | zero-extended from the width of a pointer |
| `DOUBLE`, `FLOAT` | `double`, `float` | a `REAL` (the bits of a double) |
| `VOID` | `void` | 0; a result only, or the whole parameter list: `(VOID)` is no parameter |

The name is the C name as written; `!name!` writes a name that is not an IMP
name. Three dots after the last type declare a function of a variable number of
arguments. A declaration is a statement and holds to the end of the
compilation unit (a file); a function declared in one file and called in
another is declared in each. The type names are not reserved: a variable may
be called `INT`.

For a call `CCALL NAME(ARGS)` of a declared function the compiler:

- counts the arguments: a wrong number is a compile-time error (`CCALL puts
  TAKES 1 ARGUMENTS, NOT 2.`); a variadic function takes at least the declared
  ones;
- converts each argument as C would. An integer is truncated to the width of
  its type, so a `CHAR` parameter given 353 receives 97. A `PTR` takes a word
  as its value, or a string constant directly: `CCALL puts('hello')` passes the
  address of the bytes, ended by a zero byte, with no `LOC`. A `DOUBLE` or
  `FLOAT` takes a `REAL` or an integer, which is converted to real (`CCALL
  ldexp(3,2)` passes 3.0); a `REAL` given to an integer type or a `PTR` is
  converted to an integer, as `FIX` does;
- gives the arguments beyond the declared ones of a variadic call C's default
  promotions: a `REAL` is a `double`, passed in a floating-point register with
  the count of such registers set for the callee, and an integer is a `long`. An
  argument with no declared type cannot be told to be an address, so a string
  constant is the word of its characters there: the address is `LOC('text')`;
- makes the result the word the program sees (table above); the call has the
  type real for `DOUBLE` and `FLOAT`, so it mixes with other reals.

A function declared twice with different types, declared after a call of it
that had no declaration, or declared with a type name that is not C's, is a
compile-time error. A function called with `CCALL` and not declared is called as
before.
In the IR a declared function is named `NAME@SIGNATURE` in the call: the
signature is a letter for the result and one for each argument (`c C s S i I l L
q Q z p d f v` for `CHAR UCHAR SHORT USHORT INT UINT LONG ULONG LLONG ULLONG
SIZE PTR DOUBLE FLOAT VOID`), with a dot before the arguments of a variadic call
that have no declared type: `printf@ip.d` is `int printf(ptr, ...)` called with
a double. A `DOUBLE`, `FLOAT`, `LLONG` or `ULLONG` argument is the address of
the 64-bit word `imp_cw0` to `imp_cw15`, stored just before the call, because the
call of the IR cannot carry 64 bits on a 32-bit machine. Both writers of LLVM
IR make the call with the types (`declare i32 @puts(ptr)`, `call double
@ldexp(double %x, i32 signext %n)`, `call i32 (ptr, ...) @printf(ptr %f, double
%x)`), so LLVM's calling convention for the target does the rest: the floating
point registers, the count of vector registers of a variadic call on x86-64,
the 32-bit machines. The call has the type long on every machine, whatever the
result is, so the 32-bit targets do not truncate a 64-bit result.

`X IS EXTERNAL;` declares a variable defined elsewhere, in the C library for
example (`!environ! IS EXTERNAL;`); nothing is defined here, and `LOC(X)` is its
address. `!name!` writes a name that is not an IMP name.

`CCALL impcallp(F, A, ...)` and `CCALL impcallw(F, A, ...)` call the function
at address F with arguments by value. The result of `impcallp` is a C `long`;
that of `impcallw` is a 64-bit word. `CCALL impcallw(LOC(TWICE), LOC(Y))`
calls an IMP routine through its address; its arguments are references, so
they are addresses.

Examples that run in the suite:

- `tests/native/ffi2.imp`: `CCALL strlen(P)` with a pointer from C;
  `CCALL qsort(LOC(ARR),N,8,LOC(CMP))` with an IMP `SUBR` as the comparison (its
  parameters are pointers to the elements, which is what `qsort` passes);
  `CCALL printf(LOC('...'),A,B)` with a string constant (a string constant is
  bytes, so its address is a C string); a `COMMON` array that the C side sorts
  and reads.
- `tests/native/extcall.imp`: `EXTERNAL`, `impcallp`, `impcallw`.
- `tests/native/cdecl.imp`: declared functions with `double` and `float`
  arguments and results (`ldexp`, `ldexpf`, `strtod`, `copysignf`, `frexp`),
  `snprintf` and `printf` with a `REAL`, `puts` with a string constant,
  `malloc` and `free`, an `int` result of -1, a `uint` result, a call from a
  `SUBR`.
- `tests/native/cdecl2.imp`: every integer type as parameter and as result, a
  function with no parameter, `VOID`.
- `tests/cli/run.sh`: the compile-time errors.

A `REAL` shared with C (a `COMMON REAL`, an array of them) is an IEEE 754
double in a word that is the double's own bits. C code converts with
`imp_real_to_double` and `imp_double_to_real` from `lib/imp72.h`
(`tests/native/realffi.imp`).

## Limits

- A function that is not declared is `long f(long, ...)`.
- At most 16 arguments, and 16 parameters in a declaration.
- A structure by value is not a type.
- The compiler links the C library and not the math library: where the math
  functions are a library of their own (glibc's libm), `sqrt`, `pow` and `sin`
  can be declared and called but not linked, while the functions of the C
  library that take and return `double` (`ldexp`, `frexp`, `strtod`, `atof`) link
  everywhere.
- The routines of `lib/*.imp` that call C by hand, with masks for the `int`
  results, are written as before.
