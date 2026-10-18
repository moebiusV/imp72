# imp72

IMP72 (Irons / Bilofsky, DECUS 10-198, 1973) bootstrapped onto modern Linux,
the BSDs and macOS, with pcc as the back end and a C-ABI FFI.

Status: stage 0 (a hand port of the compiler's kernel to C) reads the whole of
the IMP72 syntax file, `decus/syntax.imc`, building the syntax graph, the
semantics and the code tree; it does not compile programs yet. The plan is in
`docs/PLAN.md`, how to build and run in `docs/BUILDING.md`.

    cd stage0 && make check && ./imp72s0 ../decus/syntax.imc

- `decus/`: the original tape, unchanged, with hashes and attribution.
- `third_party/pcc`: the pcc back end (submodule, github `moebiusv/pcc`).
- Stage 0 is C99, with GNU extensions allowed only where pcc compiles them.
