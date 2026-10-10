#!/bin/bash
# LAB-01 - Build BusyBox 1.35.0 static
# Da sua san: (1) menuconfig loi ncurses -> sua .config bang sed
#             (2) tc.o loi CBQ           -> tat CONFIG_TC
set -e

WORKDIR="$HOME/embedded_lab1"
BVER="1.35.0"
REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

mkdir -p "$WORKDIR/rootfs" "$WORKDIR/configs"
cd "$WORKDIR/rootfs"

if [ ! -d "busybox-$BVER" ]; then
    wget -c "https://busybox.net/downloads/busybox-$BVER.tar.bz2"
    tar -xf "busybox-$BVER.tar.bz2"
fi
cd "busybox-$BVER"

export ARCH=arm
export CROSS_COMPILE=arm-linux-gnueabihf-

make defconfig

sed -i 's/^# CONFIG_STATIC is not set/CONFIG_STATIC=y/' .config
grep -q "^CONFIG_STATIC=y" .config || echo "CONFIG_STATIC=y" >> .config

sed -i 's/^CONFIG_TC=y/# CONFIG_TC is not set/' .config
grep -q "^CONFIG_TC" .config || echo "# CONFIG_TC is not set" >> .config

make -j"$(nproc)"
make install

file _install/bin/busybox
cp .config "$WORKDIR/configs/busybox.config"
cp .config "$REPO_DIR/configs/busybox.config" 2>/dev/null || true

echo "=== BusyBox build DONE ==="
