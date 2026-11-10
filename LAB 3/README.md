# LAB-03 — Kernel Debugging and Performance Analysis

This directory contains the practical work for LAB-03 in the Embedded Linux Systems course at FPT University. It investigates application defects, memory-management errors, system-call behavior, application performance, and Linux scheduling latency. It builds on the ARM Linux environment prepared in LAB-01 and LAB-02.

## Objectives

- Debug ARM userspace programs with GDB and remote debugging.
- Identify array out-of-bounds access, use-after-free, and integer overflow.
- Detect and verify memory leaks with Valgrind.
- Trace system calls with strace.
- Collect application performance data with perf.
- Measure observed scheduling latency with cyclictest.
- Keep test output and analysis reproducible.

## Development Environment

| Component | Configuration |
|---|---|
| Host OS | Ubuntu 26.04 (x86_64) |
| Target architecture | ARMv7-A |
| Target CPU | Cortex-A9 |
| Emulator | `vexpress-a9` |
| Linux kernel | 6.6.21 |
| Cross compiler | `arm-linux-gnueabihf-gcc` |
| Debugger | `gdb-multiarch` |
| Memory analysis | Valgrind |
| System-call tracing | strace |
| Performance profiling | perf |
| Latency testing | cyclictest |

Some tests run natively on the host (for example, Valgrind and host profiling), while ARM-targeted applications can be executed in QEMU.

## Repository Contents

The following structure reflects the files currently tracked in this directory:

```text
LAB 3/
├── README.md
├── apps/
│   ├── buggy_app.c
│   ├── buggy_app
│   ├── leaky_app.c
│   ├── leaky_app_fixed.c
│   ├── leaky_hold.c
│   ├── leaky_hold_arm
│   ├── perf_target.c
│   ├── perf_target
│   └── perf_target_arm
└── logs/
    └── cpio_build.log
```

Generated root filesystems, kernel images, device-tree files, and additional runtime reports may be created locally. They are not shown above because they are not currently tracked in this directory.

## Part A — Remote Debugging with GDB

The debugging examples investigate three common C defects:

1. Array out-of-bounds: accessing an element outside the valid array range.
2. Use-after-free: accessing dynamically allocated memory after it has been released.
3. Integer overflow: arithmetic exceeding the representable range of an integer type.

Build the ARM example with debug symbols and no optimization:

```bash
cd "LAB 3/apps"

arm-linux-gnueabihf-gcc \
    -g -O0 -Wall \
    -march=armv7-a -mfpu=vfpv3 -mfloat-abi=hard \
    -o buggy_app buggy_app.c

file buggy_app
arm-linux-gnueabihf-readelf -S buggy_app | grep debug
```

Use GDB remote debugging to set breakpoints, step through the code, inspect variables, and record program state. For each defect, document the cause, source location, observed behavior, debugging steps, and correction.

## Part B — Memory Analysis with Valgrind

Build and analyze the original and corrected applications:

```bash
cd "LAB 3/apps"

gcc -g -O0 -Wall -o leaky_app leaky_app.c
valgrind --leak-check=full --show-leak-kinds=all \
         --track-origins=yes ./leaky_app

gcc -g -O0 -Wall -o leaky_app_fixed leaky_app_fixed.c
valgrind --leak-check=full --show-leak-kinds=all \
         --track-origins=yes ./leaky_app_fixed
```

Compare the actual reports, identify allocation sites, and explain the changes made to release memory. Report only leak counts and byte totals observed in the tool output.

## Part C — Performance Profiling

Build the performance target:

```bash
cd "LAB 3/apps"
gcc -g -O1 -Wall -o perf_target perf_target.c -lm
```

Trace system calls and collect performance data where supported:

```bash
strace -o strace_summary.txt ./perf_target
strace -c ./perf_target
perf stat ./perf_target
perf record -g -- ./perf_target
perf report
```

Discuss the system calls observed, workload characteristics, available performance counters, and limitations caused by permissions or kernel configuration.

## Part D — Real-Time Latency Analysis

Install the tools if necessary:

```bash
sudo apt update
sudo apt install rt-tests gnuplot
```

Example cyclictest run:

```bash
cyclictest \
    --mlockall \
    --threads=4 \
    --priority=80 \
    --interval=1000 \
    --distance=500 \
    --loops=10000 \
    --histogram=200 \
    --histfile=hist_standard.txt
```

Record minimum, average, and maximum latency, histogram distribution, and machine workload. These are measurements under specific conditions, not hard real-time guarantees.

**Measurement note:** cyclictest results collected on the host describe the host system. They must not be presented as measurements of physical ARM hardware or guaranteed QEMU guest latency.

## QEMU Environment

ARM applications can be tested with the LAB-01 kernel and a LAB-03 initramfs when those generated artifacts are available locally. Adjust the paths below to match the actual output files:

```bash
qemu-system-arm \
    -M vexpress-a9 -cpu cortex-a9 -smp 2 -m 512M \
    -kernel /path/to/zImage \
    -dtb /path/to/vexpress-v2p-ca9.dtb \
    -initrd /path/to/initramfs_lab3.cpio.gz \
    -append "console=ttyAMA0,115200 rdinit=/sbin/init" \
    -nographic
```

Inside the guest, inspect the system with uname -a, cat /proc/cmdline, mount, and ls /sys/class.

## Evidence and Logs

Store generated evidence in a logs directory. Useful records include:

| Evidence | Purpose |
|---|---|
| GDB session transcript | Breakpoints, variable inspection, and defect analysis |
| Initial Valgrind report | Memory errors or leaks before correction |
| Corrected Valgrind report | Results after memory-management fixes |
| strace output | System calls made by the application |
| perf stat/report output | Available counters and profiling samples |
| cyclictest output/histogram | Observed scheduling latency distribution |
| QEMU boot log or screenshot | ARM guest boot and runtime environment |

Only describe a result as completed after generating and reviewing its output.

## Lessons Learned

GDB helps inspect execution state; Valgrind identifies memory-management errors; strace reveals application/kernel interactions; perf provides profiling information when supported; cyclictest measures observed scheduling latency; and QEMU provides a repeatable ARM Linux environment for application testing.

## References

- [Linux Kernel Documentation](https://docs.kernel.org/)
- [QEMU Documentation](https://www.qemu.org/docs/master/)
- [GNU GDB Documentation](https://sourceware.org/gdb/documentation/)
- [Valgrind Documentation](https://valgrind.org/docs/)
- [strace](https://strace.io/)
- [Linux perf](https://perf.wiki.kernel.org/)
- [rt-tests](https://git.kernel.org/pub/scm/utils/rt-tests/rt-tests.git/)
