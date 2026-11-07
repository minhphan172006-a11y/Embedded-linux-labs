#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sched.h>
#include <pthread.h>
#include <sys/ioctl.h>

#include "sensor_thread.h"
#include "circular_buffer.h"
#include "../driver/sms_sensor.h"

#define JITTER_SAMPLE_LIMIT 1000
#define SENSOR_READ_SIZE    128
#define SENSOR_FIFO_PRIORITY 50

static uint64_t calculate_jitter_us(const struct timespec *actual,
                                    const struct timespec *scheduled)
{
    int64_t actual_ns =
        (int64_t)actual->tv_sec * 1000000000LL + actual->tv_nsec;

    int64_t scheduled_ns =
        (int64_t)scheduled->tv_sec * 1000000000LL +
        scheduled->tv_nsec;

    uint64_t difference = (uint64_t)(
        actual_ns >= scheduled_ns
        ? actual_ns - scheduled_ns
        : scheduled_ns - actual_ns
    );

    return difference / 1000ULL;
}

static int compare_u64(const void *a, const void *b)
{
    uint64_t x = *(const uint64_t *)a;
    uint64_t y = *(const uint64_t *)b;

    if (x < y)
        return -1;
    if (x > y)
        return 1;
    return 0;
}

static void record_jitter(JitterStats *stats,
                          uint64_t samples[],
                          uint64_t jitter_us)
{
    if (stats == NULL || stats->sample_count >= JITTER_SAMPLE_LIMIT)
        return;

    samples[stats->sample_count] = jitter_us;

    if (stats->sample_count == 0 || jitter_us < stats->min_jitter_us)
        stats->min_jitter_us = jitter_us;

    if (jitter_us > stats->max_jitter_us)
        stats->max_jitter_us = jitter_us;

    stats->sample_count++;

    if (jitter_us < 100)
        stats->histogram_0_100_us++;
    else if (jitter_us < 500)
        stats->histogram_100_500_us++;
    else if (jitter_us < 1000)
        stats->histogram_500_1000_us++;
    else
        stats->histogram_over_1000_us++;
}

static void finish_jitter_statistics(JitterStats *stats,
                                     uint64_t samples[])
{
    uint64_t total = 0;
    size_t i;
    size_t p99_index;

    if (stats == NULL || stats->sample_count == 0)
        return;

    for (i = 0; i < stats->sample_count; i++)
        total += samples[i];

    stats->average_jitter_us =
        (double)total / (double)stats->sample_count;

    qsort(samples, stats->sample_count,
          sizeof(samples[0]), compare_u64);

    p99_index = (stats->sample_count * 99 + 99) / 100;
    if (p99_index > 0)
        p99_index--;

    if (p99_index >= stats->sample_count)
        p99_index = stats->sample_count - 1;

    stats->p99_jitter_us = samples[p99_index];
}

static int parse_sensor_data(const char *text, SensorData *data)
{
    unsigned long long timestamp = 0;
    int temperature = 0;
    int humidity = 0;
    int fields;

    if (text == NULL || data == NULL)
        return -1;

    fields = sscanf(text, "%llu,%d,%d",
                    &timestamp, &temperature, &humidity);

    if (fields != 3) {
        fprintf(stderr,
                "[PARSER DEBUG] fields=%d, length=%zu, data=[%s]\n",
                fields, strlen(text), text);
        return -1;
    }

    data->timestamp_ms = (uint64_t)timestamp;
    data->temperature = temperature;
    data->humidity = humidity;

    return 0;
}

static int configure_sensor_interval(int fd, unsigned int interval_ms)
{
    uint32_t value = (uint32_t)interval_ms;

    if (ioctl(fd, SMS_SET_INTERVAL, &value) < 0) {
        perror("[SENSOR] SMS_SET_INTERVAL ioctl");
        return -1;
    }

    return 0;
}

static int sleep_until(const struct timespec *deadline,
                       atomic_bool *running)
{
    int ret;

    while (atomic_load(running)) {
        ret = clock_nanosleep(CLOCK_MONOTONIC,
                              TIMER_ABSTIME,
                              deadline,
                              NULL);

        if (ret == 0)
            return 0;

        if (ret == EINTR)
            continue;

        fprintf(stderr,
                "[SENSOR] clock_nanosleep failed: %s\n",
                strerror(ret));
        return -1;
    }

    return 1;
}

