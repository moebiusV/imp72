#!/bin/sh
# tools/token-diff.imp (an IMP program, built by the compiler of the tree: tools/imptool.sh) compares two versions of a program token by token and writes HTML:
#  - tests/tokdiff/old.imc (the original's IMP: the assignment is _, the lines end in CR LF) and new.imc (the port's: :=) give tests/tokdiff/expected.html: white space is no difference,
#    _ replaced by := is an ordinary replacement, an apostrophe in a comment starts no text, '' in a text and !...! are one token each;
#  - a file that is not there (-) is all old; two pairs of the same name are refused; the index lists the pages;
#  - the whole tape against the port (tools/token-diff.sh) is written, and every module of the tape has its page.
root=$(cd "$(dirname "$0")/../.." && pwd)
cd "$root" || exit 1
t=tmp/tokdiff-test
rm -rf "$t"; mkdir -p "$t" build
fail=0
sh tools/imptool.sh token-diff "$t" tests/tokdiff/old.imc tests/tokdiff/new.imc tests/tokdiff/old.imc - || { echo "FAILED: token-diff" >&2; exit 1; }
sh tools/imptool.sh token-diff "$t" tests/tokdiff/old.imc tests/tokdiff/new.imc tests/tokdiff/old.imc tests/tokdiff/new.imc >/dev/null 2>"$t/dup.err" && { echo "FAILED: tokdiff: two pairs of the same name were not refused" >&2; fail=1; }
cmp -s "$t/new.imc.html" tests/tokdiff/expected.html || { diff "$t/new.imc.html" tests/tokdiff/expected.html | head -5; echo "FAILED: tokdiff: the page of old.imc and new.imc is not expected.html" >&2; fail=1; }
grep -q '<del>_</del><ins>:=</ins>' "$t/new.imc.html" || { echo "FAILED: tokdiff: _ replaced by := is not shown as a replacement" >&2; fail=1; }
grep -q 'IT.S ONLY TEXT' "$t/new.imc.html" && ! grep -q '<del>[^<]*IT' "$t/new.imc.html" || { echo "FAILED: tokdiff: the comment is marked" >&2; fail=1; }
[ -f "$t/index.html" ] && grep -q 'new.imc.html' "$t/index.html" || { echo "FAILED: tokdiff: the index does not list the page" >&2; fail=1; }
sh tools/token-diff.sh "$t/all" >/dev/null || { echo "FAILED: tokdiff: tools/token-diff.sh" >&2; fail=1; }
for f in decus/*.imc decus/*.iml; do
    [ -f "$t/all/$(basename "$f").html" ] || { echo "FAILED: tokdiff: no page for $f" >&2; fail=1; }
done
rm -rf "$t"
[ $fail = 0 ] && echo ok
exit $fail
