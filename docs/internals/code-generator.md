# The code generator

The code generator turns the semantics of `src/llvm-ir/syntax64.imc` into IR. It exists twice, as the same program in two
languages: in IMP in `src/llvm-ir/` (part of the compiler `imp72`), and in C in `stage0/gen_*.c` (part of the seed
`imp72stage0`, `stage0-semantics.md`). Both write the same IR byte for byte for the same program;
`tests/seed/run.sh` compares them for the native tests, the oracle programs, the runtime library and the modules of the
code generator itself.

    source.imp
      lex, parse, cotree, dosem                  the kernel (src/*.imp; stage0/*.c)
      semantic routines                          impsem, code, code2, code3: a chain of records in free array CO
      demit                                      the chain of a unit into IR trees
      IR text                                    ix.imp (the seed: stage0/ir.c)
      ir2ll                                      LLVM IR text (src/ir2ll.imp, src/ir2llx.imp; the seed: tools/ir2ll.c)
      LLVM, in the process                       an object file, or ORC and a run (src/llvm.imp; the seed: stage0/llvmorc.c)
      lld, in the process                        the executable, linked with libimp72.a

## Files

| IMP | C (seed) | contents |
|---|---|---|
| `src/llvm-ir/impsem.imp` | `stage0/gen_impsem.c` | registers (`RG`), subroutines, declarations, lists, `NAME`, `FCON`, `ASSEMC` |
| `src/llvm-ir/code.imp` | `stage0/gen_code.c` | `UNPCK`, `FETCH`, `DEWOP`, `ADDOP`, `STORE`, `COPYCO`, `MCODE`, `SUBARG` |
| `src/llvm-ir/code2.imp` | `stage0/gen_code2.c` | `ADDREC`, `HOOK`, `CONOP`, `SUBRCALL`, `FLOAT`, `FIX`, `DATAST`, `PRINPAR`, jumps |
| `src/llvm-ir/code3.imp` | `stage0/gen_code3.c` | `SUBCALL`, `SUBSCRIPT`, `BYTEP`, `PCODE` |
| `src/llvm-ir/demit.imp` | `stage0/gen_demit.c` | `DEMIT`: records to IR trees |
| `src/llvm-ir/ix.imp` | `stage0/ir.c` and the `IR...` helpers of `gen_demit.c` | tree builders, IR text, names of directory entries |
| `src/llvm-ir/irsyn.imc` | `stage0/gen.h` | macros for the records and operations; the IR operator and type codes |
| `src/llvm-ir/syntax64.imc` | the same file | the syntax and semantics of the language |

`stage0/gen.h` has the table of IMP names against C names (`g_` for the semantic routines, `ir_` for the builders). The C
port keeps the IMP evaluation order where it has effects (a register's first use takes the next temporary, a shift
writes statements).

## The semantic routines

`syntax64.imc` is the tape's `syntax.imc` with the operations of the machine of `docs/porting/llvm-ir-machine.md`. Its
semantics call `DEWOP`, `ADDOP`, `FETCH`, `STORE`, `HOOK`, `SUBCALL` and the rest with operation numbers. Each stack
entry carries its code as a chain of five word records in free array `CO`: `DEWOP(op,r,s)` adds an operation with
accumulator `r` and the operand `s`, `ADDREC` adds any record, `HOOK(d,a,b)` joins the chain of `a` to that of `b` and
puts it in `d`. A register is an index into the array `RG`; `AREG1` makes a new one, and `DEMIT` gives each its own IR
temporary. `FETCH` loads an operand into a register; `SUBARG` gives an argument an address. The routines are entered
with `DSEM` by `IMPSEM`, `CODEI`, `CODE2I` and `CODE3I` (`gen_init` in the seed).

`ASSEMC(a)` ends a compilation unit: it lists the chain of `a` with `PCODE` if the switch `A` is set and calls
`DEMIT(a)`.

Register declarations (`X IS REGISTER`) make a temporary (`DECLARE`). `REMOTE` code is kept in the chain `REMO` and
emitted with the routine it belongs to.

## DEMIT

`DEMIT` reads the chain of a unit and writes one function for the main program (`impmain`) and one for each subroutine,
each starting at an `ENTRY` record. In a subroutine, an `OPARAM` record gives the temporary that receives each pointer
parameter (`IXPARAM(k)`). A `RET` record stores the value in a temporary and jumps to the label at the end, where the
function loads it into the result register.

- **Names.** A symbol is the name as written; characters other than letters, digits and `_` become `_XX` (two
  hexadecimal digits), so `PRN.` is `PRN_2e`. IMP names are case sensitive, so a C function is called by its C name
  (`docs/FFI.md`).
- **Variables.** A variable that the unit uses is `.comm name,8*n,8` (n is the length of an `IS n LONG` array, 1
  otherwise), `.local` if it is not `COMMON`, and not defined at all if it is `EXTERNAL`. Floating scratch words
  `imp_fs0`, `imp_fs1`, `imp_fsr` and shift words `imp_sa`, `imp_sb`, `imp_sr` are `.comm` when used.
- **Constants.** A text or number used as an address is pooled: a label `.Ln` in `.data` followed by the string (as
  `.string`) or the number (as `.quad`). A text constant of up to eight characters used as a value is the word whose
  bytes in memory are the characters.
- **`DATA`.** A tag followed by constant words becomes initialised data named for the tag, one `.quad` a word.
- **Addresses.** The address of a variable is its byte address, and the offset of an operand counts words. An operand
  with a variable, an offset `O` and an index register `R` is at `&V + 8*O + 8*R` (`V[I]` is `&V + 8*I`); with a register
  and no variable, at `R + 8*O`, where `R` holds a byte address (`[P+3]` is `P+24`); with neither, at the absolute address
  `O`. An indirect operand reads the word at the address and takes it as a pointer.
- **Calls.** IMP passes arguments by reference: a call is `CALLB` (the function), one `ARG` record per argument (the
  address of the value; a constant is pooled, a register is stored in a temporary word), and `CALLE`, which gives the
  value to a register. A subroutine is a function whose parameters are those pointers and which returns a word. A
  function called with `CCALL` or declared `EXTERNAL` is called by value: the arguments are the values, not the addresses. Up to
  16 arguments; those beyond the registers of the machine go in the outgoing stack area (`FUNARG` nodes pushed by pass 2
  on `i386`).
- **Typed calls.** `EXTERNAL INT puts(PTR);` (`CDECL`, from `lib/ffi.imc`) enters the types of the function in the table `CPR`
  of `impsem.imp` (four words a name: the name, the head, the types of the parameters; `CPHEAD`, `CPTYPE`), which `ASSEMC`
  empties at the end of the unit. A name that `CCALL` calls without a declaration is entered with the head 0, so that a
  declaration after the call is an error. `CSUBCALL` (and `CCALL0` for a call with no arguments) counts the arguments and
  `CPAR` converts each to the declared type before it is stored (`FLOAT` for a `DOUBLE` or `FLOAT`, `FIX` for a real given to an
  integer type); the type is the X of its `ARG` record. A declared `DOUBLE` or `FLOAT` function gets the property `ATYPE` real,
  so the value of its call is a real. `DEMIT` (`ECALLT`) names the function `NAME@SIGNATURE` in the IR: the letter of the result type,
  then one letter for each argument (`c C s S i I l L q Q z p d f v`), with a dot before the arguments of a variadic call that have no
  declared type. The arguments are words; `DOUBLE`, `FLOAT`, `LLONG` and `ULLONG` ones are stored in the words `imp_cw0` to
  `imp_cw15` and the address of the word is the argument. The call has the result code 2 of `IXCALL`: type long on every machine,
  where an undeclared C call has the type int on the machines with 32-bit words. The IR writers read the signature, make the LLVM
  call with the types (`signext` and `zeroext` on the parameters narrower than a word), and convert the result to the word.
  `docs/FFI.md` has the rules of the conversions.
- **`GO TO` out of a routine.** A tag of another routine is reached by `longjmp`: the routine that has the tag calls
  `_setjmp` into `imp_jb_<tag>` (a 512 byte `.comm`) at its entry, and the jumping routine calls `_longjmp` on it. The
  routine with the tag has to be running. A tag that no routine has is an error that names the tag.
- **Computed `GO TO (L0,L1,...) N`.** The jumps after the table tag become one compare and branch for each.
- **`PRINT` and `READ`.** The syntax builds a call of `PRN.` or `RED.` with one `PITEM` record per item. Each item
  stores its code and the address of its operand in the cells `impca` and calls `imppr(&impca[0], &impca[1])` (`imprd`
  for `READ`); the library in `lib/imp72io.imp` holds the format between items. `pmod.md` describes the formats.
- **`CALLI(a,b)`** calls `impcalli` (`lib/imp72rt.imp`: exit, date, time of day, run time, job number).
- **`EXECUTE a`** calls `impxct`, which does the one operation of the machine that the word holds, with the registers
  `0R` to `15R` stored in `impac` before and loaded after. Floating operations and jumps are an error at run time.
- **`MOVE B THROUGH N TO A`** calls `impblk(A,B,N)`, which copies N+1 words upward, a word at a time.

## Byte fields

`BYTEP A<S,P>` makes a descriptor in the table `BDESC` (seven words: the name, the operand `A`, the size `S`, the
position `P`). A byte field read or written through a descriptor is a shift and a mask of the word, with `S` and `P`
constants or variables (`ELDB`, `EDPB`, `FMASK`).

A byte pointer kept in a variable or a register is one word. A pointer to characters (`A<7,36>`, also `A<8,64>`) is the
byte address of the byte before the first of `A`; `<+P>` adds 1 and then reads or writes the byte there, and a
character is a byte. Any other field keeps the layout of the PDP-10 pointer in one word: bits 63-58 the size, 57-52 the
position, the low 52 bits the byte address of the word. The two are told apart by the size bits, which a byte address
never has. Stepping a field pointer moves to the next lower field of the same size, or to the next word (address + 8)
when there is no room. `impibp`, `impldb` and `impdpb` in `lib/imp72rt.imp` do `IBP`, `LDB` and `DPB`. When the
environment variable `IMP72CHARPTR` is set the compiler does the stepping, loading and storing of character pointers in
line; the runtime library is compiled so, because those routines are made of such pointers.

## Shifts

`LS`, `RS`, `ALS` (`ASH`) and `LROT` (`ROT`) by a constant count are done in line. A shift by 64 places or more gives 0,
except that an arithmetic right shift of that size gives the sign; a negative count shifts the other way. A variable count calls `implsh`, `impash` or `improt` with the addresses of the words
`imp_sa`, `imp_sb` and `imp_sr`, because a call cannot carry a 64 bit value on a 32 bit machine. The shift is a node of
the IR that takes a temporary and a count assigned back to the same temporary, so every shift goes through a fresh
temporary (`IXSHIFT`).

## Reals

A `REAL` is the bits of an IEEE 754 binary64 in a word. `FCON` makes a constant from its digits with the C library
(`sscanf`). `FLOAT` and `FIX` convert (`FIX` of a constant is done by the compiler). `FADD`, `FSUB`, `FMUL` and `FDIV`
store both words in the scratch words `imp_fs0` and `imp_fs1`, read them as doubles where they are used, and store the
result in `imp_fsr` and read it back as a word; no double is held in a temporary. A comparison of a real with anything
uses the floating jumps (`FJEQ` to `FJNE`, the integer jumps plus 10). `CONOP` folds integer constants in 64 bits and
negates a real constant by flipping its sign bit; any other operation on a real constant is an error. `lib/imp72.h`
converts between a `REAL` word and a `double` for C.

## The IR

The IR is the text of a tree language with the operators and types of the pass 2 of pcc (`stage0/ir.h`). It is lowered
for one machine (`amd64` by default, also `aarch64`, `i386`, `arm`; `-T` of the kernel, `--target` of the driver): the
arguments of a call in the registers and the outgoing stack area of that machine's procedure call standard, the result
in the return register. A function is

    * assembler directive              .text, .globl, .type, data
    ! type flags prolab tmpbase lblbase name
    ^ label
    " op type qual [value name | 0 num | rval]       one tree node a line, preorder
    % autos retlab tmpend lblend name

`ir2ll` reads it and writes a module of LLVM IR; the lowering is undone there, so any host with LLVM can run the
compiler for any target LLVM has. A word is `i64`, a real a `double` moved through the scratch words, an address an `i64`
made a pointer where it is dereferenced. `ir2ll` is `src/ir2ll.imp` with `src/ir2llx.imp` (two files, because the code
generator can name only 2047 registers in one compilation), and `tools/ir2ll.c` in the seed. `tests/direct/ir2ll.sh`
and `tools/ir2ll-fuzz.py` compare the two. A C function called with `CCALL` and not declared is marked by the line
`* # cfunc NAME`: it returns a C long (an `int` on the 32-bit machines), and its arguments are pointer-sized integers. A declared one
needs no line: the call names it `NAME@SIGNATURE` (typed calls, above).
