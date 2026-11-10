# Assignment 2 — Sensor Monitoring System

This assignment implements a simulated sensor driver and a multithreaded userspace monitoring application. The target is ARMv7-A on QEMU vexpress-a9, using Linux kernel 6.6.21.

## Objectives

- Implement a Linux character device for simulated temperature and humidity readings.
- Enforce a configurable minimum interval between sensor reads.
- Provide an IOCTL command to change the sampling interval.
- Expose driver statistics through procfs.
- Build a userspace application with sensor, processor, alert, and logger threads.
- Coordinate threads with a circular buffer, message queues, mutexes, and atomic state.
- Handle shutdown cleanly and report monitoring statistics.

## Repository Structure

```text
ASSIGNMENT_2/
├── app/
│   ├── Makefile
│   ├── sms_app.c / sms_app.h
│   ├── circular_buffer.c / circular_buffer.h
│   ├── message_queue.c / message_queue.h
│   ├── sensor_thread.c / sensor_thread.h
│   ├── processor_thread.c / processor_thread.h
│   ├── alert_thread.c / alert_thread.h
│   ├── logger_thread.c / logger_thread.h
│   └── sms_app
├── driver/
│   ├── Makefile
│   ├── sms_sensor.h
│   └── sms_sensor_driver.c
└── logs/
    └── driver_build.log
```

Generated root filesystem images, initramfs archives, and runtime output are created in the local workspace and are not currently listed as tracked files in this directory.

## Development Environment

| Component | Configuration |
|---|---|
| Host OS | Ubuntu 26.04, x86_64 |
| Target | ARMv7-A / Cortex-A9 |
| Emulator | QEMU `vexpress-a9` |
| Kernel | Linux 6.6.21 |
| Kernel cross compiler | `arm-linux-gnueabihf-` |
| Application threading | POSIX pthreads |

The driver is cross-compiled for ARM. The application Makefile defaults to the host gcc; use an ARM cross-compiler or the checked-in ARM executable when the application must run inside the ARM guest.

## System Architecture

```text
                 Userspace: sms_app
 ┌──────────────────────────────────────────────────────┐
 │ Sensor Thread → Circular Buffer → Processor Thread   │
 │                                      │               │
 │                                      ▼               │
 │                                 Alert Queue          │
 │                                      │               │
 │                                      ▼               │
 │                                  Alert Thread        │
 │                                      │               │
 │                                      ▼               │
 │                                   Log Queue          │
 │                                      │               │
 │                                      ▼               │
 │                                  Logger Thread       │
 └──────────────────────────────────────────────────────┘
                         │ read() / ioctl()
                         ▼
                  Kernel: sms_sensor
                    /dev/sms_sensor
```

The sensor thread reads CSV samples from `/dev/sms_sensor`. The processor compares readings with configured thresholds and sends alert/log messages to the queues. The alert worker handles alert events; the logger worker writes queued records to the configured log destination.

## Kernel Driver

### Device interface

The driver registers a character device named `sms_sensor` and creates:

- `/dev/sms_sensor` — sensor data interface.
- `/proc/sms_stats` — read-only driver statistics.

Each successful read returns a CSV record:

```text
timestamp_ms,temperature,humidity
```

The simulated temperature range is 15–45 °C and humidity range is 30–90%. A mutex protects shared sensor state. Reads attempted before the minimum sampling interval has elapsed return `-EAGAIN`.

### IOCTL

The shared header defines `SMS_SET_INTERVAL` to set the minimum interval between sensor reads in milliseconds. The default driver interval is 100 ms; zero is rejected.

### Procfs statistics

`/proc/sms_stats` reports the successful read count, last temperature, last humidity, and driver uptime in seconds.

### Build the driver

```bash
cd ASSIGNMENT_2/driver
make KDIR="$HOME/embedded_lab1/kernel/linux-6.6.21" \
     ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf-
```

After copying the module into the ARM guest:

```sh
insmod sms_sensor_driver.ko
ls -l /dev/sms_sensor
cat /proc/sms_stats
dmesg | tail -n 40
```

Unload the driver after stopping the application:

```sh
rmmod sms_sensor_driver
```

## Userspace Application

The application has four worker threads:

| Thread | Responsibility |
|---|---|
| Sensor | Read samples from the device, maintain sampling cadence, and collect timing jitter |
| Processor | Consume samples from the circular buffer and evaluate alert thresholds |
| Alert | Process queued alert events and update alert statistics |
| Logger | Consume log messages and write records to the log destination |

The main program initializes the buffer and queues, starts workers, handles SIGINT/SIGTERM, and performs an ordered shutdown. It stops the sensor first, lets the processor and alert worker drain pending items, and then lets the logger finish queued messages.

### Command-line options

| Option | Meaning | Default |
|---|---|---:|
| `-i MS` | Sampling interval in milliseconds | 500 |
| `-t TEMP` | Temperature alert threshold | 40 |
| `-h HUM` | Humidity alert threshold | 80 |
| `-d SEC` | Run duration; 0 means run until stopped | 60 |
| `-v` | Enable verbose output | Off |
| `--help` | Display usage information | — |

The application uses `SMS_SET_INTERVAL` to configure the driver's minimum interval. The driver enforces its own interval while the application controls its requested polling cadence.

### Build the application

```bash
cd ASSIGNMENT_2/app
make
```

The Makefile uses gcc, pthreads, and libm. To build for the ARM guest, set the compiler explicitly and ensure the target libraries are available:

```bash
make clean
make CC=arm-linux-gnueabihf-gcc
```

### Run

After the driver is loaded and the application executable is available in the target root filesystem:

```sh
./sms_app --help
./sms_app -i 500 -t 40 -h 80 -d 60 -v
```

Use Ctrl+C to request graceful shutdown. A duration of 0 runs until a termination signal is received.

## Build and Validation Notes

- Build the driver against the same Linux 6.6.21 kernel build tree used by QEMU.
- Build or select an ARM-compatible application executable before running inside the guest; the default application Makefile compiler is host gcc.
- The tracked `logs/driver_build.log` records a previous module build that reached MODPOST and failed with an unresolved `__aeabi_uldivmod` symbol. This log is evidence of a build issue, not a successful module build. Rebuild and verify the module before claiming success.
- Validate device creation, CSV sample format, interval enforcement, IOCTL behavior, procfs statistics, thread startup, alert handling, logging, and graceful shutdown using actual runtime output.
- Do not infer runtime performance or alert counts from source code; report measured test results.

## References

- [Linux driver API](https://docs.kernel.org/driver-api/)
- [Linux ioctl documentation](https://docs.kernel.org/userspace-api/ioctl/ioctl-number.html)
- [Linux procfs documentation](https://docs.kernel.org/filesystems/proc.html)
- [Linux kernel timekeeping](https://docs.kernel.org/core-api/timekeeping.html)
- [POSIX threads](https://man7.org/linux/man-pages/man7/pthreads.7.html)
