# Provenance of `decus/`

Everything in this directory is the DECUS 10-198 tape directory, **unchanged**.
Never edit these files; ported sources live in `src/`.

- `SHA256SUMS`: the 30 text files (`cd decus && sha256sum -c SHA256SUMS`).
- `SHA256SUMS.bin`: the three PDP-10 binaries (`imp.low`, `imp.shr`, `implib.lib`),
  fetched 2026-10-18 from the same archive directory. The archive serves 36-bit
  words as 5 bytes each (12090 words -> 60450 bytes for `imp.low`), **but with only
  35 bits of each word** (every byte is 7 bit; the last bit of each word is lost).
  They are useful for reading, not for running: the exact words are in `decus/exact/`
  (recovered from the DECUS tape image, see `docs/ORACLE.md`).
- The text files carry DEC-10 conventions (CRLF line ends, upper case).

## Source and attribution

- **Program:** DECUS program 10-198a, *IMP - PDP-10 IMP72 Compiler, Version
  1.5*, October 3, 1973.  **Author:** Walter Bilofsky, Yale University,
  Department of Computer Science.  **Submitted by:** Sonya Shapiro, Bolt Beranek
  and Newman, Inc.  Source language: IMP72.
- **The language:** IMP, by Edgar T. (Ned) Irons; the compiler is described in
  Bilofsky's *IMP72 Reference Manual* (DECUS 10-198, 1973; the scan at the
  [Computer History Museum](https://archive.computerhistory.org/resources/access/text/2024/09/102737513-05-0002-acc.pdf))
  and in "Syntax Extension and the IMP72 Programming Language" (SIGPLAN
  Notices, 1974).
- **Where these files came from:** the PDP-10 archive's extracted listing of the
  DECUS library tape 10-LIB-3, directory `[43,50306]`:
  <https://pdp-10.trailing-edge.com/decuslib10-03/index.html>.  A file is
  `https://pdp-10.trailing-edge.com/decuslib10-03/01/43,50306/<name>`.  The
  files carry the tape's own dates (September 1973).
- **Licence:** the archive states of the DECUS software that it "was donated by
  PDP-10 users to the DECUS library over the past 5 decades ... and was put into
  public domain at the time of donation", and that "use and distribution of files
  from the DECUS tapes is completely unrestricted" (<https://pdp-10.trailing-edge.com/>,
  as read 2026-10-17).  The manual's own notice says the same in other words: the
  program is a user contribution, offered as it is, with no warranty.  hbnf keeps
  the files unchanged, with this attribution, and points at the archive as the
  authority: where a file here and the archive disagree, the archive is right.

