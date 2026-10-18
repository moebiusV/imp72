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
