# Building and running

Stage 0 is C99 with no dependencies beyond libc.

    cd stage0
    make            # libstage0.a and the driver, imp72s0
    make check      # unit tests, and the syntax file read in full
    ./imp72s0 ../decus/syntax.imc
    ./imp72s0 -s TSX -o dump.txt ../decus/syntax.imc    # code tree, semantics, graph

`-s` takes the letters of the original's compiler switches (see the header of
`imp72s0.c`); dumps go to the `-o` file, else standard output. The exit status
is 1 if an error (not an advisory) was reported.

Any C99 compiler: `make CC=pcc`, `make CC=clang`. With pcc see
`docs/porting/pcc-notes.md` (the headers it lacks, linking, musl).

Under a sanitizer:

    make clean
    make check CFLAGS="-std=c99 -O1 -g -fsanitize=address,undefined"

## The whole system

    make                  # stage 0, the runtime, ir2c, and on amd64 pcc's cc1
    make check            # all the tests (a few minutes)
    make install PREFIX=/usr DESTDIR=/tmp/root

Make variables: `CC`, `CFLAGS`, `LDFLAGS`, `DESTDIR`, `PREFIX`, and
`WITH_PCC=yes|no|auto` (auto: build pcc's cc1 on x86_64 only). The submodule
`third_party/pcc` must be checked out (`git submodule update --init`).

`tools/imp72c` works from the source tree and from an installed one. Without
`cc1` it uses the C back end; `-C` chooses it anywhere. Programs:

    tools/imp72c -o prog prog.imp                 # executable
    tools/imp72c -c -o prog.o prog.imp            # object, to link with C
    tools/imp72c -C -S -o prog.c prog.imp         # the C text

Tests: `make -C stage0 check` (units, the syntax file, code listings),
`tests/native/run.sh` (pcc) and `runc.sh` (C back end) compile and run the
programs of `tests/native/` and compare their output, `tests/self/run.sh` the
ported modules, `tests/self/stage1.sh` stage 1 against stage 0 (it needs ELF
`objcopy`). `tools/survey.sh` and `tools/modules.sh` report how far stage 0
gets on each module of the original compiler's source.

Packages: `packaging/README.md`.
