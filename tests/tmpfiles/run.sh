#!/bin/sh
# The temporary files of the compilers (the seed, stage0/impstage0, and imp, build/llvm-ir2/impstage-llvm-ir): none unless the link needs a file for the object; then an unnamed
# memory file where Linux has one, else a private directory in TMPDIR (an absolute path naming a directory) or the default, removed on every way out.  Checks that -S, -c, -Sir and --run make
# nothing, even with a TMPDIR that cannot be written; that a link leaves TMPDIR empty with and without the memory file (IMPNOMEMFD=1); that a failed compile does; that SIGTERM, SIGINT,
# SIGHUP and SIGQUIT during a link do; and that a TMPDIR that is relative, a file or not there is not used.  Run from the top of the tree (make check does).
root=$(cd "$(dirname "$0")/../.." && pwd)
cd "$root" || exit 1
two=${TMPFILES_IMP72:-$root/build/llvm-ir2/impstage-llvm-ir}
seed=$root/stage0/impstage0
[ -x "$two" ] || { echo "tmpfiles test skipped: no $two"; exit 0; }
export IMPSYNTAX=$root/src/llvm-ir/syntax64.imc IMPFFI=$root/lib/ffi.imc IMPLIB=$root/build/lib/libimp.a
w=build/tmpfiles
rm -rf $w; mkdir -p $w
fail=0
bad() { echo "FAILED: tmpfiles: $*" >&2; fail=1; }
echo "== tmpfiles"
cp tests/native/hello.imp $w/hello.imp
printf 'A ARE COMMON;\nA:=;\n%%%%\n' > $w/bad.imp
src=$w/hello.imp
root_user=0; [ "$(id -u)" = 0 ] && root_user=1
dflsig=; env --default-signal=INT,QUIT true >/dev/null 2>&1 && dflsig="env --default-signal=INT,QUIT"
signum() { case $1 in HUP) echo 1 ;; INT) echo 2 ;; QUIT) echo 3 ;; TERM) echo 15 ;; esac; }
# nothing in the directory $1
empty() { [ -z "$(ls -A "$1" 2>/dev/null)" ]; }
fresh() { rm -rf "$1"; mkdir -p "$1"; }
# the program $1 prints what the native test prints
runs() { "$1" 2>&1 | cmp -s - tests/native/hello.out; }
# A compiler $1 started in the background with the directory in use (IMPNOMEMFD=1), the signal $2 sent when the private directory has appeared in $t, for the sources $3 to the program
# $4.  caught=1 if the compiler died of it, rc its status.  (A background command of a shell without job control has SIGINT and SIGQUIT ignored, and the compiler then leaves them so;
# env --default-signal makes them default, where env has it.)
sigtry() {
    fresh $t
    rm -f "$4"; mkfifo "$4"                      # (the output is a FIFO with no reader: the linker blocks opening it, so the compiler holds its directory until the signal comes)
    IMPNOMEMFD=1 TMPDIR=$root/$t $dflsig "$1" -o "$4" $3 >/dev/null 2>&1 &
    pid=$!
    n=0
    while [ $n -lt 300000 ]; do
        set -- "$1" "$2" "$3" "$4" $t/impstage0.*
        [ -e "$5" ] && break
        kill -0 $pid 2>/dev/null || break
        n=$((n + 1))
    done
    kill -$2 $pid 2>/dev/null
    wait $pid 2>/dev/null; rc=$?
    rm -f "$4"
    caught=0; [ $rc -gt 128 ] && caught=1
}
# signals $2 sent to the compiler $1 linking the sources $3: TMPDIR is empty after each, the status is the signal's, and the signal did arrive in some of the tries
sigs() {
    for sig in $2; do
        [ -z "$dflsig" ] && case $sig in INT|QUIT) echo "tmpfiles: no env --default-signal: SIG$sig is not tried"; continue ;; esac
        got=0; try=0
        while [ $got -lt 3 ] && [ $try -lt 60 ]; do
            try=$((try + 1))
            sigtry "$1" $sig "$3" $w/sig.exe
            [ $caught = 1 ] && got=$((got + 1))
            empty $t || bad "$4: SIG$sig left something in TMPDIR: $(ls -A $t)"
            [ $caught = 1 ] && [ $rc != $((128 + $(signum $sig))) ] && bad "$4: SIG$sig: the status is $rc"
        done
        [ $got -gt 0 ] || bad "$4: SIG$sig did not reach the compiler while it had a directory"
    done
}

