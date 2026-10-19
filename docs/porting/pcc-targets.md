# Which pcc back ends work, and how we found out

The translator writes pcc pass 2 IR (`imp72s0 -c`). Calls, parameters and
returns in that IR are already lowered for one machine: argument registers
`rdi rsi rdx rcx r8 r9`, further arguments at `0(%rsp)+8k` (caller) and
`16(%rbp)+8k` (callee), the result in `rax`, assembler directives in GNU
x86 style (`translator: trans.c`, `ir.c`; `ir2c.c` reads the same). A pcc target
is usable when pcc's own back end for it works, and then needs the same few facts
for its registers (from `arch/<target>/macdefs.h`) and its directives.

## Method

pcc is built natively (`configure && make`) inside an Alpine container of the
architecture, run under QEMU user mode through Docker's binfmt handlers, the
way `ada-on-alpine` does it (`docker run --platform linux/arm64 alpine:edge`),
and asked to compile and run a small C program of integer arithmetic, an
eight argument call, shifts, a struct passed by value, an array loop, and
`printf`. The same containers build this repository and run `make check`.

## Results (pcc revision 9db636ff, 2026-10-18)

| machine | pcc builds | pcc compiles the test | our `make check` (C back end) |
|---|---|---|---|
| x86_64 | yes | yes | passes; also passes with pcc as the back end |
| aarch64 | yes | **no**: `compiler error: op 75 slipped through` on the integer test; a `double` constant gives `strtosf: soft != hard` | passes |
| riscv64 | **no**: `riscv64-unknown-linux-musl is not (yet) supported by pcc` | - | passes |
| ppc64le | pcc's `powerpc` target is 32 bit big endian | - | passes |
| s390x | no pcc target | - | passes |
| loongarch64 | no pcc target | - | passes |

("passes" is the whole of `make check`: the unit tests, the syntax file, the
listings, 13 programs run and compared, the ported module and stage 1.)

i386, arm, mips and m68k pcc targets exist but are 32 bit, and the language's
word is 64 bits; they would need the word type mapped to `long long`, and are
untried.

## What follows

- aarch64 is the target that matters (Linux arm64, Apple silicon): fix pcc's
  port (the audit in `docs/PLAN.md`), then add its register table to the
  translator and run the same test. Until then the C back end is how arm64 is
  supported.
- When a target's pcc works, the table is: argument registers, the stack
  argument area, the return register, the frame pointer, the assembler
  directives (`.type f,%function` on ARM).
