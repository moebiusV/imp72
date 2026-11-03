#!/bin/sh
# bsd-vm.sh COMMAND OS [ARGS]: a BSD virtual machine of our own, the one CI runs (OpenBSD 8.0 or NetBSD 11.0, amd64), so that a failure of the BSD jobs can be looked into here: the prebuilt
# images of the cross-platform-actions project (the ones the CI action boots) under QEMU with KVM, in a container that has the QEMU, so that nothing but docker and /dev/kvm is needed on the host.
# The files are in tmp/vm/OS (the tree's own scratch directory, never in git); the images are downloaded once and stay (OpenBSD 0.8 GB, NetBSD 0.3 GB, 2.7 GB with the disk extracted).
#
#   bsd-vm.sh fetch OS        download the image (resumes; checks the size)
#   bsd-vm.sh start OS        boot a fresh machine on a copy-on-write overlay of the image (the image stays as it was), install what CI installs (gmake, and the LLVM on NetBSD)
#   bsd-vm.sh ssh OS [CMD]    a shell on it, or the command, as the user runner
#   bsd-vm.sh ci OS [check]   copy the tree (the files git knows, with the changes) and run what CI runs there: tools/ci-run.sh OS (the build, then the suite); `check` runs only gmake check;
#                             the output is in tmp/vm/OS/run.log, the status is the exit status
#   bsd-vm.sh status OS       is it running
#   bsd-vm.sh stop OS         halt it from inside (an OpenBSD killed in the middle of the relinking it does at each boot loses files: it needs the overlay made again), then stop the container
#   bsd-vm.sh clean OS        forget the machine (the overlay and the container); the image stays
# VM_CPUS names the CPUs the machine runs on (6-9; never 5, which is somebody's), VM_MEM its memory (5G).
set -e
cmd=$1 os=$2; shift 2 2>/dev/null || true
root=$(cd "$(dirname "$0")/../.." && pwd)
here=$root/tools/bsd-vm
dir=$root/tmp/vm/$os
ctr=imp72-vm-$os
cpus=${VM_CPUS:-6-9}
mem=${VM_MEM:-5G}
die() { echo "bsd-vm: $*" >&2; exit 1; }

case $os in
openbsd)
    url=https://github.com/cross-platform-actions/openbsd-builder/releases/download/v0.15.0/openbsd-8.0-x86-64.qcow2
    img=openbsd-8.0-x86-64.qcow2; size=815595520; port=2222 ;;
netbsd)
    url=https://github.com/cross-platform-actions/netbsd-builder/releases/download/v1.2.0/netbsd-11.0-x86-64.tar.zst
    img=netbsd-11.0-x86-64.tar.zst; size=279738071; port=2223 ;;
*) die "usage: bsd-vm.sh fetch|start|ssh|ci|status|stop|clean openbsd|netbsd" ;;
esac

inctr() { docker exec -i "$ctr" "$@"; }                  # (-i: the tree of `ci` comes in on the standard input; the others read none: </dev/null)
# a command on the machine as runner (the image says how it is let in: OpenBSD with the password runner, NetBSD with none)
vm() {
    if [ $os = openbsd ]; then
        inctr sshpass -p runner ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR -o ConnectTimeout=30 -o ServerAliveInterval=30 -p $port runner@127.0.0.1 "$@"
    else
        inctr ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR -o ConnectTimeout=30 -o ServerAliveInterval=30 -o PreferredAuthentications=none -p $port runner@127.0.0.1 "$@"
    fi
}
rootvm() {
    inctr sshpass -p runner ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR -o ConnectTimeout=30 -o PubkeyAuthentication=no -p $port root@127.0.0.1 "$@"
}
up() { vm 'uname -s' </dev/null 2>/dev/null | grep -qi bsd; }
running() { docker ps --format '{{.Names}}' | grep -qx "$ctr" && inctr pgrep qemu-system </dev/null >/dev/null 2>&1; }

case $cmd in
fetch)
    mkdir -p "$dir/img"
    [ "$(wc -c < "$dir/img/$img" 2>/dev/null)" = $size ] || curl -fSL -C - -o "$dir/img/$img" "$url"
    [ "$(wc -c < "$dir/img/$img")" = $size ] || die "$img is not $size bytes"
    if [ $os = netbsd ] && [ ! -f "$dir/vm/disk.img" ]; then
        mkdir -p "$dir/vm"
        docker run --rm -v "$dir:/d" alpine:edge sh -c 'apk add --no-cache zstd tar >/dev/null 2>&1; cd /d/vm && zstd -d -c /d/img/'$img' | tar -xf -'
    fi
    echo "bsd-vm: $os image is in $dir" ;;
