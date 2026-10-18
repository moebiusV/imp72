# Stage 0, part 2: the syntax graph and the parser

Ports of `graph.imc`, `parse.imc` and `ercor.imc` to `stage0/graph.c`,
`parse.c` and `ercor.c`. `tests/stage0/test_parse.c` builds a small grammar
with `graph()` and `gmatr()` and runs `parse()` over it.

## How it fits together

`graph(synt, nsynt, pn)` enters one production. `synt` is a free array of
directory indices: the nonterminal produced, then the symbols of the
production (a nonterminal's name carries a leading `#`, as `RSYN`'s `SYNT`
makes it). `gmatr(0)` rebuilds the connectivity matrices after each; `RSYN`
calls it after every syntax statement. `parse("#PG")` then reads symbols with
`lex` and calls `parse_cotree(k)` for each output `k` as it becomes certain:
a production number, or minus the directory index of a name or symbol that
matched `NAM#` or `SYM#`. `parse_cotree` is a hook until the semantic stack
reducer (`cotree`, `entree`, `dosem`) is ported.

`graph.c`'s header comment gives the node layout; `parse.c`'s gives the parse
and output formats. Both differ from the originals' in the position of fields
(32 bit halves) and not otherwise.

## Choices that matter when comparing with the original

- **`FREE` and `FREES` are functions** (static inline). Free storage moves:
  with a macro, `FREES(a, dir(s))` can store through the `fmem` it loaded before
  `dir` grew it. AddressSanitizer found this in a test.
- **Output chain halves.** `parse.imc`'s comment says an output entry has the
  value in bits 0-17 and the link in 18-35. The code puts them the other way,
  as `parse.c` does.
- **`bit()` is bounds checked** (see part 1). `GRAPH` and `GMATR` ask for
  rows one past the last node; the original reads whatever memory follows.
- **`GMARK`'s `BIT(BITM,NN,MAKENT)`** inside `GRAPH` is a read whose result is
  thrown away. It is kept as a call (it counts in the statistics) with its
  result ignored.
- **Parse time statistics** use process time and omit the original's one
  millisecond bias per measurement.
- The step by step trace of the error corrector (`/G`) is reduced to its main
  lines; the parser's trace is close to the original's. `PARTRA` and `GPRINT`,
  which call debugging routines from `debug.imc`, are hooks (`gprint_hook`).

## Things the original does that look like bugs

Reproduced, with the reason to check them against the oracle:

1. **Output is put out twice after error correction.** The round in which
   all parses die has already added output to the surviving parses in `P`;
   after the correction the same round runs again from the same parses.
   (`test_parse` expects `2 2 1 3` for a missing `B`.)
2. **`ERCOR` continues past circled nodes without checking the goal.** After a
   goal is made, backing up tries going on from the circled node through its
   `SCR` chain, which holds the tails of every production that *begins* with
   that nonterminal; the parser filters those with `GNEXT`, `ERCOR` does not.
   With `#PG ::= #S %` (a production starting with the nonterminal) and input
   `A C % %` it "corrects" `%` to `%` forever; a grammar whose productions do
   not begin with the nonterminal (`#PG ::= G #S %`) corrects properly. The
   real syntax has many productions starting with a nonterminal, so either the
   real compiler lives with it, or the oracle will show I misread something.
3. `ISNAME` starts with `NM = R''''`, comparing a stale `NM`; it never holds.
4. `PARSE` caches `ISNAME` in the directory (`ISNM`) and then calls it again.

## Open

The oracle comparison (graph dump after `GMATR`, parse traces) is not done.
Nothing here has been run on the real `syntax.imc` yet; that needs `rsyn`.
