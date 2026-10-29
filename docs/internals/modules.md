# The kernel module by module

One section for each module of the compiler: what it does, the original it ports, the data it keeps, what differs from
the PDP-10 original, and where it stands (C in `stage0/`, or also IMP in `src/`). Each C file's header comment is the
detailed reference; `docs/porting/` has the notes of each module ported to IMP; `docs/internals/stage0-*.md` and
`codegen-and-translation.md` explain the groups. `ARCHITECTURE.md` has the map and `BOOTSTRAP.md` the route to the
compiler that compiles itself.

| Module | Original | C | IMP | Port notes |
|---|---|---|---|---|
| free | `decus/free.imc` | `stage0/free.c`, `free_imp.c` | `src/free.imp` | `porting/free.md` |
| dir | `dir.imc` | `dir.c` | `src/dir.imp` | `porting/dir.md` |
| stack | `stack.imc` | `stack.c` | `src/stack.imp` | `porting/stack.md` |
| lex | `lex.imc` | `lex.c` | `src/lex.imp` | `porting/lex.md` |
| graph | `graph.imc` | `graph.c` | `src/graph.imp` | `porting/graph.md` |
| parse | `parse.imc` | `parse.c` | `src/parse.imp` | `porting/parse.md` |
| ercor | `ercor.imc` | `ercor.c` | `src/ercor.imp` | `porting/ercor.md` |
| entree | `entree.imc` | `entree.c` | `src/entree.imp` | `porting/entree.md` |
| cotree | `cotree.imc` | `cotree.c` | `src/cotree.imp` | `porting/cotree.md` |
| dosem | `dosem.imc` | `dosem.c` | `src/dosem.imp` | `porting/dosem.md` |
| rsyn | `rsyn.imc` | `rsyn.c` | `src/rsyn.imp` | `porting/rsyn.md` |
| impsem | `impsem.imc` | `impsem.c`, `impsem_imp.c` | `src/impsem.imp` | `porting/impsem.md` |
| sem, codegen | `impsem.imc` (lists), `code*.imc` | `sem.c`, `codegen.c` | no | |
| trans, ir, pcode | `amod.imc`, `rmod.imc`, `omod.imc` (replaced) | `trans.c`, `ir.c`, `pcode.c` | no | |
| debug | `debug.imc` | `debug.c`, `debug_imp.c`, `pr.c` | `src/debug.imp` | `porting/debug.md` |
| pr, error | PMOD (part), `impm.imc` (ERROR, MSG) | `pr.c`, `error.c` | no | |
| driver | `impm.imc` | `imp72s0.c` | no | |

## free: free storage

All the tables of the compiler live in one growable block of words, `fmem`, as a doubly linked list of "arrays", each
with a four word header (links, name, increment and length, and the registration of a relocatable array). An array is
named by the index of its first data word. Arrays move when the block grows or is packed, so a pointer held elsewhere
goes stale: a relocatable array (FRELOT) has a registered variable that is updated after each move, and `fb` (the base
of `fmem`) is refreshed by the glue after any call that may allocate. Entry points: `FREE`, `FREES`, `FLEN`, `FNAME`,
`FADD`, `FALLOT`, `FRELOT`, `FINCSET`, `FADDEX`, `FCLOSE`, `FMAP`, `FSTATS`, `FTRACE`. Differences from `free.imc`: the
block is a `realloc`ed C heap, not the TOPS-10 low segment; a relocatable array's pointer location is a registry slot,
since a C address does not fit in a halfword; the 64000 word ceiling on a gap is gone. `IMP_FCHECK` checks the list
(`docs/porting/helpers.md`). Ported to IMP (the allocation routines; the diagnostics stay in C), `docs/porting/free.md`.

## dir: the directory of names

