# LAB-01 — Cấu hình Kernel & Khởi động Hệ thống Linux Nhúng (ARMv7)

Build Linux kernel 6.6.21 LTS, U-Boot 2022.04 và BusyBox 1.35.0 cho QEMU `vexpress-a9`
(Cortex-A9), đóng gói thành initramfs, boot tới BusyBox shell.

## Chuỗi khởi động (boot sequence)

```
Power ON → ROM bootloader (QEMU firmware ảo)
         → U-Boot (tuỳ chọn, demo riêng ở B.2–B.4)
         → zImage (kernel) + Device Tree Blob (.dtb)
         → start_kernel() → mount initramfs (cpio archive trong RAM)
         → exec /sbin/init (BusyBox) → /etc/inittab → /etc/init.d/rcS
         → BusyBox shell (/ #)
```

Trong lab này, QEMU đóng vai trò nạp trực tiếp `zImage`/`.dtb`/`initrd` qua tham số
command-line (`-kernel -dtb -initrd`) để boot nhanh gọn — U-Boot chỉ được demo riêng ở
Phần B (printenv/setenv/bootargs) để hiểu vai trò bootloader, không dùng để boot chuỗi
chính thức trong C.5.

## Build từ đầu

```bash
cd scripts
./build_kernel.sh      # tải + build kernel 6.6.21, output: ../output/zImage, *.dtb
./build_uboot.sh        # tải + build U-Boot 2022.04, output: ../output/u-boot
./build_busybox.sh      # tải + build BusyBox static, output: rootfs/busybox-1.35.0/_install
./pack_rootfs.sh        # tạo rootfs/initramfs/ từ overlay + busybox, đóng gói .cpio.gz
./run_qemu.sh           # boot QEMU hoàn chỉnh
```

Mỗi script tự tạo thư mục làm việc tại `~/embedded_lab1/` (ngoài repo — tránh commit file
build nặng).

## Các tham số kernel quan trọng đã bật trong `menuconfig`

| Option | Vị trí menu | Vì sao cần |
|---|---|---|
| `CONFIG_BLK_DEV_INITRD` | `General setup → Initial RAM filesystem and RAM disk support` | **Bắt buộc** — thiếu sẽ không mount được rootfs dạng cpio, kernel panic |
| `CONFIG_SERIAL_AMBA_PL011` | `Device Drivers → Character devices → Serial drivers → ARM AMBA PL011` | Driver UART ảo của vexpress-a9, không có sẽ không thấy gì trên console |
| `CONFIG_SMP` (tuỳ chọn) | `Kernel Features → Symmetric Multi-Processing support` | Tận dụng `-smp 2` khi chạy QEMU |

File `.config` đã dùng: [`configs/kernel.config`](configs/kernel.config),
[`configs/busybox.config`](configs/busybox.config).

## Lỗi đã gặp khi build trên Ubuntu 26.04 (toolchain mới) và cách sửa

### 1. `Error: D register out of range for selected VFP version` (build `vfpmodule.o`)

**Nguyên nhân:** tự `export KCFLAGS="-march=armv7-a -mfpu=vfpv3 -mfloat-abi=hard"` đè lên
flags riêng mà `arch/arm/vfp/Makefile` của kernel đã tự thiết lập theo `.config` → gây xung
đột giữa mã do GCC sinh ra (dùng thanh ghi `d16`, cần VFPv3 đủ 32 thanh ghi D) và flags
assembler nhận được (chỉ hỗ trợ `d0-d15`).

**Fix:** **không** export `KCFLAGS` thủ công. Chỉ cần `ARCH` và `CROSS_COMPILE`, để
`vexpress_defconfig`/`.config` tự quyết định toàn bộ flags kiến trúc.

### 2. `make menuconfig` lỗi `dochecklxdialog` khi build BusyBox

**Nguyên nhân:** thiếu header `ncurses` đầy đủ cho giao diện dialog.

**Fix:**
```bash
sudo apt install -y libncurses-dev libncursesw5-dev pkg-config
```
Hoặc tránh hẳn `menuconfig`, sửa thẳng `.config` bằng `sed` (cách `build_busybox.sh` trong
repo này dùng):
```bash
sed -i 's/^# CONFIG_STATIC is not set/CONFIG_STATIC=y/' .config
```

### 3. `networking/tc.o` lỗi `TCA_CBQ_MAX' undeclared` khi build BusyBox

**Nguyên nhân:** applet `tc` (traffic control) của BusyBox 1.35.0 tham chiếu các macro
`TCA_CBQ_*` thuộc queuing discipline CBQ, đã bị **loại bỏ khỏi kernel header hiện đại**
(Ubuntu 24.04+) từ lâu.

**Fix:** tắt applet `tc` trong `.config` trước khi build:
```bash
sed -i 's/^CONFIG_TC=y/# CONFIG_TC is not set/' .config
```

### 4. `qemu-system-arm: could not load kernel 'zImage'`

**Nguyên nhân:** build kernel dở dang/lỗi (thường do lỗi #1 ở trên làm `make` dừng trước
khi sinh `zImage`), hoặc quên copy file ra `output/`.

**Fix:** build lại sau khi sửa lỗi #1, kiểm tra `ls -lh arch/arm/boot/zImage` trước khi copy.

### 5. Gõ `bootm`/`bootz` trong U-Boot shell báo `ERROR: can't get kernel image!`

**Nguyên nhân:** QEMU (không gắn ổ đĩa ảo/TFTP) không có cách để U-Boot "thấy" file
`zImage` trên host nạp vào đúng địa chỉ RAM — lệnh `bootcmd` set trong B.3 chỉ mang tính
minh hoạ, không chạy thật được trong môi trường QEMU đơn giản này.

**Đây là hành vi mong đợi, không phải lỗi cần sửa** — Phần B chỉ demo U-Boot shell/biến môi
trường; việc boot kernel thật nằm ở C.5, nơi QEMU tự nạp `-kernel`/`-dtb`/`-initrd` trực
tiếp, bỏ qua U-Boot.

### 6. `mkdir: Permission denied` / file thuộc `root` trong rootfs

**Nguyên nhân:** `sudo mknod` khi tạo device node trong C.3 khiến các file đó thuộc `root`.

**Fix:**
```bash
sudo chown -R $(whoami):$(whoami) ~/embedded_lab1/rootfs/initramfs
```

## Kết quả mong đợi khi boot (C.5)

```
Booting Linux on physical CPU 0x0
...
Boot completed! Welcome to Embedded Linux Lab 1
/ #
```

Kiểm tra: `uname -a`, `cat /proc/cpuinfo`, `cat /proc/meminfo | head -5`, `mount`, `ps`.
