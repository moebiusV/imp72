# The machine the code generator compiles to

The code generating routines of the tape (`code.imc`, `code2.imc`, `code3.imc`) generate PDP-10 instructions into free array
`CO`. The routines of `src/llvm-ir/` generate the operations of the machine this implementation compiles to: 64 bit
words, byte addresses, `double` for `REAL`, an unlimited supply of virtual registers (IR temporaries), and the C procedure
call standard. The operation numbers are the macros of `src/llvm-ir/irsyn.imc`; `src/llvm-ir/syntax64.imc` uses the
numbers themselves, because a semantics cannot name a macro.

The assembler, the register allocator and the object writer of the tape (`amod`, `rmod`, `omod`) have no counterpart:
`src/llvm-ir/demit.imp` walks the chains and writes IR (`docs/internals/code-generator.md`), and LLVM makes the object file.

The structure of the original is kept: the code of a stack entry is a chain in `CO`, `HOOK` joins chains, `ADDREC` extends
one (where the tape has `ADDCODE`), and `DEWOP(OP,R,S)` adds an operation with accumulator `R` and the operand `S`. The
rules of the language call `STORE`, `FETCH`, `ADDOP`, `CONOP` and the rest. The instruction is the machine's.

## The stack entry

Three words (`docs/porting/layouts.md`):

    word 0   bits 5-0 TY: 2 register, 4 constant, 8 identifier, 16 subscripted variable; bits 7-6 AR: 1 integer, 2 real, 3 byte;
             the left half DI, a directory index
    word 1   TY 4: the constant, a full 64 bit word (a REAL is the IEEE double's bits)
             TY 2, 8, 16: bit 63 the indirect bit, bits 32-42 the register (a temporary), the right half the offset (signed)
    word 2   the code: left half the first record of the chain, right half the last (0: no code)

## The records of a chain

A record is five words of `CO` (`RECW` in `irsyn.imc`):

    word 0   left half: the next record of the chain (0 ends it); right half: bits 7-0 KIND, bits 15-8 OP
    word 1   RD, a temporary (a register index), or 0
    word 2   V0 \  the operand: words 0 and 1 of a stack entry, as UNPCK reads them
    word 3   V1 /
    word 4   X, a second operand (a directory index of a tag, or a count)

KIND 1 is an operation, 2 a tag (`X` is its directory index), 3 a constant word of a `DATA` statement (`V1` is the value).

## The operations (OP, decimal)

`RD` is the accumulator (a temporary) and `V` the operand. Integers are 64 bit two's complement; `DIV` truncates toward zero
and `MOD` has the sign of the dividend.

    1 LOAD  RD = V          2 NEG  RD = -V          3 COMPL  RD = ~V          4 ADDR  RD = the address of V
    5 STORE V = RD
    6 ADD   7 SUB   8 MUL   9 DIV   10 MOD                                    RD = RD op V
    11 AND  12 ANDCA (~RD & V)  13 ANDCM (RD & ~V)  14 ANDCB (~RD & ~V)
    15 IOR  16 ORCA (~RD | V)   17 ORCM (RD | ~V)   18 ORCB (~RD | ~V)
    19 XOR  20 EQV (~(RD ^ V))
    21 LSH  RD shifted logically by the signed count V (left if positive; 64 places or more give 0)
    22 ASH  the same, arithmetic (a right shift keeps the sign)
    23 ROT  RD rotated left by V
    24 LDB  RD = the byte field V names         25 DPB  the low bits of RD into the field V names
    26 ILDB, 27 IDPB                            the same after stepping the byte pointer V
    28 ADDX RD = RD + 8*V (a word index added to a pointer)
    30 FADD  31 FSUB  32 FMUL  33 FDIV                                         RD = RD op V, on doubles
    34 FLOAT RD = (double) V (V an integer)     35 FIX  RD = (long) V (V a double, truncated)
    36 FNEG  RD = V with the sign bit flipped
    40 JUMP  to the tag named by V (a computed GO TO when V is a subscripted variable)
    41 JEQ  42 JLT  43 JLE  44 JGE  45 JGT  46 JNE                            jump to the tag X if RD cmp V (integers)
    51 .. 56 FJEQ .. FJNE                                                      the same on doubles
    60 CALLB begin a call of the function named by V      61 ARG the address of V is the next argument (X: the C type, below)
    62 CALLE end the call; RD receives its value (0: none)
    63 PITEM print list item: X is the format code, V its operand (0: none)
    64 CALLI  RD = impcalli(V, RD)                         65 XCT  do the machine instruction in the word V (impxct)
    66 HRLM, 67 HRRM  the low 32 bits of RD into the left (66) or the right (67) half of V
    68 REMOTE marks the code that follows as the remote code of the routine named by X
    70 ENTRY the subroutine named by V begins          71 RET return RD          72 PARAM RD receives the next pointer parameter

The X of an ARG record is 0 for an argument of an IMP routine or of a C function that is not declared. For a function declared with
`EXTERNAL` (`docs/FFI.md`) it is the number of the C type the argument is converted to (1 CHAR, 2 UCHAR, 3 SHORT, 4 USHORT, 5 INT, 6 UINT,
7 LONG, 8 ULONG, 9 LLONG, 10 ULLONG, 11 SIZE, 12 PTR, 13 DOUBLE, 14 FLOAT), plus 16 for the address of a text constant passed to a PTR.
The conversion of the value (`FLOAT`, `FIX`) is done by the semantics, in the records of the argument, and the declaration is in the
table CPR of `impsem` (`CPHEAD`, `CPTYPE`), which `ECALL` reads for the result.

The X of an ADDR record is 0 for the address of the operand and 1 for `CALLBACK(NAME)` (`docs/FFI.md`): the operand is then the name of a subroutine
declared a callback, and the address is that of the symbol `NAME@SIGNATURE`, a function with the C types of the declaration that calls the subroutine
(`ECBADDR` in `demit.imp`, `CBADDR` in `code3.imp`). The head in `CPR` of a declaration is the type of the result in bits 0 to 3, the number of
parameters in bits 4 to 8, 512 for a variadic function, 1024 for a callback and 2048 for a call through a pointer (the first parameter is then the
function, a PTR, and the call is named `impcallt@SIGNATURE`).

The operand `V` is read the way the entry says: TY 2 the register, TY 4 the constant (a text constant used as a value is
the word whose bytes are its first eight characters), TY 8 the word of the variable DI plus the offset, TY 16 the same
indexed by the register and indirect if bit 63 is set. The address of a variable is its byte address and the offset
counts words: with a variable `V`, an offset `O` and a register `R` it is `&V + 8*O + 8*R`; with a register and no
variable, `R + 8*O`, `R` holding a byte address; with neither, the absolute address `O`.

## Fields of structures

`FIELD(P, NAME.FIELD)` (`docs/FFI.md`, structure layouts) is an operand like a byte field: a variable named for a descriptor in the table
`FDESC` (eight words: the name, the operand of the address `P` in two words, the operand of the element index in two words or 0, the byte
offset, the C type 1 to 14, the size of an element). The operation LDB with that operand loads the field and DPB stores to it, each as one typed
memory access at `P + I*size + offset` in IR: a `UMUL` of the type of the memory (2 `CHAR`, 3 `UCHAR`, 4 `SHORT`, 5 `USHORT`, 6 `INT`, 8 `LONG`,
9 `ULONG`, 12 `FLOAT`, 13 `DOUBLE`) under a `PCONV` of the address. The register holds a word: a signed type is sign-extended, an unsigned type and
a pointer zero-extended, a `DOUBLE` the 8 bytes of the field, a `FLOAT` the double of the float. Where the pointers are 32 bits, `LONG` and
the pointer types are 4 byte `INT` accesses. The entry of the descriptor has the real bit set for `DOUBLE` and `FLOAT`, so the register of a
load is real.

A data word that holds an address is, where the pointers are 32 bits, two 32 bit halves in the global of the words (a packed structure of `i64`
and `{ i32, i32 }` members): the address in the low half of the word (the first member on a little endian target, the second on a big endian one) and 0 in the other, because a `ptrtoint` to `i64` is not
a relocatable constant on those targets.

## Instruction words

`EXECUTE A` runs the one operation of this machine that the word `A` holds: the operation number in bits 56-63 (1 to 23,
28 and 64 are done), the accumulator in 52-55, the index register in 48-51, bit 47 for an immediate operand, and the
address or number in the low 47 bits, signed. `impxct` in `lib/imp72rt.imp` does it with the registers `0R` to `15R` as the
accumulators; jumps and floating point operations are an error at run time. `XWD A,B` is `(A AND HMASK) LS 32 OR (B AND
HMASK)` and `IOWD A,B` is `XWD -A,B-1`.
