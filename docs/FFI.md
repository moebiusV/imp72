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

A routine that C calls with its arguments by value (the comparison of `qsort`, a
signal handler) is a callback, declared with `CALLBACK` (below).

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

### Callbacks: an IMP routine that C calls

A `SUBR` takes its arguments by reference, so its address cannot be handed to C
for a call with C's arguments by value (`LOC(CMP)` is the address of the
routine itself, whose parameters are pointers to the values C passed). A
`CALLBACK` declaration gives a `SUBR` the C signature of the call:

    CALLBACK INT CMP(PTR, PTR);
    CALLBACK VOID CATCH(INT);
    CALLBACK DOUBLE HALVE(DOUBLE);
    CALLBACK VOID BYE(VOID);

The grammar is that of `EXTERNAL` without the dots:

    CALLBACK result name ( )
    CALLBACK result name ( type , type ... )

The types are those of the declarations of C functions (table above), and
`(VOID)` is no parameter. `CALLBACK(NAME)` is the address of a C function that
calls the subroutine `NAME`; it is an expression whose value is an address, and
it goes wherever a pointer to a function goes:

    CCALL qsort(LOC(ARR), 8, 8, CALLBACK(CMP));
    CCALL signal(2, CALLBACK(CATCH));
    CCALL atexit(CALLBACK(BYE));

    SUBR CMP(A, B) IS (
      [A] < [B] => RETURN 0-1;
      [A] = [B] => RETURN 0;
      1)

The C function made for the callback (the thunk) takes C's arguments with C's
types in the registers and on the stack of the target, converts each to a word,
puts the word in a variable of its own, and calls the subroutine with the
addresses of the variables, as any call of a `SUBR` passes them. The words are
the ones of a declared C function's result (table above): a signed type is
sign-extended, an unsigned type and `PTR` are zero-extended, `DOUBLE` and
`FLOAT` are a `REAL`. A parameter declared `PTR` is therefore the address C
passed: in `CMP`, `A` is the address of an element, and `[A]` is the element. The
word the subroutine returns is converted to the result type: an integer type
takes the low bits (`RETURN 0-1` is the `int` -1), `PTR` takes the word as an
address, `DOUBLE` and `FLOAT` take a `REAL`, `VOID` ignores it. A `REAL` is not
converted to an integer there, as the subroutine's value is a word.

The thunk is an internal function of the object file, named `NAME@SIGNATURE`
(the signature is that of a typed call, below), made at the first
`CALLBACK(NAME)` of the file. Two files that take the address of the same
callback have each a thunk of their own, and `CALLBACK(NAME)` is not the same
address in both. The subroutine itself need not be in the file: a file that has
the declaration and the expression calls the symbol `NAME`, which another file
defines.

Compile-time checks:

- `CALLBACK(NAME)` of a name that no `CALLBACK` declaration gave a signature is
  an error (`NOPE IS NOT DECLARED A CALLBACK.`);
- a `SUBR` defined after the declaration has as many parameters as the
  declaration, else `CALLBACK CMP HAS 2 PARAMETERS IN ITS DECLARATION AND THE
  SUBR HAS 1.` (declare before the `SUBR` to have the check);
- a name declared twice with different types, or with a type that is not C's, is
  an error, as for `EXTERNAL`;
- `CCALL CMP(...)` of a callback is an error: the name is a `SUBR`, called
  without `CCALL`, and its arguments are references.

The variables of an IMP routine are static, so a callback that is entered again
while it runs (a signal handler that interrupts the same routine, a comparison
that calls `qsort`, a thread that runs it while another does) shares its
variables and the temporaries of its calls with the earlier entry. This is how
IMP routines work (`docs/LANGUAGE-NOTES.md`); a callback that can be re-entered
keeps what it needs in the stack of its caller or does not use variables that a
second entry changes. The runtime library of IMP is not thread safe, so a
callback that a C thread calls (`pthread_create`) does not use `PRINT`, `READ`
or the other routines of the library while another thread does.

### Calling a function through a pointer

A C function pointer that a program receives (an address from `dlsym`, a field
of a structure, a callback's own `CALLBACK(NAME)`) is called with C's types by a
declaration of the signature:

    FNPTR INT CMPCALL(PTR, PTR);
    FNPTR DOUBLE HALVECALL(DOUBLE);
    FNPTR INT PRINTCALL(PTR, ...);

with the grammar of `EXTERNAL`: `FNPTR result name ( type , ... )`, the dots for a
function of a variable number of arguments. The name is then called with the
function as its first argument, an address; the others are the arguments of C's
call, converted as for a declared function:

    R := CCALL CMPCALL(F, LOC(X), LOC(Y));
    X := CCALL HALVECALL(F, 5.0);
    CCALL PRINTCALL(F, 'a=%d%c', 42, 10);

The first argument is counted: a declaration with one type is called with two
arguments (`CCALL PC TAKES 2 ARGUMENTS, NOT 3.`). At most 15 types are declared.
The result is the word the program sees, as for `EXTERNAL`. In the IR the call is
`impcallt@SIGNATURE`, the signature of a typed call with a `p` (the function)
after the letter of the result; the writers of LLVM IR make an indirect call with
the types.

The name of a `FNPTR` declaration belongs to the file as the name of a declared
C function does. It is not a symbol: no C function by that name is called.

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
- `tests/native/callback.imp`: `qsort` and `bsearch` with an IMP comparison, a comparison
  that calls IMP code and a declared C function, `signal` and `raise`, `atexit`, and
  callbacks and calls through a pointer with every kind of type (`CHAR`, `UCHAR`,
  `SHORT`, `UINT`, `LLONG`, `FLOAT`, `DOUBLE`, `PTR`, `VOID`), `printf` through a pointer.
- `tests/cli/run.sh`: the compile-time errors.

A `REAL` shared with C (a `COMMON REAL`, an array of them) is an IEEE 754
double in a word that is the double's own bits. C code converts with
`imp_real_to_double` and `imp_double_to_real` from `lib/imp72.h`
(`tests/native/realffi.imp`).

## Limits

- A function that is not declared is `long f(long, ...)`.
- At most 16 arguments, and 16 parameters in a declaration.
- A structure by value is not a type.
- A callback has at most 16 parameters and is not variadic. A function pointer
  declaration (`FNPTR`) has at most 15 types besides the function.
- A callback returns a word; a structure result or parameter is not a type.
- The compiler links the C library and not the math library: where the math
  functions are a library of their own (glibc's libm), `sqrt`, `pow` and `sin`
  can be declared and called but not linked, while the functions of the C
  library that take and return `double` (`ldexp`, `frexp`, `strtod`, `atof`) link
  everywhere.
- The routines of `lib/*.imp` that call C by hand, with masks for the `int`
  results, are written as before.
