#!/bin/sh
# tools/cimport.imp (imp72-cimport): the declarations of C headers as IMP72.  Run from the top of the tree (make check does).
#   - tests/cimport/cases.txt: each line is NAME INPUT ARGS...; the tool reads the preprocessed text tests/cimport/INPUT (no cc) and its output is compared with tests/cimport/NAME.out
#   - the output of the first case, with tests/cimport/basic.use.imp after it, is compiled by the seed and by imp72 and run (basic.use.out; basic.use32.out where long is 32 bits)
#   - each file of the tool compiles with the seed
#   - the usage and the errors
#   - with a C compiler: <string.h>, <stdlib.h> and <stdio.h> declarations used by a program that sorts with qsort and a CALLBACK; the layouts of struct tm, timespec (and stat and dirent on
#     Linux) that the tool reads from the headers, compared with sizeof and offsetof printed by a C program; the values of macros (-D) compared with those C prints; no file left in TMPDIR
root=$(cd "$(dirname "$0")/../.." && pwd)
cd "$root" || exit 1
two=${CIMPORT_IMP72:-$root/build/llvm-ir2/imp72stage-llvm-ir}
seed=$root/stage0/imp72stage0
if [ ! -x "$two" ]; then echo "cimport test skipped: no $two (the tool is built by imp72)"; exit 0; fi
w=build/cimport
rm -rf $w; mkdir -p $w
fail=0
bad() { echo "FAILED: cimport: $*" >&2; fail=1; }
echo "== cimport"
tool() { sh tools/imptool.sh cimport "$@"; }
tool -h >/dev/null 2>&1; [ $? = 2 ] || { bad "-h is not a usage message with status 2"; exit 1; }
[ -x build/tools/cimport ] || { bad "the tool is not built"; exit 1; }

# the fixtures
while read -r name input args; do
    [ -n "$name" ] || continue
    tool $args -i tests/cimport/$input > $w/$name.got 2> $w/$name.err
    cmp -s $w/$name.got tests/cimport/$name.out || { diff $w/$name.got tests/cimport/$name.out | head -20; bad "$name: output differs from tests/cimport/$name.out"; }
done < tests/cimport/cases.txt
# the same text from the standard input
tool -i - < tests/cimport/basic.i 2>/dev/null | cmp -s - tests/cimport/basic.out || bad "-i - (the standard input) gives another output"
# -o: the file is written
tool -o $w/o.imp -n pt -i tests/cimport/basic.i 2>/dev/null; [ -s $w/o.imp ] && grep -q '^STRUCT pt (INT x, INT y);$' $w/o.imp || bad "-o"
# the warning for a name that is not declared (the fixture 'nothing')
grep -q 'nothing is declared with the name no_such_name' $w/nothing.err || bad "no warning for a name that nothing declares"

# the output compiled with a program, by imp72 and by the seed (one file); the sizes in what it prints are those of the machine: basic.use.out (64 bit long) or basic.use32.out
use=tests/cimport/basic.use.out; [ "$(getconf LONG_BIT)" = 64 ] || use=tests/cimport/basic.use32.out
cat tests/cimport/basic.out tests/cimport/basic.use.imp > $w/basic_all.imp
"$two" -o $w/basic_all $w/basic_all.imp > $w/basic_all.log 2>&1 || { cat $w/basic_all.log; bad "imp72 does not accept the declarations of the fixture"; }
[ -x $w/basic_all ] && { $w/basic_all > $w/basic_all.got 2>&1; cmp -s $w/basic_all.got $use || { diff $w/basic_all.got $use; bad "the program of the fixture prints something else"; }; }
if [ -x "$seed" ]; then
    "$seed" -o $w/basic_seed $w/basic_all.imp > $w/basic_seed.log 2>&1 || { cat $w/basic_seed.log; bad "the seed does not accept the declarations of the fixture"; }
    [ -x $w/basic_seed ] && { $w/basic_seed > $w/basic_seed.got 2>&1; cmp -s $w/basic_seed.got $use || bad "the program of the fixture, by the seed, prints something else"; }
    # (the files of the tool, each compiled by the seed)
    for f in cimport cimport-tok cimport-parse cimport-out; do
        "$seed" -c -o $w/seed-$f.o tools/$f.imp > $w/seed-$f.log 2>&1 || { cat $w/seed-$f.log; bad "the seed does not compile tools/$f.imp"; }
    done
fi

