# pcc notes

What was needed to build and use `third_party/pcc` (moebiusv/pcc,
1.2.0.DEVEL 20231021) on Debian 13 x86_64, found while checking that stage 0
compiles with pcc.

- **Build tools.** `cc/ccom` needs a yacc and a lex. `./configure` did not find
  them (none installed); a `byacc` and `flex` unpacked from the Debian
  packages into a private directory worked:
  `make YACC=byacc LEX=flex LEX_OUTPUT_ROOT=lex.yy` (`LEX_OUTPUT_ROOT` comes
  out empty from configure when lex is missing).
- **No system headers of its own.** The tree has no `stddef.h`, `stdarg.h`
  (those live in the separate pcc-libs project), so `#include <stdlib.h>`
  fails with "cannot find 'stddef.h'". Minimal ones were enough
  (`size_t`, `ptrdiff_t`, `wchar_t`, `NULL`, `offsetof`; `stdarg.h` mapped to
  the `__builtin_va_*` functions and providing `__gnuc_va_list`).
- **glibc inlines.** `-D__NO_INLINE__` is needed, or glibc's extern inline
  definitions in `<stdio.h>` are a syntax error.
- **No crt or runtime.** Linking with pcc fails (`crtbegin.o`, `-lpcc`).
  Compiling with pcc and linking with the system `cc` works.
- **Result.** With the above, all of `stage0/` and its tests compile with pcc
  and pass (designated initializers included).

These go on the to-do list for the project's own build: carry minimal
headers, or build pcc-libs as part of `third_party`.
