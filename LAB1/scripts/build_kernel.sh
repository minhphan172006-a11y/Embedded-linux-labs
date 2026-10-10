#!/bin/bash
# LAB-01 - Build kernel 6.6.21 cho QEMU vexpress-a9
set -e

WORKDIR="$HOME/embedded_lab1"
KVER="6.6.21"
REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

mkdir -p "$WORKDIR"/{kernel,output,configs}
cd "$WORKDIR/kernel"

if [ ! -d "linux-$KVER" ]; then
    wget -c "https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-$KVER.tar.xz"
    tar -xf "linux-$KVER.tar.xz"
fi
cd "linux-$KVER"

export ARCH=arm
export CROSS_COMPILE=arm-linux-gnueabihf-
# KHONG export KCFLAGS (gay loi "D register out of range" o vfpmodule.o)

make vexpress_defconfig
./scripts/config --enable CONFIG_BLK_DEV_INITRD
./scripts/config --enable CONFIG_SMP
yes "" | make oldconfig > /dev/null

make -j"$(nproc)" zImage dtbs modules
make modules_prepare

cp arch/arm/boot/zImage "$WORKDIR/output/"
cp arch/arm/boot/dts/arm/vexpress-v2p-ca9.dtb "$WORKDIR/output/"
cp .config "$WORKDIR/configs/kernel.config"
cp .config "$REPO_DIR/configs/kernel.config" 2>/dev/null || true

echo "=== Kernel build DONE ==="