# the usage and the errors
tool > /dev/null 2>$w/e1.err; [ $? = 2 ] || bad "no argument: the status is not 2"
tool -x foo.h > /dev/null 2>$w/e2.err; [ $? = 1 ] && grep -q 'unknown option' $w/e2.err || bad "unknown option"
tool -n > /dev/null 2>$w/e3.err; [ $? = 1 ] && grep -q 'missing value' $w/e3.err || bad "-n without a name"
tool -i $w/no_such_file.i > /dev/null 2>$w/e4.err; [ $? = 1 ] && grep -q 'cannot read' $w/e4.err || bad "unreadable input"
tool -i tests/cimport/basic.i -D EOF > /dev/null 2>$w/e5.err; [ $? = 1 ] || bad "-D with -i"
tool 'a;b.h' > /dev/null 2>$w/e6.err; [ $? = 1 ] && grep -q 'not a plain header name' $w/e6.err || bad "a header name with a semicolon"
tool -D 'a;b' x.h > /dev/null 2>$w/e7.err; [ $? = 1 ] && grep -q 'not a macro name' $w/e7.err || bad "a macro name with a semicolon"

# the rest needs the C compiler
cc=${CC:-cc}
if ! command -v $cc >/dev/null 2>&1; then
    echo "cimport: no C compiler ($cc): the tests of the headers are skipped"
    [ $fail = 0 ] && echo ok
    exit $fail
fi
if [ "$(getconf LONG_BIT)" != 64 ]; then
    echo "cimport: long is not 64 bits: the tests of the headers are skipped"
    [ $fail = 0 ] && echo ok
    exit $fail
fi

# a program with the declarations of three headers: qsort with a callback, printf, puts, strlen, abs, strcmp, free, malloc
tool -n qsort -n abs -n malloc -n free stdlib.h > $w/q1.imp 2>$w/q1.err || { cat $w/q1.err; bad "stdlib.h"; }
tool -n printf -n puts stdio.h > $w/q2.imp 2>$w/q2.err || { cat $w/q2.err; bad "stdio.h"; }
tool -n strlen -n strcmp string.h > $w/q3.imp 2>$w/q3.err || { cat $w/q3.err; bad "string.h"; }
cat $w/q1.imp $w/q2.imp $w/q3.imp tests/cimport/qsort.imp > $w/qsort_all.imp
"$two" -o $w/qsort_all $w/qsort_all.imp > $w/qsort_all.log 2>&1 || { cat $w/qsort_all.log; bad "qsort program does not compile"; }
if [ -x $w/qsort_all ]; then
    $w/qsort_all > $w/qsort_all.got 2>&1
    cmp -s $w/qsort_all.got tests/cimport/qsort.out || { diff $w/qsort_all.got tests/cimport/qsort.out; bad "qsort program prints something else"; }
fi
if [ -x "$seed" ]; then
    "$seed" -o $w/qsort_seed $w/qsort_all.imp > $w/qsort_seed.log 2>&1 || { cat $w/qsort_seed.log; bad "qsort program does not compile with the seed"; }
    [ -x $w/qsort_seed ] && { $w/qsort_seed > $w/qsort_seed.got 2>&1; cmp -s $w/qsort_seed.got tests/cimport/qsort.out || bad "qsort program by the seed prints something else"; }
fi

# the layouts: STRUCT NAME from HEADER, its size and the offsets of its fields, as C says them and as imp72 computes them
layout() {  # name header
    n=$1; h=$2
    tool -n $n $h > $w/l_$n.imp 2>$w/l_$n.err || { cat $w/l_$n.err; bad "$h"; return; }
    line=$(sed -n "s/^STRUCT $n (\(.*\));\$/\1/p" $w/l_$n.imp)
    [ -n "$line" ] || { bad "$h: no STRUCT $n"; return; }
    fields=$(printf '%s\n' "$line" | tr ',' '\n' | sed 's/^ *[A-Za-z_0-9!]* //; s/\[.*//' | grep -v '^PAD[0-9]')
    { printf '#include <stdio.h>\n#include <stddef.h>\n#include <%s>\nint main(void) {\n' $h
      printf 'printf("size %%ld\\n", (long)sizeof(struct %s));\n' $n
      for f in $fields; do printf 'printf("%s %%ld\\n", (long)offsetof(struct %s, %s));\n' $f $n $f; done
      printf 'return 0; }\n'; } > $w/l_$n.c
    $cc -o $w/l_$n $w/l_$n.c 2>$w/l_$n.cerr || { cat $w/l_$n.cerr; bad "$n: cc"; return; }
    { cat $w/l_$n.imp
      printf "EXTERNAL INT printf(PTR, ...);\n"
      printf "CCALL printf('size %%ld%%c',SIZEOF(%s),10);\n" $n
      for f in $fields; do printf "CCALL printf('%s %%ld%%c',OFFSETOF(%s,%s),10);\n" $f $n $f; done
      printf "CCALL printf('end%%c',10)\n%%%%\n"; } > $w/l_$n.all.imp
    "$two" -o $w/l_$n.exe $w/l_$n.all.imp > $w/l_$n.log 2>&1 || { head -5 $w/l_$n.log; bad "$n: imp72 does not compile the declarations from $h"; return; }
    $w/l_$n > $w/l_$n.want
    $w/l_$n.exe | grep -v '^end$' > $w/l_$n.got
    cmp -s $w/l_$n.want $w/l_$n.got || { diff $w/l_$n.want $w/l_$n.got | head; bad "struct $n from <$h>: the layout is not the one of the C compiler"; }
}
layout tm time.h
layout timespec time.h
if [ "$(uname -s)" = Linux ]; then
    layout stat sys/stat.h
    layout dirent dirent.h
    layout timeval sys/time.h
