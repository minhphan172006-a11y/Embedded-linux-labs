#!/bin/bash
# LAB-02 - Copy rootfs tu LAB-01, them lab2_driver.ko + test scripts + rcS moi, dong goi
set -e

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LAB1_WORKDIR="$HOME/embedded_lab1"
WORKDIR="$HOME/embedded_lab2"
KVER="6.6.21"

rm -rf "$WORKDIR"
mkdir -p "$WORKDIR"/{driver,rootfs,output}
mkdir -p "$WORKDIR/rootfs/initramfs"

if [ ! -d "$LAB1_WORKDIR/rootfs/initramfs" ]; then
    echo "ERROR: Chua thay rootfs LAB-01 tai $LAB1_WORKDIR/rootfs/initramfs"
    echo "Chay LAB01-kernel-boot/scripts/pack_rootfs.sh truoc."
    exit 1
fi

sudo cp -r "$LAB1_WORKDIR/rootfs/initramfs/"* "$WORKDIR/rootfs/initramfs/"
sudo chown -R "$(whoami):$(whoami)" "$WORKDIR/rootfs/initramfs"

# Copy driver da build
mkdir -p "$WORKDIR/rootfs/initramfs/lib/modules/$KVER"
cp "$REPO_DIR/driver/lab2_driver.ko" "$WORKDIR/rootfs/initramfs/lib/modules/$KVER/"

# Ap overlay LAB-02 (rcS moi co auto-load, test scripts) - ghi de rcS cua LAB-01
cp -r "$REPO_DIR/rootfs-overlay/"* "$WORKDIR/rootfs/initramfs/"
chmod +x "$WORKDIR/rootfs/initramfs/etc/init.d/rcS"
chmod +x "$WORKDIR/rootfs/initramfs/usr/bin/test_driver.sh"
chmod +x "$WORKDIR/rootfs/initramfs/usr/bin/test_procfs.sh"

find "$WORKDIR/rootfs/initramfs" -not -user "$(whoami)" && \
    echo "WARNING: con file khong thuoc $(whoami) o tren" || true

cd "$WORKDIR/rootfs/initramfs"
find . | cpio -o -H newc | gzip > "$WORKDIR/output/initramfs.cpio.gz"

ls -lh "$WORKDIR/output/initramfs.cpio.gz"
echo "=== rootfs pack DONE: $WORKDIR/output/initramfs.cpio.gz ==="
