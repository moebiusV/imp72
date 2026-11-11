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
LLVMBINDIRDEF =
OBJS    = error.o free.o stack.o dir.o lex.o graph.o parse.o ercor.o pr.o debug.o dosem.o entree.o cotree.o rsyn.o semtab.o strl.o ir.o gen_impsem.o gen_code.o gen_code2.o gen_code3.o gen_demit.o
TESTDIR = ../tests/stage0
TESTS   = test_free test_stack test_dir test_lex test_parse test_debug

all: libstage0.a imps0 impstage0

imps0: imps0.o imphost.o $(LLVMOBJS) libstage0.a
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ imps0.o imphost.o $(LLVMOBJS) libstage0.a $(LLVMLIBS)

imphost.o: imphost.c
	$(CC) -std=c99 -O1 -g $(LLVMBINDIRDEF) -c imphost.c

imps0.o: imps0.c imp.h ir.h gen.h
	$(CC) $(CFLAGS) $(LLVMCFLAGS) -c imps0.c

llvm.o: llvm.c
	$(CC) -std=gnu99 -O1 -g $(LLVMCFLAGS) -c llvm.c

lvtriple.o: lvtriple.cpp
	$(CXX) -std=c++17 -O1 -g -fno-rtti $(LLVMCFLAGS) -c lvtriple.cpp

impld.o: impld.cpp
	$(CXX) -std=c++17 -O1 -g -fno-rtti $(LLVMCFLAGS) -c impld.cpp

strl.o: strl.c strlcpy.c strlcat.c

ir2ll.o: ../tools/ir2ll.c
	$(CC) -std=gnu99 -O1 -g -DIR2LL_LIB -c ../tools/ir2ll.c

# the compiler is the same program under another name
impstage0: imps0
	rm -f $@ && ln -s imps0 $@

libstage0.a: $(OBJS)
	$(AR) rcs $@ $(OBJS)

$(OBJS): imp.h

.c.o:
	$(CC) $(CFLAGS) -c $<

check: $(TESTS) imps0
	@for t in $(TESTS); do echo "== $$t"; ./$$t || exit 1; done
	@echo "== test_syntax"; sh $(TESTDIR)/test_syntax.sh

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


clean:
	rm -f $(OBJS) imps0.o llvm.o ir2ll.o imps0 impstage0 libstage0.a $(TESTS)
