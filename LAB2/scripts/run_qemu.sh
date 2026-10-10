#!/bin/bash
# LAB-02 - Boot QEMU voi kernel 6.6.21 + rootfs da tich hop lab2_driver
# Thoat QEMU: Ctrl+A roi X
set -e

LAB1_WORKDIR="$HOME/embedded_lab1"
WORKDIR="$HOME/embedded_lab2"
KVER="6.6.21"

qemu-system-arm \
  -M vexpress-a9 \
  -kernel "$LAB1_WORKDIR/kernel/linux-$KVER/arch/arm/boot/zImage" \
  -dtb "$LAB1_WORKDIR/kernel/linux-$KVER/arch/arm/boot/dts/arm/vexpress-v2p-ca9.dtb" \
  -initrd "$WORKDIR/output/initramfs.cpio.gz" \
  -append "console=ttyAMA0 rdinit=/sbin/init" \
  -nographic \
  -m 512M
