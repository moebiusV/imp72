#!/bin/sh
# token-diff.sh [OUTDIR]: the tape (decus/) and the port (src/ and lib/) compared token by token, as HTML in OUTDIR (build/token-diff by default; open OUTDIR/index.html).
# Each module of decus/ is compared with the file of the same name in src/ or in lib/; the modules of src/ and lib/ that the tape has not are listed as new.  The program is
# tools/token-diff.imp (IMP, built by the compiler of the tree: tools/imptool.sh), which has the rules.
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root" || exit 1
out=${1:-build/token-diff}
mkdir -p "$out" || exit 1
pairs=
for f in decus/*.imc decus/*.iml; do
    [ -f "$f" ] || continue
    n=$(basename "$f")
    if [ -f "src/$n" ]; then new=src/$n
    elif [ -f "lib/$n" ]; then new=lib/$n
    else new=-
    fi
    pairs="$pairs $f $new"
done
for f in src/*.imc lib/*.iml; do
    [ -f "$f" ] || continue
    n=$(basename "$f")
    [ -f "decus/$n" ] && continue
    pairs="$pairs - $f"
done
# shellcheck disable=SC2086
note=
if command -v git >/dev/null 2>&1 && git rev-parse --short HEAD >/dev/null 2>&1; then
    note="Generated from decus/ and src/ at commit $(git rev-parse --short HEAD) ($(git log -1 --format=%cs))."
fi
if [ -n "$note" ]; then sh tools/imptool.sh token-diff -n "$note" "$out" $pairs || exit 1
else sh tools/imptool.sh token-diff "$out" $pairs || exit 1
fi
echo "token-diff: $out/index.html"
