#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>

#include "logger_thread.h"

#define LOG_MAX_SIZE       (1024L * 1024L)
#define LOG_FLUSH_INTERVAL 5000

static const char *get_alert_name(AlertType type)
{
    switch (type) {
    case TEMP_HIGH:
        return "TEMP_HIGH";

    case HUMID_HIGH:
        return "HUMID_HIGH";

    case TEMP_SPIKE:
        return "TEMP_SPIKE";

    default:
        return "UNKNOWN";
    }
}

static int rotate_log_file(
    FILE **log_file,
    const char *log_path)
{
    char backup_path[512];

    int written = snprintf(
        backup_path,
        sizeof(backup_path),
        "%s.1",
        log_path);

    if (written < 0 || (size_t)written >= sizeof(backup_path))
        return -1;

    if (fflush(*log_file) != 0)
        return -1;

    if (fclose(*log_file) != 0) {
        *log_file = NULL;
        return -1;
    }

    *log_file = NULL;

    if (remove(backup_path) != 0 && errno != ENOENT)
        return -1;

    if (rename(log_path, backup_path) != 0 && errno != ENOENT)
        return -1;

    *log_file = fopen(log_path, "a");

    if (*log_file == NULL) {
        perror("[LOGGER] Cannot reopen log file");
        return -1;
    }

    fprintf(stderr, "[LOGGER] Log file rotated\n");

    return 0;
}

static int write_sensor_log(
    FILE *file,
    const SensorData *sensor)
{
    return fprintf(
        file,
        "[%llu] SENSOR temperature=%d C humidity=%d %%\n",
        (unsigned long long)sensor->timestamp_ms,
        sensor->temperature,
        sensor->humidity) < 0 ? -1 : 0;
}

static int write_alert_log(
    FILE *file,
    const AlertData *alert)
{
    return fprintf(
        file,
        "[%llu] ALERT %s value=%.2f threshold=%.2f\n",
        (unsigned long long)alert->timestamp_ms,
        get_alert_name(alert->type),
        alert->value,
        alert->threshold) < 0 ? -1 : 0;
}

static int write_log_message(
    FILE *file,
    const LogMessage *message)
{
    switch (message->type) {
    case LOG_SENSOR:
        return write_sensor_log(file, &message->data.sensor);

    case LOG_ALERT:
        return write_alert_log(file, &message->data.alert);

    default:
        fprintf(stderr, "[LOGGER] Unknown message type\n");
        return -1;
    }
}

void *logger_thread_func(void *arg)
{
    LoggerThreadArgs *args = (LoggerThreadArgs *)arg;

    if (args == NULL || args->log_queue == NULL) {
        fprintf(stderr, "[LOGGER] Invalid arguments\n");
        return NULL;
    }

    const char *path = args->log_path != NULL
                       ? args->log_path
                       : SMS_LOG_PATH;

    FILE *log_file = fopen(path, "a");

    if (log_file == NULL) {
        perror("[LOGGER] Cannot open log file");
        return NULL;
    }

    fprintf(stderr, "[LOGGER] Thread started: %s\n", path);

    LogMessage message;

    while (1) {
        int ret = log_queue_timed_pop(
            args->log_queue,
            &message,
            LOG_FLUSH_INTERVAL);

        if (ret == ECANCELED)
            break;

        if (ret == ETIMEDOUT) {
            if (fflush(log_file) != 0)
                perror("[LOGGER] Flush failed");

            continue;
        }

        if (ret != 0) {
            fprintf(stderr,
                    "[LOGGER] Queue error: %d\n", ret);
            continue;
        }

        if (write_log_message(log_file, &message) != 0) {
            fprintf(stderr, "[LOGGER] Write failed\n");
            continue;
        }

        long position = ftell(log_file);

        if (position < 0) {
            perror("[LOGGER] ftell failed");
            continue;
        }

        if (position > LOG_MAX_SIZE) {
            if (rotate_log_file(&log_file, path) != 0) {
                fprintf(stderr,
                        "[LOGGER] Log rotation failed\n");

                if (log_file == NULL)
                    break;
            }
        }
    }

    if (log_file != NULL) {
        if (fflush(log_file) != 0)
            perror("[LOGGER] Final flush failed");

        fclose(log_file);
    }

    fprintf(stderr, "[LOGGER] Thread stopped\n");

    return NULL;
}
