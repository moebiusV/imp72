# Which pcc back ends work, and how we found out

The pcc used is the moebiusv fork (`third_party/pcc`, branch `imp72-fixes`:
the fork's master plus two portability fixes). The upstream revision this
project first pinned (9db636ff) had an aarch64 back end that was the 32 bit ARM
port renamed and could not compile a loop; the fork replaced it with an AAPCS64
LP64 back end, which is what is used now.

The translator writes pcc pass 2 IR (`imp72s0 -c`). Calls, parameters and
returns in that IR are already lowered for one machine: `stage0/ir.c` has the
table (`struct irtarget`): the numbers of the argument registers and the
result register, the frame and stack pointers, where stack arguments start, the
frame the back end reserves before the first automatic, and the `.type`
operand. `imp72s0 -T TARGET` chooses it; `imp72stage0` picks the machine it
runs on. The C back end (`tools/ir2c.c`) reads the amd64 numbering.

## Method

pcc is built natively (`configure && make`) inside an Alpine container of the
architecture, run under QEMU user mode through Docker's binfmt handlers, the way
`ada-on-alpine` does it (`docker run --platform linux/arm64 alpine:edge`;
`tools/ci-arch.sh ARCH` builds and tests this repository that way). For the C
compiler itself, `tests/pcc/*.c` are compiled by gcc and by pcc's ccom and what
they print is compared (`tools/pcc-difftest.sh`); the same containers run
`make check` for the whole system.

## Results

| machine | pcc | our `make check` |
|---|---|---|
| x86_64 | works; the back end for IMP | passes, pcc and C back ends |
| aarch64 | the fork's back end passes `tests/pcc` (integers, floats, calls with stack arguments, structs, globals) after two fixes below; the back end for IMP | passes, with pcc as the back end |
| riscv64 | upstream's port is for NetBSD only and its output does not assemble; not tried in the fork | passes, C back end |
| ppc64le, s390x, loongarch64 | no pcc target | passes, C back end |
| i386 | 32 bit; the IMP word is mapped to `long long` (pass 2 pushes the arguments, returns edx:eax, and the runtime keeps C `long` results apart from words) | passes, with pcc and with the C back end (32 bit Alpine) |
| arm (armv7) | 32 bit; the IMP word is `long long` in a register pair. The fork's back end needed: globals and named constants through the literal pool (kept out of line), a callee-saved register save and a correct return (`sp` was left 16 bytes low), 64-bit subtract, multiply (`__muldi3`), constant loads (`mvn` of the complement), signed shifts by 32 or more, compare operand order and unsigned low word, `livecall` for pushed arguments, and OREG ranges for word pairs. Arguments 5 and up are pushed by pass 2 (`FUNARG`, last first), the first four are in r0-r3, the word is returned in r0:r1 | passes (`tests/native`, the oracle goldens, the stack module), in Alpine armv7 under QEMU |
| mips64 | the fork's back end is an o32 design with 64-bit values in pairs of 32-bit registers; a proper n64 back end would be a rewrite | the C back end passes (Debian `mips64le/debian:bookworm-slim` under QEMU) |
| mips, m68k, powerpc, sparc64, hppa | no emulator to test with here | not tried |

("passes" is the whole of `make check`: the unit tests, the syntax file, the
listings, 13 programs run and compared, the ported module and stage 1.)

## Fixes made to the fork (branch imp72-fixes)

- `amd64`, `i386`, `aarch64`, `pdp11` and `vax` keep the assembly of the file in
  memory (`-xdmr`) by assigning `stdout = open_memstream(...)`. `stdout` is a
  constant on musl, so none of them compiled there (Alpine). `common/outcap.h`
  redirects file descriptor 1 to a temporary file instead.
- `common/softfloat.h` compared every floating point constant with the host's
  `long double`, which is only the 80 bit x87 type on x86; on an aarch64 host it
  is the 128 bit quad and every `double` constant was an error.

## Adding a target

Check that pcc's back end passes `tests/pcc` where it runs; read its `macdefs.h`
for the register numbers; add a row to the table in `stage0/ir.c`; if the back
end keeps a frame record or the like before the first automatic, give it as
`autos0`. Run `tools/ci-arch.sh`.
