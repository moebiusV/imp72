#!/bin/sh
# The interactive mode of the compiler proper (imp72d -i): the commands of the original, read from the terminal: *REL,LST_FILE.EXT/A/B...
# The IR written for a command must be that of the same file named on the command line.  Run from the top of the tree (make check does).
root=$(cd "$(dirname "$0")/../.." && pwd)
cd "$root" || exit 1
d=${INTERACTIVE_IMPD:-build/pccir2/imp72d}
[ -x "$d" ] || { echo "interactive test skipped: no $d"; exit 0; }
syn="$root/src/pccir/syntax64.imc $root/lib/ffi.imc"
out=build/interactive
rm -rf $out; mkdir -p $out
cp tests/native/hello.imp tests/native/basic.imp $out/
fail=0
echo "== interactive"
$d -c $out/hello.ref $syn $out/hello.imp >/dev/null 2>&1
( cd $out && printf '?\n/H\nnosuch\nhello/C\nmine,list_basic/L\n' | $root/$d -i $syn >session.out 2>&1 ) || { echo "FAILED: interactive: exit status" >&2; fail=1; }
cmp -s $out/hello.ir $out/hello.ref || { echo "FAILED: interactive: hello.ir differs from the IR of the command line" >&2; fail=1; }
[ -s $out/mine.ir ] || { echo "FAILED: interactive: REL file mine.ir not written" >&2; fail=1; }
[ -s $out/list.lst ] || { echo "FAILED: interactive: LST file list.lst not written" >&2; fail=1; }
grep -q 'TYPE /H FOR HELP' $out/session.out || { echo "FAILED: interactive: no answer to ?" >&2; fail=1; }
grep -q 'SINGLE LETTER SWITCHES' $out/session.out || { echo "FAILED: interactive: no answer to /H" >&2; fail=1; }
grep -q 'FILE NOT FOUND' $out/session.out || { echo "FAILED: interactive: no complaint about a missing file" >&2; fail=1; }
[ $fail = 0 ] && echo ok
exit $fail