for c in seed two; do
    if [ $c = seed ]; then bin=$seed; modes="-S -c"; else bin=$two; modes="-S -c -Sir --run"; fi        # (the seed has no runtime to run a program with)
    [ -x "$bin" ] || continue
    # (a) the modes that make no file: with an empty TMPDIR it stays empty, and with one that cannot be written they work
    t=$w/$c.tmp; fresh $t
    for m in $modes; do
        TMPDIR=$root/$t "$bin" $m -o $w/$c.out $src >$w/$c.log 2>&1 || { bad "$c $m"; cat $w/$c.log >&2; }
        empty $t || bad "$c $m left something in TMPDIR: $(ls -A $t)"
        if [ $root_user = 0 ]; then
            chmod 500 $t
            TMPDIR=$root/$t "$bin" $m -o $w/$c.out $src >$w/$c.log 2>&1 || { bad "$c $m with a TMPDIR that cannot be written"; cat $w/$c.log >&2; }
            chmod 700 $t
        fi
    done
    # (b) the link, with the directory (IMPNOMEMFD=1) and with the memory file where there is one
    for nm in 1 0; do
        fresh $t
        rm -f $w/$c.exe
        IMPNOMEMFD=$nm TMPDIR=$root/$t "$bin" -o $w/$c.exe $src >$w/$c.log 2>&1
        r=$?
        [ $r = 0 ] && runs $w/$c.exe || { bad "$c link (IMPNOMEMFD=$nm)"; cat $w/$c.log >&2; }
        empty $t || bad "$c link (IMPNOMEMFD=$nm) left something in TMPDIR: $(ls -A $t)"
    done
    if [ $root_user = 0 ]; then                 # a TMPDIR that cannot be written: the memory file (else the default directory) takes the object
        fresh $t; chmod 500 $t; rm -f $w/$c.exe
        TMPDIR=$root/$t "$bin" -o $w/$c.exe $src >$w/$c.log 2>&1 && runs $w/$c.exe || { bad "$c link with a TMPDIR that cannot be written"; cat $w/$c.log >&2; }
        chmod 700 $t
    fi
    # the directory is made with IMPNOMEMFD=1 (the time of TMPDIR is set far back first, and then it is newer), and a note says whether it is made without
    for nm in 1 0; do
        fresh $t; touch -t 200001010000 $t; touch -t 200001010001 $w/ref; rm -f $w/$c.exe
        IMPNOMEMFD=$nm TMPDIR=$root/$t "$bin" -o $w/$c.exe $src >/dev/null 2>&1
        touched=0; [ -n "$(find $t -prune -newer $w/ref)" ] && touched=1
        [ $nm = 1 ] && [ $touched = 0 ] && bad "$c: IMPNOMEMFD=1 did not use the directory"
        if [ $nm = 0 ]; then if [ $touched = 0 ]; then echo "tmpfiles: $c: the link makes no directory (a memory file)"; else echo "tmpfiles: $c: the link makes a directory"; fi; fi
    done
    # (c) a compile that fails
    for nm in 1 0; do
        fresh $t
        IMPNOMEMFD=$nm TMPDIR=$root/$t "$bin" -o $w/$c.bad $w/bad.imp >$w/$c.log 2>&1 && bad "$c: a program with a syntax error was accepted"
        empty $t || bad "$c: a failed compile left something in TMPDIR: $(ls -A $t)"
    done
    # (d) signals during a link that uses the directory
    t=$w/$c.sig.tmp
    sigs "$bin" "TERM INT HUP QUIT" $src $c
    # (e) a TMPDIR that is not used: relative, a file, not there, empty; the default takes the directory, and the link works
    fresh $w/rel; mkdir $w/rel/dir; : > $w/file
    ( cd $w/rel && IMPNOMEMFD=1 TMPDIR=dir "$bin" -o ../$c.exe ../hello.imp >../$c.log 2>&1 ) && runs $w/$c.exe || bad "$c link with a relative TMPDIR"
    empty $w/rel/dir || bad "$c used a relative TMPDIR: $(ls -A $w/rel/dir)"
    rm -f $w/$c.exe
    IMPNOMEMFD=1 TMPDIR=$root/$w/file "$bin" -o $w/$c.exe $src >$w/$c.log 2>&1 && runs $w/$c.exe || bad "$c link with a file as TMPDIR"
    [ -f $w/file ] && [ ! -s $w/file ] || bad "$c touched the file that TMPDIR names"
    rm -f $w/$c.exe
    IMPNOMEMFD=1 TMPDIR=$root/$w/nosuch/dir "$bin" -o $w/$c.exe $src >$w/$c.log 2>&1 && runs $w/$c.exe || bad "$c link with a TMPDIR that is not there"
    [ ! -e $w/nosuch ] || bad "$c made the directory that TMPDIR names"
    rm -f $w/$c.exe
    IMPNOMEMFD=1 TMPDIR= "$bin" -o $w/$c.exe $src >$w/$c.log 2>&1 && runs $w/$c.exe || bad "$c link with an empty TMPDIR"
done

# imp compiles several sources with a process for each (no shell): the objects are in the private directory
cat > $w/m1.imp <<'E'
N ARE COMMON;
N:=TWICE(21);
PRINT STG 0,'ANSWER ',IGR 0,N,/;
FINI(0)
%%
E
printf 'SUBR TWICE(A) IS A*2\n%%%%\n' > $w/m2.imp
t=$w/multi.tmp
for nm in 1 0; do
    fresh $t; rm -f $w/multi.exe
    IMPNOMEMFD=$nm TMPDIR=$root/$t "$two" -o $w/multi.exe $w/m1.imp $w/m2.imp >$w/multi.log 2>&1 && [ "$($w/multi.exe)" = "ANSWER 42" ] || { bad "a link of two sources (IMPNOMEMFD=$nm)"; cat $w/multi.log >&2; }
    empty $t || bad "two sources left something in TMPDIR: $(ls -A $t)"
done
sigs "$two" "TERM INT" "$w/m1.imp $w/m2.imp" multi
[ $fail = 0 ] && echo ok
exit $fail