A hash coded directory: free array `PRP` is 512 buckets of three word nodes with overflow nodes after them, `ALF` holds
the names packed five characters to a word. A name's directory index is its node's index in `PRP`, the same number the
PDP-10 compiler computes (the hash is the original's, over the first five characters packed as they were). Entry points:
`DIR`, `DSEQ`, `PNAME`, `DNAME`, `GNAME`, `DSTATS`, `DSPILL`, `RADI50`, `CONVC`, `STCON`, `NEWNAME`, and the property
accessors (`DPROP`, `DPROPS`). The names keep the original packing (7 bit characters, 36 bits of a word): that is what
makes the dumps comparable with the oracle's. Ported to IMP.

## stack: the semantic stack, the pushdown stack, bit matrices

The semantic stack is three word entries in free array `STACK`; `STPUSH`, `STPOP`, `STACK(n)` return the free storage
index of an entry. `PUSHMI`/`PULLYU` are a plain pushdown stack. A bit matrix is a three word descriptor and one
relocatable array; `BITS`, `BIT`, `BITZ`, `BITOR` and `BSTATS` are its operations. Differences: the matrix packs 64 bits
to a word, not 36 (the dumps print the masks as the 36 bit original would, so the numbers agree). Ported to IMP, the
first module to be (the pattern for the others).

## lex: input and lexical analysis

A character at a time machine. A token is a run of letters and digits, an apostrophe string, `!text!`, or one other
non-blank character; blanks and control characters separate tokens and `#...#` is a comment. Entry points `LEXIN`,
`LEX`, `NEXTCHAR`, `PLINE1`, `PLINE`. Not ported: the TOPS-10 command line routines (`GETTY`, `READCMD`, …); the driver
takes arguments from `argv`. The file is read whole instead of by buffer. Ported to IMP.

## graph: the syntax graph

The syntax is a free array `GA` of two word nodes (alternate and successor links; type, row and id), plus the connectivity
bit matrices `BITM` (node n can lead to nonterminal nt) and `NBITM` (reached through circled or boxed nodes only). Entry
points `GRAPH` (add a production), `GMATR`, `GGOAL`, `GNEXT`, `GALT`, `GCONN`, and the node accessors `SCR`, `ALT`,
`GTYPE`, `GVAL`, `GNB`. The node word keeps the same three fields in 64 bits (type at bit 48, row at 32, id at 0) where the
original packs them in 36. Ported to IMP.

## parse: the Irons fast multi-track parser

Reads symbols with `LEX` and follows every parse still possible at once, as runs of words in free arrays `P` and `PP`
(the current and the next symbol's parses); the output (production numbers, and minus the directory index of a name or
symbol) is chained in `O`, newest first, and handed to `COTREE` whenever one parse remains. `PARSE`, `SYMBOL`, `PFORCE`,
`COPY`, `POUT`, `PSTATS`. Errors go to `ercor`. Ported to IMP.

## ercor: error correction and disambiguation

`ERCOR` finds the cheapest sequence of terminal symbols to insert before the symbol that killed every parse, searching the
syntax graph up to `MAXPATH` terminals deep; `AMBIG` and `ISNAME` decide between parses that are all alive. The step trace
of `/G` is reduced to its summary lines. Ported to IMP.

## entree: the code tree

The special cases of the semantics are kept in a tree in free array `CT`: a node matches an item of the pattern, an
identifier, a kind of stack item (the `WSVCRN` bits) or a production. `ENTREE` adds a pattern, `INSNO`/`COMPNO` keep the
alternates of a node in order of increasing generality, `MATCH` matches the stack against the tree. The stuff words keep
the original's 36 bit layout; the links word has three 20 bit fields (the original's 12 bit fields limited the tree to
4095 words). Ported to IMP.

## cotree: the semantic stack reducer

`COTREE(S)` adds one item of parser output to the code stack; for a production it tries a match between a path in the code
tree and the stack from each position on, and a match is copied to the semantic stack and given to `DOSEM`, whose result
replaces what matched. `CODENT`, `CFUDGE`, `MAXWELL`/`MAXEND` (the quoted semantics collector), `SETPRI`, `SAMEAS`,
`PSTACK`. The stack entries keep their 36 bit words; the code attached to an entry (CB, CE) is a chain of PDP-10
instruction words in free array `CO`. Ported to IMP.

