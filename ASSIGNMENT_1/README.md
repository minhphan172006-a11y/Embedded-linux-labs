# Assignment 1 — IOCTL Character Driver and Misc Counter Driver

This assignment contains two Linux kernel-driver exercises for the Embedded Linux Systems course. The target is ARMv7-A on QEMU vexpress-a9, using Linux 6.6.21 from the LAB-01 environment.

## Objectives

- Extend a character device with a userspace IOCTL interface.
- Transfer structured statistics between kernel space and userspace.
- Protect shared driver state with a mutex.
- Implement a counter device using the misc-device framework.
- Use atomic operations and expose statistics through procfs.

## Repository Structure

```text
ASSIGNMENT_1/
├── README.md
├── task1_ioctl/
│   ├── Makefile
│   ├── asgn1_driver.c
│   ├── test_asgn1.c
│   └── test_asgn1
└── task2_misc/
    ├── Makefile
    └── counter_driver.c
```

## Development Environment

| Component | Configuration |
|---|---|
| Host | Ubuntu 26.04, x86_64 |
| Target | ARMv7-A / Cortex-A9 |
| Kernel | Linux 6.6.21 |
| Cross compiler | `arm-linux-gnueabihf-gcc` |
| Emulator | QEMU `vexpress-a9` |
| Kernel build directory | `~/embedded_lab1/kernel/linux-6.6.21` |

Set the target build variables:

```bash
export ARCH=arm
export CROSS_COMPILE=arm-linux-gnueabihf-
export KDIR="$HOME/embedded_lab1/kernel/linux-6.6.21"
```

## Task 1 — Character Device Driver with IOCTL

### Device behavior

The driver registers a character device named `asgn1` with major number **241** and creates `/dev/asgn1`. It maintains a 1024-byte buffer and statistics for open, read, and write operations, current buffer length, and the most recent write size. Shared state is protected by a mutex.

### IOCTL commands

| Command | Purpose |
|---|---|
| `ASGN1_RESET_BUFFER` | Clear the buffer and reset read/write statistics |
| `ASGN1_GET_STATS` | Copy the statistics structure to userspace |
| `ASGN1_SET_MODE` | Select Kernel mode or Echo mode |
| `ASGN1_GET_VERSION` | Return the driver version string in a 64-byte buffer |

Write modes:

- **Echo mode (1):** store the input data as received.
- **Kernel mode (0):** prepend `[KERNEL]` to the input before storing it.

The driver uses copy_from_user() and copy_to_user() for transfers and validates mode values and userspace pointers.

### Build

```bash
cd ASSIGNMENT_1/task1_ioctl
make KDIR="$HOME/embedded_lab1/kernel/linux-6.6.21" \
     ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf-
```

The userspace test program must be compiled for ARM to run inside the guest:

```bash
arm-linux-gnueabihf-gcc -Wall -Wextra \
    -o test_asgn1 test_asgn1.c
```

### Test workflow

After booting the target root filesystem and loading the module:

```sh
insmod asgn1_driver.ko
ls -l /dev/asgn1
./test_asgn1
dmesg | tail -n 40
```

The test program covers buffer reset, Echo mode, Kernel mode, statistics, version retrieval, invalid mode rejection, and empty-buffer verification after reset.

Unload the module after testing:

```sh
rmmod asgn1_driver
```

### Fresh-clone prerequisite

The tracked driver and test source both include `asgn1_ioctl.h`, but this header is not currently present in the tracked `task1_ioctl/` directory. Add and commit the matching shared header (IOCTL definitions and the `asgn1_stats` structure) before attempting a clean build from a fresh clone. The current directory is therefore not yet self-contained for Task 1.

## Task 2 — Misc Counter Driver

### Device and procfs interfaces

The module registers a misc device named `counter` with a dynamically allocated minor number, providing `/dev/counter`.

| Interface | Behavior |
|---|---|
| Read `/dev/counter` | Return the current integer and then increment it |
| Write `/dev/counter` | Parse a non-negative integer and set the counter; writing 0 resets it |
| `/proc/counter_info` | Show current counter, total reads, and total writes/resets |

The implementation uses atomic_t for the counter and operation statistics. The read handler checks the file position so a normal read utility reaches EOF instead of reading the same value indefinitely.

### Build

```bash
cd ASSIGNMENT_1/task2_misc
make KDIR="$HOME/embedded_lab1/kernel/linux-6.6.21" \
     ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf-
```

### Test workflow

```sh
insmod counter_driver.ko
cat /dev/counter
cat /dev/counter
echo 10 > /dev/counter
cat /dev/counter
echo 0 > /dev/counter
cat /proc/counter_info
dmesg | tail -n 40
rmmod counter_driver
```

Verify the actual output in QEMU: reads return the current value and advance it; writes set the value; procfs reports the current value and operation totals.

## Validation Notes

- Build modules against the same kernel build tree used by the QEMU target.
- Ensure module and userspace test binaries are ARM-compatible before copying them into the guest root filesystem.
- Inspect dmesg after module load, IOCTL calls, and unload.
- No dedicated Task 2 test script is currently tracked; the commands above are manual smoke tests.
- Keep LAB-01, LAB-02, and LAB-03 source directories unchanged. Assignment 1 is maintained separately.

## References

- [Linux driver API](https://docs.kernel.org/driver-api/)
- [Linux ioctl documentation](https://docs.kernel.org/userspace-api/ioctl/ioctl-number.html)
- [Linux misc device API](https://docs.kernel.org/driver-api/misc_devices.html)
- [Linux procfs documentation](https://docs.kernel.org/filesystems/proc.html)
