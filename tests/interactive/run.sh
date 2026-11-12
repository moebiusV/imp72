#!/bin/sh
# The interactive mode of the compiler: a bare imp, with no file, reads the commands of the original from the standard input (a terminal, a pipe): *REL,LST_FILE.EXT/A/B...
# Run from the top of the tree (make check does).
root=$(cd "$(dirname "$0")/../.." && pwd)
cd "$root" || exit 1
w=${INTERACTIVE_IMP:-build/llvm-ir2/impstage-llvm-ir}
[ -x "$w" ] || { echo "interactive test skipped: no $w"; exit 0; }
out=build/interactive
rm -rf $out; mkdir -p $out
cp tests/native/hello.imp tests/native/basic.imp $out/
fail=0
echo "== interactive"
( cd $out && printf '?\n/H\nnosuch\nhello/C\nmine,list_basic/L\n' | $root/$w >session.out 2>&1 ) || { echo "FAILED: interactive: exit status" >&2; fail=1; }
[ -s $out/hello.o ] && [ ! -e $out/hello.ir ] || { echo "FAILED: interactive: hello.o (the REL file) not written, or the IR left" >&2; fail=1; }      # (the driver writes the object, and the IR is not kept)
[ -s $out/mine.o ] || { echo "FAILED: interactive: REL file mine.o not written" >&2; fail=1; }
[ -s $out/list.lst ] || { echo "FAILED: interactive: LST file list.lst not written" >&2; fail=1; }
grep -q 'TYPE /H FOR HELP' $out/session.out || { echo "FAILED: interactive: no answer to ?" >&2; fail=1; }
grep -q 'SINGLE LETTER SWITCHES' $out/session.out || { echo "FAILED: interactive: no answer to /H" >&2; fail=1; }
grep -q 'FILE NOT FOUND' $out/session.out || { echo "FAILED: interactive: no complaint about a missing file" >&2; fail=1; }
grep -Eq "IMP72 $(sed 's/\./\\./g' "$root/VERSION") +[0-9]{4}-[0-9]{2}-[0-9]{2}" $out/session.out || { echo "FAILED: interactive: no banner with the date" >&2; fail=1; }
mkdir -p $out/w && cp tests/native/hello.imp $out/w/
( cd $out/w && printf 'hello/C\nx_/U\n' | $root/$w >session.out 2>&1 )
[ -s $out/w/hello.o ] && [ ! -e $out/w/hello.ir ] || { echo "FAILED: interactive: a bare imp: hello.o not written, or the IR left" >&2; fail=1; }
( cd $out/w && printf '/D\n' | $root/$w >dswitch.out 2>&1 )
grep -q 'D = TYPE THIS LIST OF DEBUGGING SWITCHES' $out/w/dswitch.out || { echo "FAILED: interactive: a bare imp: /D did not list the debugging switches" >&2; fail=1; }
grep -q 'SAVED' $out/w/session.out || { echo "FAILED: interactive: a bare imp: /U did not save" >&2; fail=1; }
[ $fail = 0 ] && echo ok
exit $fail
