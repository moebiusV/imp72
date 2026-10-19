# The C back end (`tools/ir2c.c`)

pcc does not generate code for every host (macOS on arm64 is the one that
matters), so the pcc IR that `imp72s0 -c` writes can also be turned into C and
left to the host's compiler: `imp72stage0 -C FILE.imp`. `-S` with `-C` stops at the
C file.

`ir2c` reads the IR on standard input and writes a C translation unit:

- a word is `int64_t`, a real `double`, an address an `int64_t` cast where it
  is dereferenced (`*(int64_t *)(intptr_t)(addr)`), so the output does not
  depend on pointer width tricks beyond `intptr_t`;
- each temp is a local of the type of its first use, each label `Ln`, each
  tree a statement; arithmetic is cast to the type of its node, shifts are
  done on `uint64_t` and masked to 63 bits;
- a subroutine is `int64_t name(int64_t a0, ...)`: the parameters are the
  registers (`rdi`, `rsi`, ...) and the stack slots (`16(%rbp)+8k`) that the
  translator reads, so the C calling convention is the one the pcc output has;
- a call collects the argument assignments (`ASSIGN(REG rdi, x)`, stores below
  `rsp`) into an argument list; the callee is declared `extern int64_t f();`;
- the data directives (`.comm`, `.quad`, `.string`, labels) become arrays:
  `__attribute__((common, aligned(8))) long long x[n]`, so variables are
  shared between modules as with pcc, and data referenced but not defined
  is `extern long long x[];`. Symbols with a dot become `_D`; a symbol that is a
  C keyword is an error.

GNU C attributes are used (gcc and clang both accept them), and the tool uses
`open_memstream` (POSIX 2008).

Not done: real arguments by value to C (the IR has none yet), a variadic C
function called with `CCALL` on a platform whose variadic convention differs
from the ordinary one (Apple arm64 puts the variadic arguments on the stack;
the call is made through an unprototyped declaration), and the registers other
than the six argument registers.

`tests/native/runc.sh` runs every native test through this back end; `make
check` does it with the pcc runs.
