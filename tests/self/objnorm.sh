#!/bin/sh
# tools/objnorm.py must make two compilations of the same source agree: compile a file twice, with debug information, from
# two directories, and compare the normalised objects.  (On OpenBSD the raw objects differ: RETGUARD cookies.)
root=$(cd "$(dirname "$0")/../.." && pwd)
cd "$root" || exit 1
command -v python3 >/dev/null 2>&1 || { echo "objnorm test skipped: no python3"; exit 0; }
case "$(uname)" in Darwin) echo "objnorm test skipped: not ELF"; exit 0 ;; esac
t=build/objnorm
rm -rf $t; mkdir -p $t/a $t/b
cat > $t/a/t.c <<'EOT'
#include <stdio.h>
int g;
static int h(int x) { char b[64]; snprintf(b, sizeof b, "%d", x + g); return b[0]; }
int f(int x) { return h(x) + h(x + 1); }
int main(void) { printf("%d\n", f(3)); return 0; }
EOT
cp $t/a/t.c $t/b/t.c
(cd $t/a && cc -g -O1 -c t.c) 2>/dev/null && (cd $t/b && cc -g -O1 -c t.c) 2>/dev/null || { echo "objnorm test skipped: no cc"; exit 0; }
python3 tools/objnorm.py $t/a/t.o > $t/a.txt
python3 tools/objnorm.py $t/b/t.o > $t/b.txt
[ -s $t/a.txt ] && cmp -s $t/a.txt $t/b.txt || { diff $t/a.txt $t/b.txt | head; echo "FAILED: objnorm" >&2; exit 1; }
# and it must still tell different code apart
sed 's/h(x + 1)/h(x + 2)/' $t/a/t.c > $t/b/t.c
(cd $t/b && cc -g -O1 -c t.c) 2>/dev/null
python3 tools/objnorm.py $t/b/t.o > $t/b.txt
cmp -s $t/a.txt $t/b.txt && { echo "FAILED: objnorm does not see a change" >&2; exit 1; }
echo ok
