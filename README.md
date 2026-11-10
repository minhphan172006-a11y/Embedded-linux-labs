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

## Common Issues and Fixes (Ubuntu 26.04 / Modern Toolchains)

This section collects practical problems encountered while building and testing the labs and assignments on a modern Ubuntu host with an ARM cross-toolchain. Commands are examples; adjust paths to the actual workspace.

### 1. Host architecture and target architecture are different

**Symptom:** A module or executable is reported as x86-64 even though it is intended to run in the ARM QEMU guest, or QEMU reports an executable format error.

**Cause:** The host compiler (`gcc`) was used instead of the ARM cross-compiler.

**Fix:** Build kernel modules with the ARM kernel build settings and compile guest userspace programs with the ARM compiler:

```bash
export ARCH=arm
export CROSS_COMPILE=arm-linux-gnueabihf-
export KDIR="$HOME/embedded_lab1/kernel/linux-6.6.21"

make KDIR="$KDIR" ARCH="$ARCH" CROSS_COMPILE="$CROSS_COMPILE"
arm-linux-gnueabihf-gcc -Wall -Wextra -o app_arm app.c
file app_arm
```

Use the host `gcc` only for programs intended to run on Ubuntu itself. Check the output of `file` before copying a binary into the guest root filesystem.

### 2. Kernel module build fails because the kernel build directory is wrong

**Symptom:** `make` reports that there is no Makefile, no rule to build the target, or that the kernel build directory cannot be found.

**Cause:** The `KDIR` variable points to the wrong directory, or the command is being run outside the module directory.

**Fix:** Confirm that the kernel source/build tree contains its top-level Makefile, then build from the directory containing the module Makefile:

```bash
ls "$HOME/embedded_lab1/kernel/linux-6.6.21/Makefile"
cd /path/to/driver
make KDIR="$HOME/embedded_lab1/kernel/linux-6.6.21" \
     ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf-
```

If the kernel was configured in a separate output directory, pass the correct configured build directory as `KDIR`. Do not point `KDIR` at the project root or at a directory that contains no kernel Makefile.

### 3. Module architecture or kernel-version mismatch

**Symptom:** The module builds but `insmod` rejects it with an invalid module format error.

**Cause:** The module was built for a different architecture, kernel release, configuration, or kernel build tree than the kernel currently running in QEMU.

**Fix:** Compare the guest kernel release with the build tree and inspect the module:

```sh
# Inside the QEMU guest
uname -r
```

```bash
# On the Ubuntu host
file driver.ko
modinfo driver.ko
make -C "$HOME/embedded_lab1/kernel/linux-6.6.21" \
     M="$PWD" ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- clean
make -C "$HOME/embedded_lab1/kernel/linux-6.6.21" \
     M="$PWD" ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- modules
```

Use the same configured kernel tree and configuration that produced the kernel booted by QEMU. Check `dmesg` in the guest for the detailed rejection reason.

### 4. Undefined ARM helper symbol such as __aeabi_uldivmod

**Symptom:** Module linking reaches MODPOST but reports an unresolved symbol such as `__aeabi_uldivmod`.

**Cause:** The compiler may have emitted a runtime helper for 64-bit division that is not available to a kernel module in that build configuration. This can happen when kernel code performs ordinary division on a 64-bit value.

**Fix:** Prefer kernel arithmetic helpers such as `div_u64()` where appropriate, include the required kernel header, and avoid unnecessary 64-bit division in driver code:

```c
#include <linux/math64.h>

/* Example: divide a u64 value by a 32-bit divisor */
u64 quotient = div_u64(value, divisor);
```

Rebuild the module and inspect the complete MODPOST/linker output. Do not treat a previous build log containing this error as evidence that the module was successfully built.

### 5. Device node /dev/... does not appear after loading a driver

**Symptom:** `insmod` succeeds, but `/dev/<device>` is missing.

**Cause:** Registering a device number or a misc device does not guarantee that a device node has appeared in a minimal BusyBox root filesystem. The class/device registration, devtmpfs mount, and userspace device manager affect node creation.

**Fix:** Check the registration messages and device filesystem in the guest:

```sh
dmesg | tail -n 50
mount | grep devtmpfs
ls -l /dev
```

If devtmpfs is enabled in the kernel, mount it when it is not already mounted:

```sh
mount -t devtmpfs devtmpfs /dev
```

For a fixed-major character device in a controlled lab setup, a node can be created manually using the major and minor numbers actually registered by the driver:

```sh
mknod /dev/example c <major> <minor>
```

Do not guess the major/minor values. For a misc device, check the registered misc-device name and its dynamically assigned minor.

### 6. copy_to_user() or copy_from_user() causes read/write failures

**Symptom:** A driver returns `-EFAULT`, or data is missing or corrupted when an application reads from or writes to a device.

**Cause:** Kernel code is directly dereferencing a userspace pointer, passing an incorrect length, or not checking the number of bytes that could not be copied.

**Fix:** Use the kernel user-copy APIs and handle their return values. These functions return the number of bytes that were not copied (zero means success):

```c
if (copy_from_user(kernel_buffer, user_buffer, length))
    return -EFAULT;

if (copy_to_user(user_buffer, kernel_buffer, length))
    return -EFAULT;
```

Validate the requested length against the kernel buffer capacity before copying. Never directly dereference a pointer supplied by userspace.

### 7. cat /dev/... does not stop or repeatedly returns the same data

**Symptom:** Running `cat` on a device produces repeated output or appears to hang.

**Cause:** The read handler does not correctly use the file position (`*ppos`), or it returns data again after the current record has already been consumed.

