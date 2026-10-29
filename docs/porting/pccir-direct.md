# Direct IR: the semantics emit IR, not PDP-10 code

Status, 2026-10-23: built and verified (`tools/direct.sh`, `tests/direct/`). `src/pccir/` is the translation of the tape's `code*.imc`, `impsem.imc` and
`syntax.imc` with the operations of IR in place of the PDP-10's, and `demit.imp` takes the place of `amod`/`rmod`/`omod`. The old path
(`codegen.c` -> PDP-10 chains -> `trans.c` -> IR) is still in the tree, as the seed that compiles `src/pccir` the first time and as the reference the
new path is compared with; what it takes to remove it is at the end of `docs/porting/pccir-path.md`.

## Why

Today `syntax.imc` calls `DEWOP(254B,...)`, `ADDOP(164B,...)`, `CONOP(...,1000010B+...)` and the like. The routines of `codegen.c` build
PDP-10 instruction words (opcode, AC, index register, address, immediate/self/both modes, halfword moves) in code chains, and `trans.c`
(1776 lines) reads each chain as a PDP-10 program and writes IR. Every 36-vs-64 bit difference is a special case in that translator
(halves are 32 bits, `AND NOT 777777B` is a halfword move, an integer compare or negation of a real is marked by `dewop` (`REALOP`) and made floating point,
immediate floating operands are untranslated, byte pointers are emulated). None of that is wanted: LLVM already does register allocation,
immediates, peepholes and instruction selection.

## What is kept

- The parser, graph, code tree, `dosem`, `rsyn`, `entree`, `cotree` (all IMP already): the extensible-syntax machinery, the stack, the
  pattern matcher (`"A0CON AND B0CON" => ...`), `CASE ... OF`, `LOCAL ... IN "..."`, the `EQ/` relational dispatch.
- The stack entry layout: word 0 = type (2 register, 4 constant, 10 variable, 20 memory) | arithmetic type | directory index,
  word 1 = register / constant, word 2 = code. The pattern predicates (`0CON`, `0REG`, `0IGR`, `0FLT`, `0VAR`, `0MEM`) are about this
  layout and keep meaning what they mean. (`0CON18` has no meaning without immediates: patterns that test it are dropped.)
- `src/impsyn64.imc` and the module sources.
- `ir.c` (the IR text writer, 315 lines), then the only C that stayed for code generation. IMP calls it through `CCALL` (a pointer is a
  64 bit word). Since then `src/pccir/ix.imp` has taken its place in `imp72`; the seed keeps `ir.c`.

## What changes

1. **A new language definition**, `src/syntax64.imc`, loaded in place of `decus/syntax.imc` by the new path (`-D` of the driver; `decus/` is never
   edited). It has the same rules and priorities; only the semantic expressions change:
   - the opcodes are symbolic: `ADDOP(ADD,A,B)`, `DEWOP(NEG,...)`, `CASE(SHL,...)`, with names a table maps to IR operators; no mode
     bits (immediate, memory, both), no `AND NOT 777777B`, `HRRZ`, `HLL`;
   - the relational statements make one conditional branch (`BRANCH(cond,P,R,LABEL)`), not `CAMx`/`SKIPx`/`AOJx` chains;
   - byte fields `A<s,p>` are shift and mask (or sized loads and stores for `<8,...>`), not `LDB`/`DPB`/`ILDB`;
   - `REAL` is a `double` in the word's 64 bits (the bits of the double). No encoding. Real constants are `FCON`'s double;
   - constants are 64 bit.
2. **A new code generator in IMP**, `src/codegen.imp`: the routines `DEWOP ADDOP FETCH STORE CONOP SUBSCRIPT SUBRCALL ... FIX FLOAT`
   build IR statement records, not instruction words. The code of a stack entry (word 2) is a chain of IR records in a free array `IC`, linked as
   `CO` is; `HOOK` is still a join of two chains. A record is a statement: `ASSIGN temp, expr`, `STORE addr, expr`, `LABEL`, `JUMP`,
   `BRANCH`, `CALL`; an expression is a small tree in the same array.
3. **Registers are IR temporaries.** `AREG1` makes a new temporary number; the `REGISTER`, `SCRATCH`, `PROTECTED`, `RESERVED`, `AVAILABLE`,
   `RELEASED` declarations (a PDP-10 register discipline) become declarations of named temporaries with the semantics they have for a
   user (`X IS REGISTER` is a local temporary; the others are accepted and ignored).
4. **The emitter** walks the chain of a finished subroutine or program at `ASSEMC`/`SUBEND` and calls `ir_func_begin`, `ir_stmt`,
   `ir_label`, ... from IMP (through `CCALL`). `trans.c`, `pcode.c`, `codegen.c`, `impglue`'s code-generator entries and the C `sem.c`
   dispatch of those routines go away. What remains in C: the runtime library, the driver, the table of semantic routines (a table of
   function pointers), `ir.c`.

## Calling convention and data

Unchanged from `trans.c` (documented in `docs/internals/codegen-and-translation.md`): the arguments after a call are pointers to the
values (IMP passes by reference); a subroutine is a C function taking them as pointers; a variable is its byte address; `V[I]` is at
`&V + 8*I`; FFI by-value calls (`CCALL`) pass values. The direct path must reproduce these exactly, so the same native tests, FFI tests
and oracle goldens apply to both paths.

## Verification

- `tests/direct/run.sh`: every program of `tests/native`, `tests/oracle` and the self-hosting modules goes through both paths and the
  outputs are compared (the old path is the reference until it is removed). Dumps of the IR are not compared: the IR is allowed to be
  better.
- The stage 1 / stage 2 tests: the compiler's own modules compile with the new path, and stage 2 reproduces stage 1.
- The IEEE tests (`tests/ieee`) run with no real encoding.

## Order of work

1. **Spike (smallest vertical slice):** `X_5; PRINT X` and integer arithmetic, assignments, comparisons and `GO TO`, with `syntax64.imc` holding
   only these rules, `codegen.imp` only their routines, the emitter, and the driver switch. Run `hello` and `basic`.
2. Subroutines, parameters, arrays and `SUBSCRIPT`, `DATA`, `REMOTE`, `COMMON`.
3. `REAL`: constants, `FIX`/`FLOAT`, arithmetic, compares, `PRINT FLT`.
4. Byte fields, `BYTEP`, `<+P>`, `LOC`, `MOVE ... THROUGH`, `READ`/`PRINT` items (`PRINPAR`).
5. FFI (`CCALL`), `EXECUTE`/`CALLI` (rejected cleanly), the remaining rules.
6. Compile the compiler's own modules with it; stage 1 / stage 2 fixed point on the new path; switch the default; delete the old path.
