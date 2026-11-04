#!/bin/sh
# The version is in VERSION; each place that names it must agree (the compiler's --version and banner, the tests that expect them, the packages, the port, the manual).
# Run from the top of the tree.
root=$(cd "$(dirname "$0")/../.." && pwd)
cd "$root" || exit 1
v=$(cat VERSION)
echo "== version $v"
fail=0
chk() { grep -q -F -e "$2" "$1" || { echo "FAILED: version: $1 does not say $2" >&2; fail=1; }; }
chk src/driver.imp "imp72 $v"
chk src/impm.imp "IMP72 $v"
chk tests/cli/run.sh "VERSION"
chk tests/interactive/run.sh "VERSION"
chk debian/changelog "imp72 ($v)"
chk packaging/alpine/APKBUILD "pkgver=$v"
chk packaging/openbsd/lang/imp72/Makefile "GH_TAGNAME =		v$v"
chk docs/manual/imp72.rno "$v"
chk docs/manual/imp72.md "$v"
chk docs/manual/imp72.7 "$v"
[ $fail = 0 ] && echo ok
exit $fail