## dosem: the semantics array and its interpreter

The semantics of a production is a run of words in `SEMS` (value, argument count, and a type: call a routine, push a
parameter, enter a node, `CASE` constants, …); `PROSEM` records where in the code tree each production can match.
`DOSEM` interprets one; `SETSEM`, `SEMFIX`, `SEMFX1`, `DOCASE`, `ENPROD`, `MATCHP`, `RESETSEM` and the rest build and
query it. The words keep the original layout. Semantic routines are C functions reached through the table of `sem.c`
(the original jumps to the routine's address). Ported to IMP.

## rsyn: the bootstrap reader

Reads, with `LEX` alone, the part of `decus/syntax.imc` up to the first `%%`: statements `<CLASS> ::= elements ::=
semantics` in a tiny notation, making the graph and the semantics array, exactly as the full compiler does once the
"syntax for syntax" is in. Its routines (`SYNT`, `SYTRM`, `SEMP`, `SEMR`, `QUOSEM`, `SYNTAX`, `REPVAL`, `VALU`, …) are
semantic routines too, registered by name, because the bootstrapped syntax calls them for the rest of the file. Ported to IMP.

## sem, impsem, codegen: the semantic routines

`impsem` is ported to IMP (`docs/porting/impsem.md`); `sem.c` and `codegen.c` are C.

`sem.c` is the table of routines (`dsem(name, fn)`; a routine's id is stored as the `SEM` property of its name) and the
list routines `NEWLIST`, `ENLIST`, `GETLIST`, plus `ASSEMC`, which hands a finished program to the translator.
`impsem.c` has the machine independent routines declarations need (`NAME`, `AREG`, `AREG1`, `DECL`, `DECLARE`, `VAL`,
`SVAL`, `STACKUP`, `SWITCH`, `NSUBP`) and `codegen.c` the code generating ones (`DEWOP`, `ADDOP`, `FETCH`, `STORE`,
`SUBSCRIPT`, `SUBCALL`, `CONOP`, …). They are the original's, kept in its design: they make PDP-10 code as a chain of
instruction words (instruction, constant, link, special function) with virtual registers `R65`…, and constant-fold in 64
bits. The registers are those of the original's array `RG`. Not ported: the PDP-10 assembler, register allocator and
object module (below). These files are the modern-platform port; a 36 bit PDP-10 build would be a separate thing.

## trans, ir, pcode: from code chains to IR

`trans.c` reads each code chain as a PDP-10 program over virtual registers and writes IR (`ir.c` writes the text),
one function at a time; the meaning of an instruction is the PDP-10's with the changes for a 64 bit machine with byte
addresses (`docs/internals/codegen-and-translation.md`). It replaces `amod`, `rmod` and `omod`. `pcode.c` lists a chain as
PDP-10 assembly (`-s A`), standing in for `PCODE1` and `ALIST`.

## debug, pr, error: services

`debug.c` ports the diagnostic print-outs (`GPRIN1`, `PARTR1`, the table dumps) so that they match the oracle's, and
`src/debug.imp` does so with PRINT, as the original (`docs/porting/debug.md`);
`pr.c` is the part of PMOD (the formatted print module, `docs/internals/pmod.md`) the C version uses; `error.c` is `ERROR` and `MSG`
of the driver: `N<=1` sets the error flag (which lists the source from the first error on), `N=0` abandons the compilation.

## driver: imp72s0.c

Two programs in one binary, by its name: `imp72s0` (the kernel: read the bootstrap syntax with RSYN, then compile files;
`-s` switches, `-c` IR) and `imp72stage0`/`imp72` (the compiler: syntax and `ffi.imc`, then LLVM in the process
to an executable, an object or LLVM IR). It finds its parts relative to itself, in the tree or installed.
