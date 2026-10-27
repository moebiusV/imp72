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
grep -Eq 'IMP72 0\.9\.0 +( |[0-9])[0-9]-[A-Z]{3}-[0-9]{2}' $out/session.out || { echo "FAILED: interactive: no banner with the date" >&2; fail=1; }
# the same through the imp72 driver (-i)
w=${INTERACTIVE_IMP72:-build/pccir2/imp72stage-pccir}
if [ -x "$w" ]; then
    mkdir -p $out/w && cp tests/native/hello.imp $out/w/
    ( cd $out/w && printf 'hello/C\nx_/U\n' | CC1=$root/build/pcc/cc/ccom/cc1 $root/$w -i >session.out 2>&1 )
    if [ "$(uname -m)" = x86_64 ] && [ "$(getconf LONG_BIT)" = 64 ]; then       # the driver compiles for the machine it is on; the reference is for amd64
        cmp -s $out/w/hello.ir $out/hello.ref || { echo "FAILED: interactive: imp72 -i: hello.ir differs" >&2; fail=1; }
    else
        [ -s $out/w/hello.ir ] || { echo "FAILED: interactive: imp72 -i: no hello.ir" >&2; fail=1; }
    fi
    grep -q 'SAVED' $out/w/session.out || { echo "FAILED: interactive: imp72 -i: /U did not save" >&2; fail=1; }
fi
[ $fail = 0 ] && echo ok
exit $fail