fi

# the values of macros: -D, as the C compiler says them (a negative one is its 64 bit pattern)
tool -D EOF -D BUFSIZ -D SEEK_END -D NO_SUCH_MACRO_AT_ALL stdio.h > $w/m.imp 2>$w/m.err || { cat $w/m.err; bad "-D"; }
grep -q 'macro NO_SUCH_MACRO_AT_ALL: it is not defined' $w/m.imp || bad "-D: an undefined macro is not listed"
{ printf '#include <stdio.h>\nint main(void) { printf("%%lld %%lld %%lld\\n", (long long)EOF, (long long)BUFSIZ, (long long)SEEK_END); return 0; }\n'; } > $w/m.c
$cc -o $w/m $w/m.c 2>$w/m.cerr || { cat $w/m.cerr; bad "-D: cc"; }
{ cat $w/m.imp; printf "EXTERNAL INT printf(PTR, ...);\nCCALL printf('%%lld %%lld %%lld%%c',EOF,BUFSIZ,SEEK_END,10)\n%%%%\n"; } > $w/m.all.imp
"$two" -o $w/m.exe $w/m.all.imp > $w/m.log 2>&1 || { head -5 $w/m.log; bad "-D: imp72 does not compile the constants"; }
[ -x $w/m ] && [ -x $w/m.exe ] && { $w/m > $w/m.want; $w/m.exe > $w/m.got; cmp -s $w/m.want $w/m.got || { diff $w/m.want $w/m.got; bad "-D: the values are not the C compiler's"; }; }

# no file is left in TMPDIR by the probe of macros, a relative TMPDIR is not used, and nothing is made in the current directory
mkdir -p $w/tmp $w/cwd
TMPDIR=$root/$w/tmp tool -D EOF stdio.h > /dev/null 2>&1
[ -z "$(ls -A $w/tmp)" ] || bad "-D leaves something in TMPDIR: $(ls -A $w/tmp)"
( cd $w/cwd && TMPDIR=relative_dir sh "$root/tools/imptool.sh" cimport -D EOF stdio.h > /dev/null 2>&1; [ -z "$(ls -A .)" ] ) || bad "-D with a relative TMPDIR makes something in the current directory"
# SIGTERM while the C compiler of the probe runs: the private directory is removed and the status is 1
rm -rf $w/sigtmp; mkdir -p $w/sigtmp
TMPDIR=$root/$w/sigtmp CC="sh -c 'sleep 5' x" build/tools/cimport -D EOF stdio.h > /dev/null 2>&1 &
pid=$!
n=0
while [ $n -lt 300000 ]; do
    [ -n "$(ls -A $w/sigtmp)" ] && break
    kill -0 $pid 2>/dev/null || break
    n=$((n + 1))
done
made=$(ls -A $w/sigtmp)
kill -TERM $pid 2>/dev/null
wait $pid 2>/dev/null; rc=$?
[ -n "$made" ] || bad "the probe made no directory in TMPDIR (the signal test did not run)"
[ -z "$(ls -A $w/sigtmp)" ] || bad "SIGTERM left something in TMPDIR: $(ls -A $w/sigtmp)"
[ $rc = 1 ] || bad "SIGTERM during the probe: the status is $rc, not 1"
# a header that is not there is an error, not a file
tool no_such_header_at_all.h > /dev/null 2>&1; [ $? = 1 ] || bad "a header that does not exist: the status is not 1"

[ $fail = 0 ] && echo ok
exit $fail
