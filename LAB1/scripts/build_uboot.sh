#!/bin/bash
# LAB-01 - Build U-Boot 2022.04 cho vexpress-a9
set -e

WORKDIR="$HOME/embedded_lab1"
UVER="2022.04"

mkdir -p "$WORKDIR/uboot" "$WORKDIR/output"
cd "$WORKDIR/uboot"

if [ ! -d "u-boot-$UVER" ]; then
    wget -c "https://ftp.denx.de/pub/u-boot/u-boot-$UVER.tar.bz2"
    tar -xf "u-boot-$UVER.tar.bz2"
fi
cd "u-boot-$UVER"

export ARCH=arm
export CROSS_COMPILE=arm-linux-gnueabihf-

make vexpress_ca9x4_defconfig
make -j"$(nproc)"

file u-boot
cp u-boot "$WORKDIR/output/"

echo "=== U-Boot build DONE ==="
echo "Test: qemu-system-arm -M vexpress-a9 -cpu cortex-a9 -m 512M -nographic -kernel $WORKDIR/output/u-boot"
