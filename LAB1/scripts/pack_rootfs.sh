#!/bin/bash
# LAB-01 - Dung rootfs tu BusyBox + overlay, dong goi initramfs.cpio.gz
set -e

WORKDIR="$HOME/embedded_lab1"
BVER="1.35.0"
REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
RFS="$WORKDIR/rootfs/initramfs"

rm -rf "$RFS"
mkdir -p "$RFS"/{bin,sbin,usr/bin,usr/sbin,dev,proc,sys,tmp,etc/init.d,lib/arm-linux-gnueabihf}

cp -r "$WORKDIR/rootfs/busybox-$BVER/_install/bin/"* "$RFS/bin/"
cp -r "$WORKDIR/rootfs/busybox-$BVER/_install/sbin/"* "$RFS/sbin/"
cp -r "$WORKDIR/rootfs/busybox-$BVER/_install/usr/"* "$RFS/usr/"

cp -r "$REPO_DIR/rootfs-overlay/"* "$RFS/"
chmod +x "$RFS/etc/init.d/rcS"

echo 'root:x:0:0:root:/root:/bin/sh' > "$RFS/etc/passwd"
echo 'embedded-lab' > "$RFS/etc/hostname"

sudo mknod -m 666 "$RFS/dev/null" c 1 3
sudo mknod -m 600 "$RFS/dev/console" c 5 1
sudo mknod -m 666 "$RFS/dev/zero" c 1 5
sudo mknod -m 444 "$RFS/dev/random" c 1 8

sudo chown -R "$(whoami):$(whoami)" "$RFS"

cd "$RFS"
find . | cpio -H newc -o | gzip > "$WORKDIR/output/initramfs.cpio.gz"

ls -lh "$WORKDIR/output/initramfs.cpio.gz"
echo "=== rootfs pack DONE ==="
