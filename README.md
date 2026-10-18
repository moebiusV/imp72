# imp72

IMP72 (Irons / Bilofsky, DECUS 10-198, 1973) bootstrapped onto modern Linux,
the BSDs and macOS, with pcc as the back end and a C-ABI FFI.

Status: Phase 0 (repository and provenance). The plan is in `docs/PLAN.md`.

- `decus/`: the original tape, unchanged, with hashes and attribution.
- `third_party/pcc`: the pcc back end (submodule, github `moebiusv/pcc`).
- Stage 0 is C99, with GNU extensions allowed only where pcc compiles them.
