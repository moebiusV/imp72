# Stage 0 build.  Plain make (GNU or BSD); the compiler is $(CC).
CC      = cc
LDFLAGS =
CFLAGS  = -std=c99 -O1 -g -Wall -Wextra -pedantic
AR      = ar
CXX     = c++
# the LLVM back end in the process (the top Makefile passes these where the LLVM libraries are installed: tools/llvm.sh)
LLVMCFLAGS =
LLVMLIBS   =
LLVMOBJS   =
OBJS    = error.o free.o stack.o dir.o lex.o graph.o parse.o ercor.o pr.o debug.o dosem.o entree.o cotree.o rsyn.o semtab.o sem.o impsem.o codegen.o pcode.o ir.o trans.o
TESTDIR = ../tests/stage0
TESTS   = test_free test_stack test_dir test_lex test_parse test_debug test_decl

all: libstage0.a imp72s0 imp72stage0

imp72s0: imp72s0.o imphost.o $(LLVMOBJS) libstage0.a
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ imp72s0.o imphost.o $(LLVMOBJS) libstage0.a $(LLVMLIBS)

imphost.o: ../lib/imphost.c
	$(CC) -std=c99 -O1 -g -c ../lib/imphost.c

imp72s0.o: imp72s0.c imp.h ir.h
	$(CC) $(CFLAGS) $(LLVMCFLAGS) -c imp72s0.c

llvmorc.o: llvmorc.c
	$(CC) -std=gnu99 -O1 -g $(LLVMCFLAGS) -c llvmorc.c

impld.o: impld.cpp
	$(CXX) -std=c++17 -O1 -g -fno-rtti $(LLVMCFLAGS) -c impld.cpp

ir2ll.o: ../tools/ir2ll.c
	$(CC) -std=gnu99 -O1 -g -DIR2LL_LIB -c ../tools/ir2ll.c

# the compiler is the same program under another name
imp72stage0: imp72s0
	rm -f $@ && ln -s imp72s0 $@

libstage0.a: $(OBJS)
	$(AR) rcs $@ $(OBJS)

$(OBJS): imp.h

.c.o:
	$(CC) $(CFLAGS) -c $<

check: $(TESTS) imp72s0
	@for t in $(TESTS); do echo "== $$t"; ./$$t || exit 1; done
	@echo "== test_syntax"; sh $(TESTDIR)/test_syntax.sh
	@echo "== test_codegen"; sh $(TESTDIR)/test_codegen.sh

test_free: $(TESTDIR)/test_free.c libstage0.a
	$(CC) $(CFLAGS) -I. -o $@ $(TESTDIR)/test_free.c libstage0.a
test_stack: $(TESTDIR)/test_stack.c libstage0.a
	$(CC) $(CFLAGS) -I. -o $@ $(TESTDIR)/test_stack.c libstage0.a
test_dir: $(TESTDIR)/test_dir.c libstage0.a
	$(CC) $(CFLAGS) -I. -o $@ $(TESTDIR)/test_dir.c libstage0.a
test_lex: $(TESTDIR)/test_lex.c libstage0.a
	$(CC) $(CFLAGS) -I. -o $@ $(TESTDIR)/test_lex.c libstage0.a

test_parse: $(TESTDIR)/test_parse.c libstage0.a
	$(CC) $(CFLAGS) -I. -o $@ $(TESTDIR)/test_parse.c libstage0.a

test_debug: $(TESTDIR)/test_debug.c libstage0.a
	$(CC) $(CFLAGS) -I. -o $@ $(TESTDIR)/test_debug.c libstage0.a

test_decl: $(TESTDIR)/test_decl.c libstage0.a
	$(CC) $(CFLAGS) -I. -o $@ $(TESTDIR)/test_decl.c libstage0.a

clean:
	rm -f $(OBJS) imp72s0.o llvmorc.o ir2ll.o imp72s0 imp72stage0 libstage0.a $(TESTS)
