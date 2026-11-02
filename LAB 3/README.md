# LAB-03 — Embedded Linux Systems
## Kernel Debugging and Performance Analysis

This repository contains my implementation of **LAB-03: Debugging Kernel & Performance Analysis** for the Embedded Linux Systems course at FPT University.

The lab focuses on debugging ARM applications, detecting memory errors, analyzing program performance, and measuring Linux scheduling latency.

The project builds upon the Linux kernel and BusyBox initramfs environments prepared in LAB-01 and LAB-02.

---

## Table of Contents

- [Objectives](#objectives)
- [Development Environment](#development-environment)
- [Repository Structure](#repository-structure)
- [Part A — Remote Debugging with GDB](#part-a--remote-debugging-with-gdb)
- [Part B — Memory Analysis with Valgrind](#part-b--memory-analysis-with-valgrind)
- [Part C — Performance Profiling](#part-c--performance-profiling)
- [Part D — Real-Time Latency Analysis](#part-d--real-time-latency-analysis)
- [Build and Run](#build-and-run)
- [Results and Logs](#results-and-logs)
- [Lessons Learned](#lessons-learned)
- [References](#references)

---

## Objectives

The main objectives of this lab are:

- Debug ARM applications using GDB and remote debugging techniques.
- Identify common programming errors, including array out-of-bounds access, use-after-free, and integer overflow.
- Detect memory leaks using Valgrind.
- Analyze application behavior using `strace` and `perf`.
- Compare application behavior on the host system and the ARM virtual platform.
- Measure Linux scheduling latency using `cyclictest`.
- Organize source code, test results, and technical documentation in a reproducible project structure.

---

## Development Environment

| Component | Configuration |
|---|---|
| Host operating system | Ubuntu 26.04 |
| Host architecture | x86_64 |
| Target architecture | ARMv7-A |
| Target CPU | Cortex-A9 |
| Emulator | QEMU `vexpress-a9` |
| Linux kernel | 6.6.21 |
| Cross compiler | `arm-linux-gnueabihf-gcc` |
| Root filesystem | BusyBox + initramfs |
| Debugger | `gdb-multiarch` |
| Memory analysis | Valgrind |
| System call tracing | `strace` |
| Performance analysis | `perf` |
| Real-time latency testing | `cyclictest` |

Some tools run directly on the x86_64 host, while ARM applications run inside the QEMU virtual machine.

---

## Repository Structure

```text
embedded_lab3/
├── apps/
│   ├── buggy_app.c
│   ├── buggy_app
│   ├── leaky_app.c
│   ├── leaky_app_fixed.c
│   ├── perf_target.c
│   └── ...
│
├── rootfs/
│   └── initramfs/
│       ├── bin/
│       ├── dev/
│       ├── etc/
│       ├── lib/
│       ├── proc/
│       ├── sys/
│       └── ...
│
├── output/
│   ├── zImage
│   ├── vexpress-v2p-ca9.dtb
│   └── initramfs_lab3.cpio.gz
│
├── logs/
│   ├── cpio_build.log
│   ├── gdb_session.txt
│   ├── valgrind_report.txt
│   ├── valgrind_fixed.txt
│   ├── strace_summary.txt
│   ├── perf_report.txt
│   ├── perf_stat.txt
│   ├── hist_standard.txt
│   └── ...
│
└── README.md
```

**Note:** The tree above describes the intended project organization. Keep only files that actually exist in the repository, and update the structure as the lab progresses.

---

## Part A — Remote Debugging with GDB

### Overview

This part investigates three common programming errors in a C application compiled for ARM:

1. **Array out-of-bounds:** accessing an array element outside its valid index range.
2. **Use-after-free:** accessing memory after it has been released with `free()`.
3. **Integer overflow:** performing arithmetic that exceeds the representable range of a signed integer.

The application is compiled with debugging symbols to support source-level inspection.

### Compilation

```bash
cd ~/embedded_lab3/apps

arm-linux-gnueabihf-gcc \
    -g -O0 \
    -march=armv7-a \
    -mfpu=vfpv3 \
    -mfloat-abi=hard \
    -Wall \
    -o buggy_app buggy_app.c
```

Verify the executable:

```bash
file buggy_app
```

Check the debugging information:

```bash
arm-linux-gnueabihf-readelf -S buggy_app | grep debug
```

### Debugging Workflow

The debugging workflow includes:

- Building the ARM executable.
- Making the executable available inside the LAB-03 root filesystem.
- Starting the application under a remote debugging session.
- Connecting from the host using `gdb-multiarch`.
- Setting breakpoints and examining variables.
- Inspecting the program state when each bug is encountered.

Record the commands, debugger output, and observations in the `logs/` directory.

### Analysis

For each bug, document:

- The source of the problem.
- The relevant source-code line.
- The observed behavior.
- The debugging steps used to identify the issue.
- A proposed correction and the result after testing.

---

## Part B — Memory Analysis with Valgrind

### Overview

This part demonstrates how memory leaks can occur in C applications.

The example program contains dynamically allocated structures and buffers. The analysis focuses on memory that is allocated but not released correctly.

### Build the Application

```bash
cd ~/embedded_lab3/apps

gcc -g -O0 -Wall \
    -o leaky_app leaky_app.c
```

### Run Valgrind

```bash
valgrind \
    --leak-check=full \
    --show-leak-kinds=all \
    --track-origins=yes \
    --log-file="$HOME/embedded_lab3/logs/valgrind_report.txt" \
    ./leaky_app
```

Inspect the report:

```bash
less ~/embedded_lab3/logs/valgrind_report.txt
```

### Fix and Verify

After correcting the memory-management problems, build and run the corrected program.

Save the second report as:

```text
logs/valgrind_fixed.txt
```

Compare the original and corrected reports to determine whether the identified leaks have been eliminated.

### Analysis

The report should discuss:

- Definitely lost memory.
- Indirectly lost memory, if reported.
- The allocation sites associated with each leak.
- The changes made to release allocated memory.
- The difference between the original and corrected reports.

Only report leak counts and byte totals observed in the actual Valgrind output.

---

## Part C — Performance Profiling

### Overview

This part examines application performance using:

- `strace` to observe system calls.
- `perf stat` to collect performance counters where supported.
- `perf record` and `perf report` to inspect profiling samples.

The performance test program performs mathematical calculations, linear searches, and file I/O operations.

### Build the Application

```bash
cd ~/embedded_lab3/apps

gcc -g -O1 \
    -o perf_target perf_target.c -lm
```

### Trace System Calls

Run the application under `strace`:

```bash
strace \
    -o ~/embedded_lab3/logs/strace_summary.txt \
    ./perf_target
```

Collect a system-call summary:

```bash
strace -c ./perf_target
```

Save the summary output for later analysis.

### Collect Performance Data

Run performance statistics:

```bash
perf stat ./perf_target
```

Record the output in:

```text
logs/perf_stat.txt
```

Collect profiling samples:

```bash
perf record -g -- ./perf_target
```

Inspect the report:

```bash
perf report
```

Save the relevant findings in:

```text
logs/perf_report.txt
```

### Analysis

Discuss:

- The system calls used by the application.
- The system calls with the highest observed time or frequency.
- The computational and I/O characteristics of the workload.
- The performance data available from the host.
- Any limitations caused by permissions, kernel configuration, or unavailable performance events.

Performance results depend on the execution environment and should be measured rather than assumed.

---

## Part D — Real-Time Latency Analysis

### Overview

This part uses `cyclictest` to observe Linux scheduling latency.

**Important:** The latency test is performed on the host Linux system. QEMU is not a suitable substitute for measuring the real-time latency of physical embedded hardware.

### Install the Required Tools

```bash
sudo apt update
sudo apt install rt-tests gnuplot
```

### Run the Standard Test

```bash
cyclictest \
    --mlockall \
    --threads=4 \
    --priority=80 \
    --interval=1000 \
    --distance=500 \
    --loops=10000 \
    --histogram=200 \
    --histfile="$HOME/embedded_lab3/logs/hist_standard.txt"
```

The test should be run under appropriate conditions, with the machine's workload and configuration documented.

### Analyze the Results

Record the observed:

- Minimum latency.
- Average latency.
- Maximum latency.
- Latency distribution.
- Differences observed under different test conditions, if additional tests are performed.

Store the histogram data in `logs/hist_standard.txt`.

If a histogram image is generated, save it in the `logs/` directory and reference it in the report.

Do not describe the results as real-time guarantees. The measurements represent observed behavior under the specific test conditions.

---

## Build and Run

### 1. Verify the Kernel and Device Tree

```bash
ls -lh ~/embedded_lab3/output/zImage
ls -lh ~/embedded_lab3/output/vexpress-v2p-ca9.dtb
```

### 2. Verify the Initramfs

```bash
ls -lh ~/embedded_lab3/output/initramfs_lab3.cpio.gz

gzip -t ~/embedded_lab3/output/initramfs_lab3.cpio.gz
```

### 3. Boot the ARM System in QEMU

```bash
qemu-system-arm \
    -M vexpress-a9 \
    -cpu cortex-a9 \
    -smp 2 \
    -m 512M \
    -kernel ~/embedded_lab3/output/zImage \
    -dtb ~/embedded_lab3/output/vexpress-v2p-ca9.dtb \
    -initrd ~/embedded_lab3/output/initramfs_lab3.cpio.gz \
    -append "console=ttyAMA0,115200 rdinit=/sbin/init" \
    -nographic
```

Inside the QEMU shell, verify the system:

```sh
uname -a
cat /proc/cmdline
mount
ls /sys/class
```

The expected target is an ARMv7 Linux environment booted with the LAB-03 initramfs.

---

## Results and Logs

The `logs/` directory stores the evidence collected during testing.

| File | Purpose |
|---|---|
| `gdb_session.txt` | GDB debugging commands and observations |
| `valgrind_report.txt` | Initial memory-analysis report |
| `valgrind_fixed.txt` | Memory-analysis report after fixes |
| `strace_summary.txt` | System-call tracing results |
| `perf_report.txt` | Profiling analysis |
| `perf_stat.txt` | Performance statistics |
| `hist_standard.txt` | Cyclictest histogram data |
| `latency_histogram.png` | Optional latency visualization |

These filenames are suggested conventions. Include each file only after generating it, and use the actual filenames produced by your workflow.

### Screenshots and Evidence

Screenshots or captured logs should demonstrate:

- Successful QEMU boot.
- GDB breakpoints and inspection of the buggy application.
- Valgrind findings before and after the fixes.
- System-call and performance-profiling results.
- Cyclictest output and latency distribution.

---

## Lessons Learned

This lab provides practical experience with several important Embedded Linux development tools.

- **GDB:** helps inspect program state and locate errors during execution.
- **Valgrind:** helps identify memory-management problems in supported host applications.
- **strace:** reveals the system calls made by an application.
- **perf:** helps investigate application performance and profiling data when supported by the environment.
- **cyclictest:** measures observed scheduling latency under specific test conditions.
- **QEMU:** provides a convenient environment for booting and testing an ARM Linux system without requiring a physical ARM board.

The main takeaway is that debugging and performance analysis require reproducible tests, careful interpretation of tool output, and evidence-based conclusions.

---

## References

- [Reference Embedded Linux Repository](https://github.com/ntue2719-commits/embedded-linux-systems)
- [Linux Kernel Documentation](https://docs.kernel.org/)
- [QEMU Documentation](https://www.qemu.org/docs/master/)
- [GNU GDB Documentation](https://sourceware.org/gdb/documentation/)
- [Valgrind Documentation](https://valgrind.org/docs/)
- [strace Manual](https://strace.io/)
- [Linux perf Documentation](https://perf.wiki.kernel.org/)
- [rt-tests Project](https://git.kernel.org/pub/scm/utils/rt-tests/rt-tests.git/)

