# Embedded Linux Systems — LAB-01 & LAB-02

Hai bài lab môn Embedded Linux Systems: cấu hình/build kernel + boot hệ thống nhúng (LAB-01),
và viết character device driver + MTD/JFFS2 (LAB-02), chạy trên QEMU `vexpress-a9` (ARMv7
Cortex-A9).

## Môi trường build (host)

| Thành phần | Phiên bản dùng thực tế |
|---|---|
| Host OS | Ubuntu 26.04 LTS (64-bit) |
| Linux kernel (target) | **6.6.21 LTS** (thay cho 5.15 LTS trong đề bài gốc — xem lý do bên dưới) |
| U-Boot | 2022.04 |
| BusyBox | 1.35.0 |
| ARM toolchain | gcc-arm-linux-gnueabihf (Ubuntu 15.2.0) |
| QEMU | qemu-system-arm, machine `vexpress-a9` |

### Vì sao đổi kernel 5.15 → 6.6.21?

Đề bài gốc LAB-01 chỉ định kernel 5.15 LTS cho Ubuntu 20.04/22.04. Trên **Ubuntu 26.04**
(toolchain/glibc/binutils mới hơn nhiều), 6.6.21 — cũng là một bản LTS — tương thích tốt hơn
với host hiện đại và được dùng xuyên suốt cho cả hai lab để nhất quán.

## Cấu trúc repo

```
.
├── LAB01-kernel-boot/     # Kernel config, U-Boot, BusyBox, rootfs overlay, script build
├── LAB02-driver-mtd/      # Character device driver, procfs/sysfs, test scripts, rootfs overlay
└── docs/                  # Đề bài gốc (docx)
```

Mã nguồn kernel/U-Boot/BusyBox **không** nằm trong repo (quá lớn, đã có sẵn công khai trên
kernel.org / denx.de / busybox.net) — dùng script trong `scripts/` để tải và build lại.

## Build nhanh

```bash
cd LAB01-kernel-boot/scripts
./build_kernel.sh && ./build_uboot.sh && ./build_busybox.sh
./pack_rootfs.sh
./run_qemu.sh

cd ../../LAB02-driver-mtd/scripts
./build_driver.sh
./pack_rootfs.sh        # dùng rootfs đã build ở LAB-01 làm nền
./run_qemu.sh
```

Chi tiết từng bước, giải thích lý thuyết, và toàn bộ lỗi đã gặp + cách sửa: xem README trong
từng thư mục lab.

## Tổng hợp lỗi đã gặp và cách sửa (áp dụng cho Ubuntu 26.04 / toolchain mới)

| # | Lỗi | Nguyên nhân | Cách sửa |
|---|---|---|---|
| 1 | `error: implicit declaration of function 'class_create'` khi build driver | API `class_create()` đổi chữ ký từ kernel **6.4+** (bớt tham số `THIS_MODULE`) | Dùng `#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,4,0)` để chọn đúng chữ ký — xem `LAB02-driver-mtd/driver/lab2_driver.c` |
| 2 | `.dtb` không thấy ở `arch/arm/boot/dts/` | Kernel 6.6 đổi cấu trúc thư mục, thêm `arm/` con | Dùng đúng path `arch/arm/boot/dts/arm/vexpress-v2p-ca9.dtb` |
| 3 | `Error: D register out of range for selected VFP version` khi build `vfpmodule.o` | Tự `export KCFLAGS="-mfpu=vfpv3 ..."` đè lên flags riêng mà kernel's `arch/arm/vfp/Makefile` đã tự set theo `.config` | **Không** export `KCFLAGS` thủ công — để `.config` (từ `vexpress_defconfig`) tự quyết định |
| 4 | `make menuconfig` lỗi `dochecklxdialog` khi build BusyBox | Thiếu header `ncurses` cho giao diện dialog | Cài `libncurses-dev libncursesw5-dev`, hoặc sửa thẳng `.config` bằng `sed` (không qua menuconfig) |
| 5 | `networking/tc.o` lỗi `TCA_CBQ_* undeclared` khi build BusyBox | Applet `tc` (traffic control) tham chiếu macro `CBQ` đã bị kernel header hiện đại loại bỏ | Tắt `CONFIG_TC` trong `.config` BusyBox (`sed -i 's/^CONFIG_TC=y/# CONFIG_TC is not set/' .config`) |
| 6 | `insmod: File exists` khi chạy `test_driver.sh` | Module đã được `rcS` tự `insmod` lúc boot, script test cố nạp lại | Thêm kiểm tra `lsmod | grep -q lab2_driver` trước khi `insmod` trong script |
| 7 | `mkdir: Permission denied` khi tạo thư mục trong rootfs copy từ lab1 | `sudo cp -r` giữ nguyên owner gốc (`root`) cho một số thư mục con | `sudo chown -R $(whoami):$(whoami)` toàn bộ `initramfs/` sau khi copy |
| 8 | `qemu-system-arm: could not load kernel 'zImage'` | File chưa được build xong hoặc chưa copy ra `output/` | Build lại, kiểm tra bằng `ls -lh arch/arm/boot/zImage` trước khi copy |
| 9 | Gõ `bootm`/`bootz` trong U-Boot shell báo `can't get kernel image!` | QEMU (không gắn disk/TFTP ảo) không có cách nạp `zImage` thật vào RAM qua đường U-Boot shell | Thoát U-Boot, chạy QEMU lần mới với `-kernel`/`-dtb`/`-initrd` để QEMU tự nạp trực tiếp (demo B.4 cố ý thiếu `-initrd` để thấy kernel panic) |

## License / Môn học

Embedded Linux Systems — bài tập thực hành cá nhân/nhóm. Mã nguồn `lab2_driver.c` dùng
license GPL (bắt buộc với kernel module).
