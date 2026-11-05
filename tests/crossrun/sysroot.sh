#!/bin/sh
# sysroot.sh ARCH DIR: make the sysroot of a Debian architecture (s390x, powerpc, i386, amd64) in DIR from the .deb files of the prep inputs (PREP_INPUTS, by default
# /home/matthew/claude/prebsd/mirror/dev/inputs): the C library and its start files, the shared library of the compiler's runtime support, and the static one with the
# builtins.  Exit 77 where the inputs are not here.  Nothing is downloaded.
arch=$1
dir=$2
in=${PREP_INPUTS:-/home/matthew/claude/prebsd/mirror/dev/inputs}
[ -n "$arch" ] && [ -n "$dir" ] || { echo "usage: sysroot.sh ARCH DIR" >&2; exit 2; }
pool=$in/debian-$arch/pool
[ -d "$pool" ] || pool=$in/debian-$arch/pool-$arch
[ -d "$pool" ] || { echo "sysroot: no $in/debian-$arch: skipped"; exit 77; }
command -v dpkg >/dev/null 2>&1 || { echo "sysroot: no dpkg: skipped"; exit 77; }
[ -f "$dir/.done" ] && exit 0
rm -rf "$dir"
mkdir -p "$dir"
n=0
for pkg in libc6 libc6-dev libgcc-s1 libgcc-16-dev; do
    f=$(find "$in/debian-$arch" -name "${pkg}_*_$arch.deb" 2>/dev/null | head -n 1)
    [ -n "$f" ] || continue
    dpkg -x "$f" "$dir" || { echo "sysroot: $f did not unpack" >&2; exit 1; }
    n=$((n + 1))
done
[ $n -ge 2 ] || { echo "sysroot: the C library of $arch is not in $in: skipped"; rm -rf "$dir"; exit 77; }
for d in lib lib64 lib32 libx32; do                 # (Debian merges /lib into /usr/lib: the loader is named /lib/ld64.so.1)
    [ -e "$dir/$d" ] || [ ! -d "$dir/usr/$d" ] || ln -s usr/$d "$dir/$d"
done
touch "$dir/.done"
exit 0
