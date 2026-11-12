# IMP72 on the PDP-10 and IMP72 here

The language is the original's. `docs/manual/imp.md` is its manual, and
`src/syntax.imc` (the original `decus/syntax.imc` with `:=`, words of
64 bits and the operations of the IR machine) is its definition. What differs
comes from the machine and from the runtime.

- **Assignment is `:=`, and `_` is a letter.** The tape writes `X_5`; here
  `X:=5`, which the lexer reads as one token (`::=`, the separator of syntax
  statements, is not `:` followed by `:=`). A name may contain `_`, so every IMP
  name is a valid C and ELF name. `decus/` and `docs/manual/imp.md` keep the
  original's `_`. In a syntax statement the token is quoted, `':='`. The command
  line of a bare `imp` is not IMP source: there `_` still separates
  (`OBJ,LIST_SRC`).
- **Names are case sensitive.** `ab` and `AB` are two variables, as on TOPS-10,
  and the symbol of a routine or `COMMON` variable is its name as written.
- **The word is 64 bits.** Integers are 64-bit two's complement, addresses are
  byte addresses, and `A[I]` scales by 8. A constant is folded as the program
  would compute it, in 64 bits. An octal constant is a 64-bit number:
  `777777777777B` is 2**36-1, and -1 is `1777777777777777777777B`.
- **Halves are 32 bits.** `XWD A,B` packs two 32-bit halves; `IOWD` follows. A
  mask such as `AND 777777B` is the 18-bit mask it is written as.
- **Reals** are IEEE 754 binary64, not the PDP-10's 27-bit floating point.
  Arithmetic is in double precision, a constant is the double nearest to it
  (`0.25` is exact; the original's conversion is one unit low), and `FIX` and `FLOAT`
  convert exactly. The word of a `REAL` is the double's own bits, so compares are
  floating compares and negation flips the sign bit. `PRINT FLT m.n` keeps the
  original's layout (no 0 before the point, fixed notation unless it does not
  fit the field, then `.ddd"e`, a negative width for scientific always) with
  correctly rounded digits, ties away from zero; zero prints as `0`; not a number
  and infinity print as `NAN` and `INF`, which the original lacks. So
  programs with reals print the original's numbers except where its 27-bit
  arithmetic showed (`0.001` printed as `.99999"-3` there). C code that shares a
  `REAL` converts with `imp_real_to_double` and `imp_double_to_real`
  (`lib/imp.h`).
- **The operators** `≤ ≥ ≠ ∧ ∨ ¬ ←` are spellings of `LE GE NE AND OR NOT :=` (`lib/utf8.imc`, a syntax extension read
  beside `lib/ffi.imc`); the arrows and mathematical operators of Unicode are symbols and never part of a name.
- **Strings and numbers** are what a C program on a 64-bit machine holds; the original's representation and this one, side by side, are in the manual (`docs/manual/imp.md`, 1.4, item 14).
- **Strings** may be written in double quotes with C's escapes (`"a\tb\n"`,
  `\xHH`, `\uHHHH`, `\UHHHHHHHH`; `\0` ends the string), the same constant as one in
  apostrophes; in a syntax statement the double quote is the delimiter of a semantic
  string, as in the original. A string constant is a NUL-terminated C string in a word
  array, not five 7-bit characters to a word, so its address can be passed to C.
  The compiler packs the characters of
  a name eight to a word inside its directory, not in programs.
- **Byte pointers** to characters (`BYTEP A<7,36>`) are byte addresses and a
  character is a byte (UTF-8 text passes through unchanged); `<s,p>` fields and
  other `BYTEP` pointers work on the bits of the word.
- **Variables** that are not `COMMON` belong to their module (compilation unit);
  `COMMON` ones are global symbols that C can name (`docs/FFI.md`).
- **Input and output** are the C library's. `PRINT` and `READ` keep their
  formats (`STG`, `IGR`, `OCT`, `FLT`, `TAB`, `FILL`, ...); `FILE`, `DEVICE` and
  `IMAGE MODE` map to files and standard input and output, at most four files at
  a time. A file name is a
  string, up to what a Unix path is (4095 bytes) with an extension of up to 255, and image mode moves 8 bytes per word. `FINI(0)`
  closes the files and exits.
- **Time and date.** `CALLI(14B,0)` is Unix time, the seconds since 1970-01-01 00:00:00 UTC, in a 64 bit word (the monitor's
  was the date as `((YEAR-1964)*12+MONTH-1)*31+DAY-1`, a number that ran out in 2027); `CALLI(23B,0)` is the same in milliseconds
  (to the second), where the monitor's was the milliseconds of the day; `CALLI(30B,0)` is the process number. Dates are written as
  `YYYY-MM-DD` and times as `HH:MM:SS` (ISO 8601, local time), with four digits for the year, where the tape wrote `DD-MON-YY`.
- **`OCT`.** A field of `OCT` is a word of 22 octal digits (the tape's, of 36 bits, was 12): a wider field is blank on the left, a narrower one
  zero filled, and a negative number prints as the 22 digits of its 64 bits. Write `-1`, not the constant of all the bits of a word.
- **Calls to C.** `CCALL NAME(A,B)` calls a C function with values;
  `EXTERNAL INT NAME(PTR);` declares the types of a C function, and
  `X IS EXTERNAL` a variable defined elsewhere; `impcallp` and `impcallw` call
  through an address. A `SUBR` is a C function taking pointers (`docs/FFI.md`);
  `CALLBACK INT CMP(PTR, PTR);` gives a `SUBR` a C signature, `CALLBACK(CMP)` is the
  address of a C function that calls it, and `FNPTR INT F(PTR);` declares the call
  of a C function through a pointer. `STRUCT TS (LONG SEC, LONG NSEC);` declares the
  layout of a C structure, `FIELD(P, TS.SEC)` is a field of the structure at the
  address `P`, and `SIZEOF(TS)` and `OFFSETOF(TS, NSEC)` are constants.
  These come from `lib/ffi.imc`, a language extension in the original's style.
- **Objects** are the host's object format, `NAME.o`.

`docs/LANGUAGE-NOTES.md` lists the points where programs written for the
original go wrong.
