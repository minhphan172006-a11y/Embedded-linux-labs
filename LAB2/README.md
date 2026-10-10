# LAB-02 — Character Device Driver, procfs/sysfs & MTD/JFFS2

Viết kernel module character device (`/dev/lab2`) với giao tiếp qua `/proc/lab2_info` và
`/sys/class/lab2_class/lab2/*`, auto-load khi boot qua `rcS`; đồng thời thực hành MTD
subsystem (`nandsim`) và JFFS2 filesystem trên host.

Build trên nền kernel 6.6.21 đã build sẵn ở [`../LAB01-kernel-boot`](../LAB01-kernel-boot).

## Phần A+B — Character device + procfs + sysfs (`driver/lab2_driver.c`)

Vòng đời module:
```
insmod → lab2_init(): register_chrdev → class_create → device_create
                      → sysfs_create_group → proc_create("lab2_info")
[hoạt động: open/read/write/close qua /dev/lab2]
rmmod  → lab2_exit(): proc_remove → sysfs_remove_group → device_destroy
                      → class_destroy → unregister_chrdev
```

- **`copy_to_user()`/`copy_from_user()`** thay vì `memcpy()`: `device_buffer` nằm trong
  không gian địa chỉ kernel, con trỏ `buf` trong `read()`/`write()` thuộc không gian địa chỉ
  tiến trình user — hai vùng khác nhau hoàn toàn. `copy_*_user()` kiểm tra địa chỉ user hợp
  lệ trước khi sao chép; `memcpy()` không kiểm tra, có thể crash hoặc gây lỗ hổng bảo mật.
- **`mutex`**: `device_buffer`/`buffer_len` là tài nguyên dùng chung — không khoá sẽ gây
  race condition khi nhiều process `write()` đồng thời.
- **`class_create()`**: chữ ký đổi từ kernel **6.4+** (bớt `THIS_MODULE`) — code dùng
  `#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,4,0)` để tương thích cả 2 phiên bản.

## Build & deploy

```bash
cd scripts
./build_driver.sh     # build lab2_driver.ko cho kernel 6.6.21
./pack_rootfs.sh       # copy rootfs từ LAB-01, thêm .ko + script test + rcS mới, đóng gói
./run_qemu.sh          # boot QEMU, module tự load qua rcS
```

Trong QEMU shell:
```sh
lsmod                           # xác nhận lab2_driver đã load
cat /proc/lab2_info
test_driver.sh                  # kiểm tra char device read/write
test_procfs.sh                  # kiểm tra procfs + sysfs
cat /sys/class/lab2_class/lab2/last_data
dmesg | grep lab2
```

## Phần C — MTD subsystem & JFFS2 (chạy trên HOST, không liên quan target kernel)

```bash
sudo modprobe nandsim first_id_byte=0x20 second_id_byte=0x35
sudo modprobe mtdblock
sudo modprobe jffs2
cat /proc/mtd
sudo mtdinfo /dev/mtd0

sudo flash_erase /dev/mtd0 0 0
sudo mkfs.jffs2 --root=<thư mục dữ liệu mẫu> --eraseblock=0x4000 \
    --output=/tmp/rootfs.jffs2 --pad --no-cleanmarkers
sudo nandwrite -p /dev/mtd0 /tmp/rootfs.jffs2

sudo mount -t jffs2 /dev/mtdblock0 /mnt/jffs2
# ghi thêm dữ liệu, umount, mount lại -> xác nhận persistence
```

Nếu Ubuntu không có sẵn `nandsim`:
```bash
sudo apt install -y linux-modules-extra-$(uname -r)
```

`nanddump` trên `mtd-utils` bản mới của Ubuntu 26.04 yêu cầu `-f <file>` thay vì truyền
đường dẫn output ở cuối câu lệnh như tài liệu cũ:
```bash
sudo nanddump -o -l 512 -f /tmp/dump.bin /dev/mtd0
```

## Lỗi đã gặp và cách sửa

| Lỗi | Nguyên nhân | Cách sửa |
|---|---|---|
| `error: implicit declaration of function 'class_create'` | Chữ ký API đổi từ kernel 6.4+ | `#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,4,0)` (đã có sẵn trong `lab2_driver.c`) |
| `insmod: can't insert ...: File exists` khi chạy `test_driver.sh` thủ công | Module đã được `rcS` tự nạp sẵn lúc boot | Script kiểm tra `lsmod | grep -q lab2_driver` trước khi `insmod` (đã sửa trong `test_driver.sh`) |
| `mkdir: Permission denied` khi tạo `lib/modules/` trong rootfs | `sudo cp -r` giữ nguyên owner `root` từ rootfs LAB-01 | `sudo chown -R $(whoami):$(whoami)` sau khi copy |
| `nanddump` báo usage/help thay vì dump | Cú pháp cũ (`nanddump ... MTD-dev output-file`) không còn đúng với `mtd-utils` bản mới | Dùng `-f <file>` |
| Dump MTD ra toàn `0xff` sau khi reload `nandsim` | `nandsim` mô phỏng RAM-based, dữ liệu mất khi `rmmod`/`modprobe` lại | Dump ngay sau `nandwrite`, trước khi `rmmod`, nếu cần thấy magic bytes JFFS2 `85 19` |

## Cấu trúc thư mục

```
LAB02-driver-mtd/
├── driver/
│   ├── lab2_driver.c
│   └── Makefile
├── rootfs-overlay/
│   ├── etc/init.d/rcS          # auto-load module khi boot
│   └── usr/bin/
│       ├── test_driver.sh
│       └── test_procfs.sh
└── scripts/
    ├── build_driver.sh
    ├── pack_rootfs.sh
    └── run_qemu.sh
```
