# Embedded Linux Systems — Labs & Assignments

This repository contains practical work for the Embedded Linux Systems course at FPT University. It covers kernel configuration and booting, Linux device drivers, kernel debugging, performance analysis, and a multithreaded sensor-monitoring application.

## Development Environment

| Component | Configuration |
|---|---|
| Host OS | Ubuntu 26.04 (x86_64) |
| Target architecture | ARMv7-A |
| Target CPU | Cortex-A9 |
| QEMU machine | `vexpress-a9` |
| Linux kernel | 6.6.21 LTS |
| Cross compiler | `arm-linux-gnueabihf-gcc` |
| Root filesystem | BusyBox + initramfs |

## Repository Structure

```text
Embedded-linux-labs/
├── LAB1/                   # Kernel configuration, U-Boot, BusyBox and QEMU boot
├── LAB2/                   # Character driver, procfs/sysfs and MTD/JFFS2
├── LAB 3/                  # GDB debugging, memory analysis and performance testing
├── ASSIGNMENT_1/           # IOCTL character driver and misc counter driver
├── ASSIGNMENT_2/           # Sensor driver and multithreaded monitoring application
├── Docs/                   # Original lab and assignment specifications
└── README.md
```

## Labs and Assignments

### LAB 1 — Kernel Configuration and System Boot
**Directory:** [LAB1/](LAB1/)

- Configure and build Linux kernel 6.6.21 for ARMv7.
- Build U-Boot and BusyBox.
- Prepare a BusyBox root filesystem and initramfs.
- Boot the ARM system with QEMU vexpress-a9 and inspect it through the BusyBox shell.

See [LAB1/README.md](LAB1/README.md) for the boot sequence, build workflow, configuration, and troubleshooting.

### LAB 2 — Character Device Driver, procfs/sysfs and MTD/JFFS2
**Directory:** [LAB2/](LAB2/)

- Implement a character device with open, read, write, and release operations.
- Transfer data safely between kernel space and userspace.
- Expose driver information through procfs and sysfs.
- Practice MTD and JFFS2 concepts.

See [LAB2/README.md](LAB2/README.md) for driver lifecycle, interfaces, build commands, and testing.

### LAB 3 — Kernel Debugging and Performance Analysis
**Directory:** [LAB 3/](LAB%203/)

- Debug ARM applications with GDB.
- Investigate array bounds errors, use-after-free, and integer overflow.
- Detect memory leaks with Valgrind.
- Trace system calls with strace and profile applications with perf.
- Measure observed scheduling latency with cyclictest.

See [LAB 3/README.md](LAB%203/README.md).

### Assignment 1 — IOCTL and Misc Counter Drivers
**Directory:** [ASSIGNMENT_1/](ASSIGNMENT_1/)

- Extend a character driver with IOCTL commands for buffer reset, statistics, operating mode, and version retrieval.
- Implement a counter device using miscdevice and atomic_t.
- Publish counter statistics through /proc/counter_info.

See [ASSIGNMENT_1/README.md](ASSIGNMENT_1/README.md).

### Assignment 2 — Sensor Monitoring System
**Directory:** [ASSIGNMENT_2/](ASSIGNMENT_2/)

- Implement the sms_sensor character driver.
- Generate simulated temperature and humidity samples.
- Configure the minimum sampling interval through IOCTL.
- Expose driver statistics through procfs.
- Build a userspace application with sensor, processor, alert, and logger threads, queues, and a circular buffer.

See [ASSIGNMENT_2/README.md](ASSIGNMENT_2/README.md).

## Shared Build Platform

The common ARM target settings are:

- ARCH=arm
- CROSS_COMPILE=arm-linux-gnueabihf-
- Kernel source: ~/embedded_lab1/kernel/linux-6.6.21
- QEMU machine: vexpress-a9
- Device tree: vexpress-v2p-ca9.dtb

Typical external-module build:

```bash
make KDIR="$HOME/embedded_lab1/kernel/linux-6.6.21" \
     ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf-
```

Use the README inside each lab or assignment for its exact build and run procedure. A fresh clone does not necessarily contain generated kernel images, root filesystems, test binaries, or runtime logs.

## Documentation and Notes

Original specifications are stored in [Docs/](Docs/). The project READMEs describe implementation details and reproducible workflows.

- Generated build products may be kept locally and are not necessarily tracked.
- Run kernel modules only in the intended test environment (QEMU or compatible ARM hardware).
- The documented target kernel for this repository is Linux 6.6.21.