static void configure_sensor_scheduling(void)
{
    struct sched_param param;
    const char *mode = getenv("SMS_SCHEDULER");
    int policy = SCHED_FIFO;
    int priority = SENSOR_FIFO_PRIORITY;

    if (mode != NULL && strcmp(mode, "OTHER") == 0) {
        policy = SCHED_OTHER;
        priority = 0;
    }

    memset(&param, 0, sizeof(param));
    param.sched_priority = priority;

    int ret = pthread_setschedparam(pthread_self(), policy, &param);

    if (ret != 0) {
        fprintf(stderr,
                "[SENSOR] Cannot set scheduler policy %s: %s\n",
                policy == SCHED_FIFO ? "SCHED_FIFO" : "SCHED_OTHER",
                strerror(ret));
        return;
    }

    printf("[SENSOR] Scheduler: %s, priority=%d\n",
           policy == SCHED_FIFO ? "SCHED_FIFO" : "SCHED_OTHER",
           priority);
}

void *sensor_thread_func(void *arg)
{
    SensorThreadArgs *args = (SensorThreadArgs *)arg;
    const char *device_path;
    unsigned int interval_ms;
    int fd = -1;
    char raw_data[SENSOR_READ_SIZE];
    uint64_t jitter_samples[JITTER_SAMPLE_LIMIT];
    struct timespec next_wakeup;
    JitterStats *stats;

    if (args == NULL || args->buffer == NULL ||
        args->running == NULL || args->interval_ms == 0) {
        fprintf(stderr, "[SENSOR] Invalid thread arguments.\n");
        return NULL;
    }

    device_path = args->device_path != NULL
                  ? args->device_path
                  : SMS_DEVICE_PATH;

    interval_ms = args->interval_ms;
    stats = args->jitter_stats;

    if (stats != NULL)
        memset(stats, 0, sizeof(*stats));

    memset(jitter_samples, 0, sizeof(jitter_samples));

    configure_sensor_scheduling();

    fd = open(device_path, O_RDONLY);
    if (fd < 0) {
        perror("[SENSOR] Cannot open sensor device");
        return NULL;
    }

    args->device_fd = fd;

    if (configure_sensor_interval(fd, interval_ms) < 0)
        goto cleanup;

    if (clock_gettime(CLOCK_MONOTONIC, &next_wakeup) < 0) {
        perror("[SENSOR] clock_gettime");
        goto cleanup;
    }

    printf("[SENSOR] Device: %s\n", device_path);
    printf("[SENSOR] Sampling interval: %u ms\n", interval_ms);

    while (atomic_load(args->running)) {
        struct timespec actual_wakeup;
        SensorData data;
        ssize_t bytes_read;
        int sleep_result;

        next_wakeup.tv_nsec += (long)interval_ms * 1000000L;

        while (next_wakeup.tv_nsec >= 1000000000L) {
            next_wakeup.tv_nsec -= 1000000000L;
            next_wakeup.tv_sec++;
        }

        sleep_result = sleep_until(&next_wakeup, args->running);

        if (sleep_result != 0)
            break;

        if (clock_gettime(CLOCK_MONOTONIC, &actual_wakeup) == 0) {
            uint64_t jitter_us =
                calculate_jitter_us(&actual_wakeup, &next_wakeup);

            record_jitter(stats, jitter_samples, jitter_us);
        }

        if (!atomic_load(args->running))
            break;

        bytes_read = read(fd, raw_data, sizeof(raw_data) - 1);

        if (bytes_read < 0) {
            if (errno == EINTR)
                continue;

            if (errno == EAGAIN || errno == EWOULDBLOCK)
                continue;

            perror("[SENSOR] read");
            break;
        }

        if (bytes_read == 0) {
            fprintf(stderr, "[SENSOR] Device returned EOF.\n");
            continue;
        }

        raw_data[bytes_read] = '\0';

        if (parse_sensor_data(raw_data, &data) < 0) {
            fprintf(stderr,
                    "[SENSOR] Invalid CSV data: %s\n",
                    raw_data);
            continue;
        }

        int push_result = circular_buffer_push(args->buffer, &data);

        if (push_result == ENOSPC) {
            fprintf(stderr,
                    "[SENSOR] Circular buffer full; sample dropped.\n");
        } else if (push_result == ECANCELED) {
            break;
        } else if (push_result != 0) {
            fprintf(stderr,
                    "[SENSOR] Buffer push failed: %s\n",
                    strerror(push_result));
            break;
        }
    }

    finish_jitter_statistics(stats, jitter_samples);

cleanup:
    close(fd);
    args->device_fd = -1;

    printf("[SENSOR] Thread stopped.\n");
    return NULL;
}
