# The seed, part 2: the syntax graph and the parser

`stage0/graph.c`, `parse.c` and `ercor.c` correspond to `src/graph.imp`, `src/parse.imp` and `src/ercor.imp` (and to
`decus/graph.imc`, `parse.imc`, `ercor.imc`). `tests/stage0/test_parse.c` builds a small grammar with `graph()` and
`gmatr()` and runs `parse()` over it.

## How the parts fit

`graph(synt, nsynt, pn)` enters one production. `synt` is a free array of directory indices: the nonterminal produced,
then the symbols of the production. A nonterminal's name carries a leading `#` (`SYNT` of `rsyn` puts it there).
`gmatr(0)` rebuilds the connectivity matrices; `SYNTAX` in `rsyn.c` calls it after every production it enters.
`parse("#PG")` reads symbols with `lex` and calls `parse_cotree(k)` for each output `k` as it becomes certain: a
production number, or minus the directory index of a name or symbol that matched `NAM#` or `SYM#`. `parse_cotree` is a
function pointer; `imp72s0` sets it to `cotree` and `test_parse` sets it to a recorder.

The header comment of `graph.c` gives the node layout and that of `parse.c` the parse and output formats. A graph node
is two words: left half ALT, right half SCR; the second word has the node type in bits 48-50, the row `NB` in the
bit matrix `NBITM` in bits 32-47, and the directory index (or production number) in the right half. Node types are 0
nonterminal, 1 terminal, 2 circled nonterminal, 3 boxed production, 5 on the chain of terminal entry points.

## Points that matter when comparing with the original

- **Output chain halves.** An output entry holds the value in the left half and the link in the right half. The comment
  in `parse.imc` says the opposite; the code is as in `parse.c`.
- **`bit()` is bounds checked** (`stage0-foundation.md`).
- **`FREE` and `FREES` are functions.** Free storage moves during a call that allocates, and a macro
  `FREES(a, dir(s))` would store through the `fmem` loaded before `dir` grew it.
- **`GMARK`'s `BIT(BITM,NN,MAKENT)`** inside `GRAPH` is a read whose result is discarded. It is kept as a call (it counts
  in the statistics).
- **Statistics** use process time (`imp_msec`).
- **Traces.** The step by step trace of the error corrector (switch `G`) is reduced to its summary lines. `PARTRA` of the
  original is `partr1` in `debug.c`.

## Behaviour of the original that the seed reproduces

1. **Output is put out twice after error correction.** The round in which every parse dies has already added output to
   the surviving parses; after the correction the same round runs again from the same parses. `test_parse` expects
   `2 2 1 3` for a missing `B`.
2. **`ISNAME` starts with `NM = R''''`**, a comparison with the value left in `NM` by the previous call; it never holds
   (`decus/ercor.imc`, `ercor.c`).
3. **`PARSE` caches `ISNAME` in the directory (`ISNM`) and then calls it again** (`parse.c`).