**Fix:** For a simple one-record read, return EOF when the file position is non-zero and advance the position after copying data successfully:

```c
if (*ppos > 0)
    return 0;

/* Copy the record to userspace. */
if (copy_to_user(buf, data, data_len))
    return -EFAULT;

*ppos += data_len;
return data_len;
```

For a stream or buffered device, implement position and buffer handling according to that device's intended read semantics. Do not apply the one-record pattern blindly to a streaming interface.

### 8. Shared driver counters or buffers return inconsistent values

**Symptom:** Read/write statistics, buffer contents, or sensor state become inconsistent when multiple processes access the device.

**Cause:** Multiple execution contexts access shared state without suitable synchronization.

**Fix:** Use a mutex for data structures and operations that may sleep, and use atomic types for simple independent counters when appropriate:

```c
static DEFINE_MUTEX(device_mutex);
static atomic_t read_count = ATOMIC_INIT(0);
```

Protect related updates to a shared buffer and its length with the same mutex. Atomic counters do not automatically make a multi-variable operation atomic or protect a buffer.

### 9. QEMU boots but the kernel cannot find the root filesystem or console

**Symptom:** QEMU starts but the guest stops at a kernel panic, cannot mount the root filesystem, or shows no expected serial console.

**Cause:** The kernel, device tree, initramfs, boot arguments, or console device do not match the selected QEMU machine and root filesystem.

**Fix:** Verify that the image paths exist and use the matching ARM vexpress platform artifacts:

```bash
file /path/to/zImage
file /path/to/initramfs.cpio.gz
ls -l /path/to/vexpress-v2p-ca9.dtb
```

For this project, the usual machine and console settings are `-M vexpress-a9` and `console=ttyAMA0,115200`. Check that the initramfs contains a valid init program and that the kernel command line points to it (for example, `rdinit=/sbin/init`). Use the actual paths generated by the local LAB-01 build.

### 10. Permission denied while preparing or modifying the root filesystem

**Symptom:** Copying device nodes, changing ownership, or modifying special files inside a root filesystem fails with `Operation not permitted`.

**Cause:** The operation may require root privileges, or the target is a special file, symlink, mounted filesystem, or archive entry that cannot be modified like a regular file.

**Fix:** Inspect the file type first:

```bash
ls -l /path/to/rootfs/dev
file /path/to/rootfs/dev/*
```

Create device nodes and set ownership while preparing the root filesystem with appropriate privileges, or create them at guest boot using the target's init script. Do not use ordinary `cp` commands to replace special device nodes. For an initramfs archive, verify the archive contents and device-node metadata rather than assuming a failed copy succeeded.

### 11. Application does not compile or link with pthreads

**Symptom:** The application reports undefined references to pthread functions, or thread-related code does not compile as expected.

**Cause:** The userspace build command omits the pthread compiler/linker option, or uses a host compiler for a program intended for the ARM guest.

**Fix:** Include `-pthread` in both compilation and linking, and select the compiler for the intended runtime:

```bash
# Native Ubuntu application
gcc -Wall -Wextra -pthread -o sms_app *.c -lm

# ARM guest application
arm-linux-gnueabihf-gcc -Wall -Wextra -pthread \
    -o sms_app_arm *.c -lm
```

Use the project's Makefile when it already defines the complete source list and compiler flags. Do not use a wildcard command if it would compile multiple files containing duplicate definitions or test programs with separate `main()` functions.

### 12. perf, Valgrind, or cyclictest gives incomplete results

**Symptom:** A tool reports permission errors, unsupported events, missing debug information, or results that differ substantially between runs.

**Cause:** These tools depend on host kernel configuration, permissions, debug symbols, CPU support, and system load. A measurement on Ubuntu host is not automatically a measurement of the ARM QEMU guest.

**Fix:**

- Build debug targets with `-g` when source-level analysis is required.
- Run Valgrind on a supported host executable and inspect the complete report.
- Check host perf permissions and available events before interpreting `perf` output.
- Record system load, CPU configuration, test duration, and tool options for cyclictest.
- Clearly label whether a measurement was taken on the Ubuntu host or inside QEMU.

Do not claim hard real-time guarantees from cyclictest results, and do not compare measurements from different environments as if they were collected on identical hardware.

### 13. Useful diagnostic commands

Run these commands in the environment indicated by the comments:

```bash
# Ubuntu host: toolchain and QEMU versions
uname -m
arm-linux-gnueabihf-gcc --version
qemu-system-arm --version

# Ubuntu host: verify kernel build tree and generated images
ls -l "$HOME/embedded_lab1/kernel/linux-6.6.21/Makefile"
ls -lh "$HOME/embedded_lab1/kernel/linux-6.6.21/arch/arm/boot/zImage"

# Ubuntu host: inspect module architecture and metadata
file path/to/driver.ko
modinfo path/to/driver.ko
```

```sh
# Inside QEMU guest: kernel and module diagnostics
uname -a
cat /proc/cmdline
cat /proc/modules
dmesg | tail -n 80
ls -l /dev
```

**Troubleshooting rule:** identify whether a command is running on the Ubuntu host or inside the QEMU guest before applying a fix. Host paths, host binaries, guest device nodes, and guest kernel modules are not interchangeable.

## Documentation and Notes

Original specifications are stored in [Docs/](Docs/). The project READMEs describe implementation details and reproducible workflows.

- Generated build products may be kept locally and are not necessarily tracked.
- Run kernel modules only in the intended test environment (QEMU or compatible ARM hardware).
- The documented target kernel for this repository is Linux 6.6.21.
