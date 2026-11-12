#!/bin/sh
# wine-setup.sh [DIR]: what the test of the Windows target (tests/winrun) needs, from the Debian packages, into DIR (tmp/win by default) and nothing installed: a mingw-w64 sysroot (DIR/root/usr/x86_64-w64-mingw32, with its libgcc.a) and a Wine
# (DIR/wine) that DIR/run.sh starts for a Windows program.  Wine is unpacked from its packages and not installed, so its loader and its server are put where they look, /usr/share/wine is given to it in a user and mount namespace (unshare -Urm: an overlay of DIR/ovl
# on /usr/share, and a private /tmp), and the one dll that Debian's libwine leaves to another package (zlib1.dll) is taken from it.  apt-get download checks the packages against the archive's signatures.  Exit 77, nothing done, where there is no apt-get, no
# dpkg-deb, no unshare or no network; a second run does nothing (DIR/ready).  The packages are Debian's (trixie); nothing of them goes into the tree but DIR, which is not committed.
root=$(cd "$(dirname "$0")/.." && pwd)
d=${1:-$root/tmp/win}
for c in apt-get dpkg-deb unshare; do command -v $c >/dev/null 2>&1 || { echo "wine-setup: no $c" >&2; exit 77; }; done
[ -f "$d/ready" ] && exit 0
mkdir -p "$d/dl" "$d/root" "$d/wine" "$d/zroot" "$d/ovl/upper" "$d/ovl/work" "$d/prefix" || exit 1
d=$(cd "$d" && pwd)
cd "$d/dl" || exit 1
get() { apt-get download "$@" >/dev/null 2>&1; }
get mingw-w64-x86-64-dev gcc-mingw-w64-x86-64-win32 mingw-w64-common gcc-mingw-w64-x86-64-win32-runtime wine64 libwine libz-mingw-w64 || { for p in mingw-w64-x86-64-dev gcc-mingw-w64-x86-64-win32 mingw-w64-common wine64 libwine libz-mingw-w64; do get $p || { echo "wine-setup: cannot download $p" >&2; exit 77; }; done; }
for f in *.deb; do
    case $f in libz-mingw-w64*) dpkg-deb -x $f "$d/zroot" ;; wine64*|libwine*) dpkg-deb -x $f "$d/wine" ;; *) dpkg-deb -x $f "$d/root" ;; esac
done
sys=$d/root/usr/x86_64-w64-mingw32/lib
cp "$(ls $d/root/usr/lib/gcc/x86_64-w64-mingw32/*/libgcc.a | head -1)" $sys/ || exit 1                               # (the driver names the libraries of one directory)
cp "$d/zroot/usr/x86_64-w64-mingw32/lib/zlib1.dll" "$d/wine/usr/lib/x86_64-linux-gnu/wine/x86_64-windows/" || exit 1
ln -sf wineserver64 "$d/wine/usr/lib/wine/wineserver"
rm -rf "$d/ovl/upper/wine"; cp -r "$d/wine/usr/share/wine" "$d/ovl/upper/wine"
cat > "$d/run.sh" <<RUN
#!/bin/sh
# run.sh PROGRAM.EXE [ARGUMENTS]: a Windows program under the Wine of this directory (made by tools/wine-setup.sh), in a user and mount namespace that gives it /usr/share/wine and a private /tmp.  TMPDIR must not be set: Wine crashes with it.
exec unshare -Urm sh -c "mount -t overlay overlay -o lowerdir=/usr/share,upperdir=$d/ovl/upper,workdir=$d/ovl/work /usr/share && mount -t tmpfs tmpfs /tmp && export WINEPREFIX=$d/prefix WINEDEBUG=\\\${WINEDEBUG:--all} LD_LIBRARY_PATH=$d/wine/usr/lib/x86_64-linux-gnu && exec $d/wine/usr/lib/wine/wine64 \\"\\\$@\\"" sh "\$@"
RUN
chmod +x "$d/run.sh"
# the first run makes the prefix (wineboot): it must work before the directory is ready
printf 'int main(void){return 0;}\n' > "$d/probe.c"
env -u TMPDIR "$d/run.sh" --version >/dev/null 2>&1 || { echo "wine-setup: wine does not start" >&2; exit 77; }
touch "$d/ready"
exit 0
