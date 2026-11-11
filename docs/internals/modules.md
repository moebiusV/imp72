# The kernel module by module

One section for each module of the compiler: what it does, the original it comes from, the data it keeps, and what
differs from the PDP-10 original. The modules exist in IMP in `src/` (the compiler `imp`) and in C in `stage0/` (the
seed). Each file's header comment is the detailed reference. `docs/porting/modules.md` lists the differences of the IMP
modules from the tape and `docs/porting/layouts.md` the layout of the words. `stage0-*.md` describe the C modules,
`code-generator.md` the code generating routines, `docs/ARCHITECTURE.md` the map and `docs/BOOTSTRAP.md` the route to
the compiler that compiles itself.

| Module | Original | C (seed) | IMP |
|---|---|---|---|
| free | `decus/free.imc` | `stage0/free.c` | `src/free.imc` |
| dir | `dir.imc` | `dir.c`, `semtab.c` | `src/dir.imc` |
| stack | `stack.imc` | `stack.c` | `src/stack.imc` |
| lex | `lex.imc` | `lex.c` | `src/lex.imc` |
| graph | `graph.imc` | `graph.c` | `src/graph.imc` |
| parse | `parse.imc` | `parse.c` | `src/parse.imc` |
| ercor | `ercor.imc` | `ercor.c` | `src/ercor.imc` |
| entree | `entree.imc` | `entree.c` | `src/entree.imc` |
| cotree | `cotree.imc` | `cotree.c` | `src/cotree.imc` |
| dosem | `dosem.imc` | `dosem.c` | `src/dosem.imc` |
| rsyn | `rsyn.imc` | `rsyn.c` | `src/rsyn.imc` |
| debug | `debug.imc` | `debug.c`, `pr.c` | `src/debug.imc` |
| impsem, code, code2, code3 | `impsem.imc`, `code.imc`, `code2.imc`, `code3.imc` | `gen_impsem.c`, `gen_code.c`, `gen_code2.c`, `gen_code3.c` | `src/impsem.imc`, `code.imc`, `code2.imc`, `code3.imc` |
| demit | `amod.imc`, `rmod.imc`, `omod.imc` (not translated) | `gen_demit.c`, `ir.c` | `src/demit.imc`, `ix.imc` |
| driver | `impm.imc` | `imps0.c`, `error.c` | `src/impm.imc`, `src/driver.imc` |
| back end | none | `llvmorc.c`, `impld.cpp`, `imphost.c`, `tools/ir2ll.c` | `src/llvm.imc`, `src/ir2ll.imc`, `src/ir2llx.imc`, `lib/imphost.iml` |
| image | none (the monitor's `SSAVE`) | none | `src/image.imc` |

## free: free storage

All the tables of the compiler live in one block of words, a doubly linked list of arrays, each with a four word header
(links, name, increment and length, registration of a relocatable array). An array is named by the index of its first
data word. Arrays move when the block is packed or an array is resized, so a pointer held elsewhere goes stale: a
relocatable array (`FRELOT`) has a registered variable that is updated after each move. Entry points: `FREE`, `FREES`,
`FLEN`, `FNAME`, `FADD`, `FALLOT`, `FRELOT`, `FINCSET`, `FADDEX`, `FCLOSE`, `FMAP`, `FSTATS`, `FTRACE`.

Differences from `free.imc`: a relocatable array's pointer location is a registry slot, because a C address does not fit
in a half word; there is no 64000 word ceiling on a gap; a named array is allocated with `FNEW(name,n)` in IMP (`fallot_new` in C),
because a name of eight characters in a 64 bit word cannot be told from an index.

The seed's block is a `realloc`ed C heap. In the IMP module the block is an arena of reserved address space at `FB`
that `CORE` makes usable with `mprotect` (`lib/impsys.iml`), so the arena never moves and a byte pointer into an array
stays good until the array itself moves.

Everything the modules keep of the arena is an index from `FB`, except five byte pointers of the lexer (`lex:PC`, `QC`,
`K0`, `K`, `PC0`), which step through the line and token arrays. The arena lies at the address of `SCHINT`, or at the
one in `IMPCOREBASE` (a test), and an image saved by `src/image.imc` is restored with it at any address: the loader
adds the difference of the bases to those five variables and sets `FB`. The census of the absolute addresses is in the
header of `src/image.imc`; `IMPIMGCENSUS=1` prints it at the saving. The seed has no images.

## dir: the directory of names

A hash coded directory: free array `PRP` is 512 buckets of three word nodes with overflow nodes after them; `ALF` holds
the names, eight characters (bytes) to a word, the first in the low byte. A name's directory index is its node's index
in `PRP`. Entry points: `DIR`, `DSEQ`, `PNAME`, `DNAME`, `GNAME`, `DSTATS`, `DSPILL`, `RADI50`, `CONVC`, `STCON`,
`NEWNAME`, and the property accessors `DPROP` and `DPROPS`.

`src/dir.imc` also holds the table of semantic routines: `DSEM(name,routine)` enters a routine and stores its id (1 plus
its slot) as the `SEM` property of the name, `SEMCALL(id,pars)` calls it with a copy of the parameters on a stack of its
own, and `SEMCOUNT`, `SEMNAME`, `SEMREMAP` give the table to the image. In the seed the table is `stage0/semtab.c`.

## stack: the semantic stack, the pushdown stack, bit matrices

The semantic stack is three word entries in free array `STACK`; `STPUSH`, `STPOP`, `STACK(n)` return free storage
indices. `PUSHMI` and `PULLYU` are a plain pushdown stack. A bit matrix is a three word descriptor and one relocatable
array; `BITS`, `BIT`, `BITZ`, `BITOR` and `BSTATS` are its operations. The matrix packs 64 bits to a word.

## lex: input and lexical analysis

A character at a time machine. A token is a run of letters and digits (and `_`), an apostrophe string, `!text!`, or one
other non-blank character; blanks and control characters separate tokens and `#...#` is a comment. `:=` is one token.
Entry points `LEXIN`, `LEX`, `NEXTCHAR`, `PLINE1`, `PLINE`.

In IMP the line and the token are packed arrays stepped with byte pointers, as in the original, and bytes of 128 and up
are letters, so that a name can be in UTF-8. `src/lex.imc` also has `GETTY`, `READCMD`, `PUTN`, `PUTO`, `PUTSW`,
`LEXSAV`, `LEXRES` for the commands of `imp -i`. The seed's lexer reads the whole file into a buffer, takes ASCII
letters only, and has none of the command line routines.

## graph: the syntax graph

The syntax is a free array `GA` of two word nodes (alternate and successor links; type, row and id), plus the
connectivity bit matrices `BITM` (node n can lead to nonterminal nt) and `NBITM` (reached through circled or boxed nodes
only). Entry points `GRAPH` (add a production), `GMATR`, `GGOAL`, `GNEXT`, `GALT`, `GCONN`, and the node accessors
`SCR`, `ALT`, `GTYPE`, `GVAL`, `GNB`. The node word keeps the three fields in 64 bits: type at bit 48, row at 32, id
at 0.

## parse: the Irons fast multi-track parser

Reads symbols with `LEX` and follows every parse still possible at once, as runs of words in free arrays `P` and `PP`;
the output (production numbers, and minus the directory index of a name or symbol) is chained in `O`, newest first, and
handed to `COTREE` whenever one parse remains. `PARSE`, `SYMBOL`, `PFORCE`, `COPY`, `POUT`, `PSTATS`. Errors go to
`ERCOR`.

## ercor: error correction and disambiguation

`ERCOR` finds the cheapest sequence of terminal symbols to insert before the symbol that killed every parse, searching
the syntax graph up to `MAXPATH` terminals deep; `AMBIG` and `ISNAME` decide between parses that are all alive. The
step trace of switch `G` is reduced to its summary lines in the seed.

## entree: the code tree

The special cases of the semantics are kept in a tree in free array `CT`: a node matches an item of the pattern, an
identifier, a kind of stack item or a production. `ENTREE` adds a pattern, `INSNO` and `COMPNO` keep the alternates of
a node in order of increasing generality, `MATCH` matches the stack against the tree, `CODEID` takes an identifier of a
pattern apart. The links word has three 20 bit fields (`SCR`, `ALT`, `NODEN`).

## cotree: the semantic stack reducer

`COTREE(S)` adds one item of parser output to the code stack; for a production it tries a match between a path in the
code tree and the stack from each position on, and a match is copied to the semantic stack and given to `DOSEM`, whose
result replaces what matched. `CODENT`, `CFUDGE`, `MAXWELL` and `MAXEND` (the quoted semantics collector), `SETPRI`,
`SAMEAS`, `PSTACK`. The code attached to an entry (`CB`, `CE`) is a chain of records in free array `CO`.

## dosem: the semantics array and its interpreter

The semantics of a production is a run of words in `SEMS` (value, argument count, and a type: call a routine, push a
parameter, enter a node, `CASE` constants, and so on); `PROSEM` records where in the code tree each production can
match. `DOSEM` interprets one; `SETSEM`, `SEMFIX`, `SEMFX1`, `DOCASE`, `ENPROD`, `MATCHP`, `RESETSEM` and the rest build
and query it.

## rsyn: the bootstrap reader

Reads, with `LEX` alone, the part of the syntax file up to the first `%%`: statements `<CLASS> ::= elements ::=
semantics` in a small notation, making the graph and the semantics array as the full compiler does once the syntax for
syntax is in. Its routines (`SYNT`, `SYTRM`, `SEMP`, `SEMR`, `QUOSEM`, `SYNTAX`, `REPVAL`, `VALU`, and others) are semantic
routines too, registered by name, because the bootstrapped syntax calls them for the rest of the file. `RSYN` returns after its head (the tables and the
routines entered in the table) when the compiler starts from an image (its argument, which the tape does not use, is 1), where it does not read the syntax.

## debug: the diagnostic print-outs

`GPRIN1`, `PARTR1`, `TPRIN1`, `SEMPR1`, `PSTAC1` and `HELPD` print the tables as the switches ask. `src/debug.imc` does it
with `PRINT`; `stage0/debug.c` with `stage0/pr.c`, the part of PMOD (`pmod.md`) that it needs. `PCODE1` lists the code
and is in `src/code3.imc`. The dumps print the 64 bit words in octal.

## impsem, code, code2, code3, demit: the code generator

See `code-generator.md`.

## driver

`src/impm.imc` is the kernel's driver program: `IMPK` reads the command line (`-s`, `-o`, `-c`, `-T`, `-I`, `-i`), reads
the syntax with `RSYN`, and compiles the files with `COMPIL`; `ERROR` and `MSG` write to standard error, and a fatal
error returns from `IMPK`. With `-i` it reads the commands of the original from the terminal (`GETTY`, `READCMD`).
`src/image.imc` writes and reads an image (`NAME_/U`, `/V`, `-I`). `src/driver.imc` is the main program of `imp`: it
finds its parts relative to itself, runs `IMPK`, then the back end (`src/llvm.imc`) and the link. In the seed, `imps0`
is the kernel and `impstage0` the compiler, the same program under another name (`stage0-semantics.md`).

The runtime library is in `lib/`: `impio.iml` (`PRINT`, `READ`, `FINI`), `imprt.iml` (`CALLI`, shifts, byte pointers,
`EXECUTE`), `impsys.iml` (the arena, argument vectors), `impmain.iml` (the C `main`), `imphost.iml` and
`impvars.iml` (what the link and the images need of the host).
