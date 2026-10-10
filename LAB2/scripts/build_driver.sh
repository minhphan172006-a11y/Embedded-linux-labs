#!/bin/bash
# LAB-02 - Build lab2_driver.ko cho kernel 6.6.21 (build san o LAB-01)
set -e

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
KDIR="$HOME/embedded_lab1/kernel/linux-6.6.21"

if [ ! -f "$KDIR/Module.symvers" ]; then
    echo "ERROR: Chua thay kernel build tai $KDIR"
    echo "Chay LAB01-kernel-boot/scripts/build_kernel.sh truoc."
    exit 1
fi

cd "$REPO_DIR/driver"
make clean 2>/dev/null || true
make

file lab2_driver.ko
modinfo lab2_driver.ko | grep vermagic

echo "=== Driver build DONE: $REPO_DIR/driver/lab2_driver.ko ==="
