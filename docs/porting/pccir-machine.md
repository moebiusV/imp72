# The machine the code generator compiles to: pcc IR

The code generating routines of the tape (`code.imc`, `code2.imc`, `code3.imc`) generate PDP-10 instructions into free array `CO`.
The translation in `src/pccir/` generates the operations of the machine this port compiles to, which is the intermediate representation
of pcc (`stage0/ir.h`): 64 bit words, byte addresses, `double` for REAL, an unlimited supply of virtual registers (IR temporaries), and
the C procedure call standard. `amod`, `rmod` and `omod` (the assembler, the register allocator and the object writer) are not
translated: `src/pccir/demit.imp` walks the chains and calls `ir.c`, and pcc makes the object file for the machine at hand.

The structure of the original is kept: the code of a stack entry is a chain in `CO`, `HOOK` joins chains, `ADDCODE` extends one,
`DEWOP(OP,R,S)` adds an instruction with accumulator R and the operand S, and the same routines (`STORE`, `FETCH`, `ADDOP`, `CONOP`, ...)
are called by the rules of the language (`src/syntax64.imc`, a translation of `decus/syntax.imc`). What changes is the instruction.

## The stack entry

Unchanged (the parser, the code tree and the semantic stack are the tape's): three words,

    word 0   bits 5-0 TY: 2 register, 4 constant, 8 identifier, 16 subscripted variable; bits 7-6 AR: 1 integer, 2 real, 3 byte;
             bits 35-18 DI, a directory index (36 bit layout, as before: L18, SX36)
    word 1   TY 4: the constant, a full 64 bit word (a REAL is the IEEE double's bits)
             TY 2, 8, 16: bit 35 the indirect bit, bits 28-18 the register (a temporary), bits 17-0 the offset in words
    word 2   the code: left half the first record of the chain, right half the last (32 bit halves; 0: no code)

## The records of a chain

A record is five words of `CO`:

    word 0   left half: the next record of the chain (0 ends it); right half: bits 7-0 KIND, bits 15-8 OP
    word 1   RD, a temporary (a register index), or 0
    word 2   V0 \  the operand: words 0 and 1 of a stack entry, as `UNPCK` reads them
    word 3   V1 /
    word 4   X, a second operand (a directory index of a tag, or a count)

KIND 1 is an operation, 2 a tag (`X` = its directory index), 3 a constant word of a DATA statement (`V1` is the value).

## The operations (OP)

`RD` is the accumulator (a temporary), `V` the operand. Integers are 64 bit two's complement; `DIV` truncates toward zero.

    1 LOAD  RD = V          2 NEG  RD = -V          3 COMPL  RD = ~V         4 ADDR  RD = the address of V
    5 STORE V = RD
    6 ADD   7 SUB   8 MUL   9 DIV   10 MOD                  RD = RD op V
    11 AND  12 ANDCA (~RD & V)  13 ANDCM (RD & ~V)  14 ANDCB (~RD & ~V)
    15 IOR  16 ORCA (~RD | V)   17 ORCM (RD | ~V)   18 ORCB (~RD | ~V)
    19 XOR  20 EQV (~(RD ^ V))
    28 ADDX RD = RD + 8*V (a word index added to a pointer)
    21 LSH  RD shifted logically by the signed count V (left if positive; a count of 64 or more gives 0)
    22 ASH  the same, arithmetic (right shifts keep the sign)
    23 ROT  rotated left by V (64 bit)
    30 FADD  31 FSUB  32 FMUL  33 FDIV                      RD = RD op V, on doubles
    34 FLOAT RD = (double) V (V an integer)     35 FIX  RD = (long) V (V a double)
    40 JUMP  to the tag named by V
    41 JEQ  42 JLT  43 JLE  44 JGE  45 JGT  46 JNE          jump to the tag X if RD cmp V (integers)
    51 .. 56 FJEQ .. FJNE                                    the same on doubles
    60 CALLB begin a call of the function named by V        61 ARG the address of V is the next argument
    62 CALLE end the call; RD receives its value (0: none)  63 PITEM print list item: X = the format code, V its operand
    64 CALLI  RD = imp_calli(V, RD) (the monitor call V with the AC RD)    65 XCT    do the PDP-10 instruction in the word V (imp_xct); 66 HRLM, 67 HRRM  the halfword stores of impsyn.imc
    70 ENTRY the subroutine named by V begins (its formal parameters: X of them)       71 RET return RD

The operand V is read the way the entry says: TY 2 the register, TY 4 the constant, TY 8 the word of the variable DI plus the offset,
TY 16 the same indexed by the register and indirect if bit 35 is set. The address of a variable is its byte address; the offset and the
index register count words when there is a variable (`V[I]` is `&V + 8*I`) and bytes when there is none (`[P+3]` is `P+3` bytes).

## Original modules

Every module of decus/ compiles with the pcc IR compiler. REMOTE code is emitted with its routine (ops 66 HRLM, 67 HRRM map HRL/HRR of impsyn.imc; 68 marks remote code).

A GO TO to a tag of another routine (ERROR of impm.imc goes to FIN of the main program) is a longjmp: the routine that has the tag takes a `_setjmp` into `imp_jb_<tag>` at its entry, and the jumping routine calls `longjmp` on it. Like the PDP-10 jump it does not return, and it needs the routine with the tag to be still running (the main program is). A GO TO to a tag that no routine has is an error naming the tag.