start)
    [ -f "$dir/img/$img" ] || die "no image: bsd-vm.sh fetch $os"
    running && die "$os is running"
    docker rm -f "$ctr" >/dev/null 2>&1 || true
    docker run -d --name "$ctr" --device /dev/kvm --cpuset-cpus "$cpus" --memory 10g -v "$dir:/vm" -v "$here:/tools:ro" alpine:edge sleep infinity >/dev/null
    inctr sh -c 'apk add --no-cache qemu-system-x86_64 qemu-img openssh-client sshpass python3 >/dev/null 2>&1' || die "cannot install qemu in the container"
    rm -f "$dir"/work.qcow2 "$dir"/vars.fd "$dir"/*.sock "$dir"/console.log
    if [ $os = openbsd ]; then
        inctr sh -c 'cd /vm && qemu-img create -q -f qcow2 -b img/'$img' -F qcow2 work.qcow2 40G && cp /usr/share/qemu/edk2-i386-vars.fd vars.fd'
        # the image boots with UEFI; its disk and network are virtio
        inctr sh -c 'qemu-system-x86_64 -enable-kvm -cpu host -m '$mem' -smp 4 -machine pc -display none -chardev socket,id=ser,path=/vm/ser.sock,server=on,wait=off,logfile=/vm/console.log -serial chardev:ser -monitor unix:/vm/mon.sock,server=on,wait=off -drive if=pflash,format=raw,readonly=on,file=/usr/share/qemu/edk2-x86_64-code.fd -drive if=pflash,format=raw,file=/vm/vars.fd -drive file=/vm/work.qcow2,if=virtio -netdev user,id=n0,hostfwd=tcp::'$port'-:22 -device virtio-net-pci,netdev=n0 >/vm/qemu.log 2>&1 &'
        # its network card boots with no carrier (and so no address): log in on the console (the only way in) and bring the interface up again
        echo "bsd-vm: waiting for the console of $os (about a minute)"
        i=0; while ! inctr grep -q 'starting local daemons' /vm/console.log 2>/dev/null; do i=$((i+1)); [ $i -gt 90 ] && die "the machine did not boot"; sleep 5; done
        sleep 25
        inctr python3 /tools/type.py root runner 'ifconfig vio0 down; ifconfig vio0 up; sleep 3; dhcpleasectl vio0' >/dev/null
    else
        inctr sh -c 'cd /vm && qemu-img create -q -f qcow2 -b vm/disk.img -F raw work.qcow2 40G'
        inctr sh -c 'qemu-system-x86_64 -enable-kvm -cpu host -m '$mem' -smp 4 -machine pc -display none -chardev socket,id=ser,path=/vm/ser.sock,server=on,wait=off,logfile=/vm/console.log -serial chardev:ser -monitor unix:/vm/mon.sock,server=on,wait=off -device virtio-scsi-pci -device scsi-hd,drive=drive0,bootindex=0 -drive if=none,file=/vm/work.qcow2,id=drive0,format=qcow2 -netdev user,id=n0,hostfwd=tcp::'$port'-:22 -device virtio-net-pci,netdev=n0 >/vm/qemu.log 2>&1 &'
    fi
    i=0; until up; do i=$((i+1)); [ $i -gt 90 ] && die "no ssh to $os"; sleep 5; done
    # what CI installs on the machine
    if [ $os = openbsd ]; then
        vm 'sudo pkg_add gmake' </dev/null >/dev/null 2>&1 || die "pkg_add gmake failed"
    else
        rootvm </dev/null 'PKG_PATH=https://cdn.NetBSD.org/pub/pkgsrc/packages/NetBSD/amd64/11.0/All /usr/sbin/pkg_add gmake llvm lld' >/dev/null 2>&1 || die "pkg_add failed"
    fi
    echo "bsd-vm: $os is up: bsd-vm.sh ssh $os" ;;
ssh)
    running || die "$os is not running: bsd-vm.sh start $os"
    vm "$@" ;;
ci)
    running || die "$os is not running: bsd-vm.sh start $os"
    what=${1:-all}
    ( cd "$root" && git ls-files -co --exclude-standard -z | tar --null -T - -cf "$dir/tree.tar" )
    inctr sh -c 'cat /vm/tree.tar' | vm 'rm -rf imp72 && mkdir imp72 && tar -xf - -C imp72'
    path='export PATH=/usr/pkg/bin:/usr/pkg/sbin:$PATH; '
    if [ "$what" = check ]; then run="gmake check"; else run="sh tools/ci-run.sh $os"; fi
    rm -f "$dir/run.log"
    st=0
    vm "cd imp72 && $path$run" </dev/null > "$dir/run.log" 2>&1 || st=$?
    tail -n 5 "$dir/run.log" | cut -c1-200
    echo "bsd-vm: $os: $run exited $st (the output is in $dir/run.log)"
    exit $st ;;
status)
    if running; then if up; then echo "$os: running, ssh works"; else echo "$os: running, no ssh yet"; fi; else echo "$os: not running"; fi ;;
stop)
    running || { docker stop "$ctr" >/dev/null 2>&1 || true; echo "$os: not running"; exit 0; }
    if [ $os = openbsd ]; then vm 'sudo shutdown -p now' </dev/null >/dev/null 2>&1 || true; else vm 'sudo /sbin/shutdown -p now' </dev/null >/dev/null 2>&1 || true; fi
    i=0; while inctr pgrep qemu-system </dev/null >/dev/null 2>&1; do i=$((i+1)); [ $i -gt 30 ] && die "the machine did not halt"; sleep 5; done
    docker stop "$ctr" >/dev/null; echo "$os: stopped" ;;
clean)
    docker rm -f "$ctr" >/dev/null 2>&1 || true
    rm -f "$dir"/work.qcow2 "$dir"/vars.fd "$dir"/*.sock "$dir"/console.log "$dir"/qemu.log "$dir"/tree.tar "$dir"/run.log
    echo "$os: forgotten (the image is kept in $dir)" ;;
*) die "usage: bsd-vm.sh fetch|start|ssh|ci|status|stop|clean openbsd|netbsd" ;;
esac
