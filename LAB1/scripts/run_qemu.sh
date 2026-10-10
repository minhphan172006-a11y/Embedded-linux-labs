#!/bin/bash
# Thoat QEMU: Ctrl+A roi X
WORKDIR="$HOME/embedded_lab1"

qemu-system-arm \
  -M vexpress-a9 \
  -cpu cortex-a9 \
  -m 512M \
  -smp 2 \
  -nographic \
  -kernel "$WORKDIR/output/zImage" \
  -dtb "$WORKDIR/output/vexpress-v2p-ca9.dtb" \
  -initrd "$WORKDIR/output/initramfs.cpio.gz" \
  -append 'console=ttyAMA0,115200 rdinit=/sbin/init mem=512M'
