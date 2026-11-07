#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <getopt.h>
#include <time.h>
#include <pthread.h>
#include <stdatomic.h>
#include <sys/mman.h>

#include "sms_app.h"
#include "circular_buffer.h"
#include "message_queue.h"
#include "sensor_thread.h"
#include "processor_thread.h"
#include "alert_thread.h"
#include "logger_thread.h"
#include "../driver/sms_sensor.h"

static volatile sig_atomic_t stop_requested = 0;

static void handle_signal(int signal_number)
{
    (void)signal_number;
    stop_requested = 1;
}

static void print_usage(const char *program)
{
    printf("Usage: %s [OPTIONS]\n", program);
    printf("Options:\n");
    printf("  -i MS    Sensor sampling interval (default: 500)\n");
    printf("  -t TEMP  Temperature alert threshold (default: 40)\n");
    printf("  -h HUM   Humidity alert threshold (default: 80)\n");
    printf("  -d SEC   Run duration in seconds; 0 means infinite\n");
    printf("  -v       Enable verbose output\n");
    printf("  --help   Display this help message\n");
}

static int parse_unsigned(const char *text,
                          unsigned int *value)
{
    char *end = NULL;
    unsigned long parsed;

    if (text == NULL || value == NULL || *text == '\0')
        return -1;

    errno = 0;
    parsed = strtoul(text, &end, 10);

    if (errno != 0 || end == text || *end != '\0' ||
        parsed > UINT32_MAX)
        return -1;

    *value = (unsigned int)parsed;
    return 0;
}

static int parse_double_value(const char *text, double *value)
{
    char *end = NULL;
    double parsed;

    if (text == NULL || value == NULL || *text == '\0')
        return -1;

    errno = 0;
    parsed = strtod(text, &end);

    if (errno != 0 || end == text || *end != '\0')
        return -1;

    *value = parsed;
    return 0;
}

static int parse_arguments(int argc, char *argv[], SMSConfig *config)
{
    int option;

    static const struct option long_options[] = {
        {"help", no_argument, NULL, 'H'},
        {NULL, 0, NULL, 0}
    };

    config->interval_ms = 500;
    config->temperature_threshold = 40.0;
    config->humidity_threshold = 80.0;
    config->duration_sec = 60;
    config->verbose = false;

    while ((option = getopt_long(argc, argv, "i:t:h:d:v",
                                  long_options, NULL)) != -1) {
        switch (option) {
        case 'i':
            if (parse_unsigned(optarg, &config->interval_ms) < 0 ||
                config->interval_ms == 0) {
                fprintf(stderr, "Invalid interval: %s\n", optarg);
                return -1;
            }
            break;

        case 't':
            if (parse_double_value(optarg,
                                   &config->temperature_threshold) < 0) {
                fprintf(stderr, "Invalid temperature threshold.\n");
                return -1;
            }
            break;

        case 'h':
            if (parse_double_value(optarg,
                                   &config->humidity_threshold) < 0) {
                fprintf(stderr, "Invalid humidity threshold.\n");
                return -1;
            }
            break;

        case 'd':
            if (parse_unsigned(optarg, &config->duration_sec) < 0) {
                fprintf(stderr, "Invalid duration: %s\n", optarg);
                return -1;
            }
            break;

        case 'v':
            config->verbose = true;
            break;

        case 'H':
            print_usage(argv[0]);
            exit(EXIT_SUCCESS);

        default:
            print_usage(argv[0]);
            return -1;
        }
    }

    if (optind != argc) {
        fprintf(stderr, "Unexpected argument: %s\n", argv[optind]);
        return -1;
    }

    return 0;
}

static bool duration_expired(const struct timespec *start,
                             unsigned int duration_sec)
{
    struct timespec now;
    uint64_t elapsed_ns;

    if (duration_sec == 0)
        return false;

    if (clock_gettime(CLOCK_MONOTONIC, &now) < 0)
        return false;

    elapsed_ns =
        (uint64_t)(now.tv_sec - start->tv_sec) * 1000000000ULL;

    if (now.tv_nsec >= start->tv_nsec)
        elapsed_ns += (uint64_t)(now.tv_nsec - start->tv_nsec);
    else
        elapsed_ns -= (uint64_t)(start->tv_nsec - now.tv_nsec);

    return elapsed_ns >= (uint64_t)duration_sec * 1000000000ULL;
}

static void print_final_statistics(const CircularBuffer *buffer,
                                   const AlertStatistics *alerts,
                                   const JitterStats *jitter)
{
    printf("\n========== FINAL STATISTICS ==========\n");

    printf("Samples produced : %llu\n",
           (unsigned long long)
           circular_buffer_get_total_produced((CircularBuffer *)buffer));

    printf("Samples consumed : %llu\n",
           (unsigned long long)
           circular_buffer_get_total_consumed((CircularBuffer *)buffer));

    printf("Buffer overflows : %llu\n",
           (unsigned long long)
           circular_buffer_get_overflow_count((CircularBuffer *)buffer));

    printf("\n========== ALERT STATISTICS ==========\n");

    printf("TEMP_HIGH  : %llu\n",
           (unsigned long long)alerts->temp_high_count);

    printf("HUMID_HIGH : %llu\n",
           (unsigned long long)alerts->humid_high_count);

    printf("TEMP_SPIKE : %llu\n",
           (unsigned long long)alerts->temp_spike_count);

    printf("\n========== JITTER STATISTICS ==========\n");

    if (jitter->sample_count == 0) {
        printf("No jitter samples collected.\n");
    } else {
        printf("Sample count : %llu\n",
               (unsigned long long)jitter->sample_count);

        printf("Minimum      : %llu us\n",
               (unsigned long long)jitter->min_jitter_us);

        printf("Average      : %.2f us\n",
               jitter->average_jitter_us);

        printf("Maximum      : %llu us\n",
               (unsigned long long)jitter->max_jitter_us);

        printf("P99          : %llu us\n",
               (unsigned long long)jitter->p99_jitter_us);

        printf("\nJitter histogram:\n");
        printf("0-100 us     : %llu\n",
               (unsigned long long)jitter->histogram_0_100_us);
        printf("100-500 us   : %llu\n",
               (unsigned long long)jitter->histogram_100_500_us);
        printf("500-1000 us  : %llu\n",
               (unsigned long long)jitter->histogram_500_1000_us);
        printf(">1000 us     : %llu\n",
               (unsigned long long)jitter->histogram_over_1000_us);
    }

    printf("======================================\n");
}

int main(int argc, char *argv[])
{
    SMSConfig config;

    CircularBuffer sensor_buffer;
    AlertQueue alarm_queue;
    LogQueue log_queue;

    AlertStatistics alert_statistics = {0};
    JitterStats jitter_statistics = {0};

    SensorThreadArgs sensor_args;
    ProcessorThreadArgs processor_args;
    AlertThreadArgs alert_args;
    LoggerThreadArgs logger_args;

    pthread_t sensor_thread;
    pthread_t processor_thread;
    pthread_t alert_thread;
    pthread_t logger_thread;

    atomic_bool running;

    bool buffer_initialized = false;
    bool alarm_queue_initialized = false;
    bool log_queue_initialized = false;

    bool sensor_created = false;
    bool processor_created = false;
    bool alert_created = false;
    bool logger_created = false;

    struct sigaction action;
    struct timespec start_time;
    int exit_status = EXIT_FAILURE;
    int ret;

    if (parse_arguments(argc, argv, &config) < 0)
        return EXIT_FAILURE;

    memset(&action, 0, sizeof(action));
    action.sa_handler = handle_signal;
    sigemptyset(&action.sa_mask);

    if (sigaction(SIGINT, &action, NULL) < 0 ||
        sigaction(SIGTERM, &action, NULL) < 0) {
        perror("sigaction");
        return EXIT_FAILURE;
    }

    atomic_init(&running, true);

    printf("========================================\n");
    printf("       SENSOR MONITORING SYSTEM\n");
    printf("========================================\n");
    printf("Device                  : %s\n", SMS_DEVICE_PATH);
    printf("Sampling interval       : %u ms\n", config.interval_ms);
    printf("Temperature threshold   : %.2f C\n",
           config.temperature_threshold);
    printf("Humidity threshold      : %.2f %%\n",
           config.humidity_threshold);
    printf("Duration                : %u seconds\n",
           config.duration_sec);
    printf("Verbose                 : %s\n",
           config.verbose ? "ON" : "OFF");


    ret = circular_buffer_init(&sensor_buffer);
    if (ret != 0) {
        fprintf(stderr, "Cannot initialize circular buffer: %s\n",
                strerror(ret));
        goto cleanup;
    }
    buffer_initialized = true;

    ret = alert_queue_init(&alarm_queue);
    if (ret != 0) {
        fprintf(stderr, "Cannot initialize alert queue: %s\n",
                strerror(ret));
        goto cleanup;
    }
    alarm_queue_initialized = true;

    ret = log_queue_init(&log_queue);
    if (ret != 0) {
        fprintf(stderr, "Cannot initialize log queue: %s\n",
                strerror(ret));
        goto cleanup;
    }
    log_queue_initialized = true;

    logger_args.log_queue = &log_queue;
    logger_args.log_path = SMS_LOG_PATH;

    ret = pthread_create(&logger_thread, NULL,
                         logger_thread_func, &logger_args);
    if (ret != 0) {
        fprintf(stderr, "Cannot create logger thread: %s\n",
                strerror(ret));
        goto cleanup;
    }
    logger_created = true;

    alert_args.alarm_queue = &alarm_queue;
    alert_args.log_queue = &log_queue;
    alert_args.statistics = &alert_statistics;

    ret = pthread_create(&alert_thread, NULL,
                         alert_thread_func, &alert_args);
    if (ret != 0) {
        fprintf(stderr, "Cannot create alert thread: %s\n",
                strerror(ret));
        goto cleanup;
    }
    alert_created = true;

    processor_args.buffer = &sensor_buffer;
    processor_args.alarm_queue = &alarm_queue;
    processor_args.log_queue = &log_queue;
    processor_args.temperature_threshold =
        config.temperature_threshold;
    processor_args.humidity_threshold =
        config.humidity_threshold;

    ret = pthread_create(&processor_thread, NULL,
                         processor_thread_func, &processor_args);
    if (ret != 0) {
        fprintf(stderr, "Cannot create processor thread: %s\n",
                strerror(ret));
        goto cleanup;
    }
    processor_created = true;

    sensor_args.buffer = &sensor_buffer;
    sensor_args.device_path = SMS_DEVICE_PATH;
    sensor_args.interval_ms = config.interval_ms;
    sensor_args.running = &running;
    sensor_args.jitter_stats = &jitter_statistics;
    sensor_args.device_fd = -1;

    ret = pthread_create(&sensor_thread, NULL,
                         sensor_thread_func, &sensor_args);
    if (ret != 0) {
        fprintf(stderr, "Cannot create sensor thread: %s\n",
                strerror(ret));
        goto cleanup;
    }
    sensor_created = true;

    /*
     * Attempt to lock memory after all worker threads have been created.
     * This prevents MCL_FUTURE from blocking pthread stack allocation.
     */
    if (mlockall(MCL_CURRENT | MCL_FUTURE) < 0) {
        fprintf(stderr,
                "[MAIN] mlockall unavailable: %s\\n",
                strerror(errno));
    } else {
        printf("[MAIN] Memory locked using mlockall().\\n");
    }


    if (clock_gettime(CLOCK_MONOTONIC, &start_time) < 0) {
        perror("clock_gettime");
        goto cleanup;
    }

    printf("[MAIN] All four threads started.\n");
    printf("[MAIN] Press Ctrl+C to stop.\n");

    while (!stop_requested &&
           !duration_expired(&start_time, config.duration_sec)) {
        struct timespec delay = {
            .tv_sec = 0,
            .tv_nsec = 100000000L
        };

        while (nanosleep(&delay, &delay) < 0 && errno == EINTR) {
            if (stop_requested)
                break;
        }
    }

    exit_status = EXIT_SUCCESS;

cleanup:
    printf("\n[MAIN] Starting graceful shutdown...\n");

    atomic_store(&running, false);

    /* 1. Stop sensor before closing the circular buffer. */
    if (sensor_created)
        pthread_join(sensor_thread, NULL);

    /* 2. Processor drains the remaining sensor samples. */
    if (buffer_initialized)
        circular_buffer_close(&sensor_buffer);

    if (processor_created)
        pthread_join(processor_thread, NULL);

    /* 3. Alert thread drains pending alerts. */
    if (alarm_queue_initialized)
        alert_queue_close(&alarm_queue);

    if (alert_created)
        pthread_join(alert_thread, NULL);

    /* 4. Logger drains all remaining messages and exits. */
    if (log_queue_initialized)
        log_queue_close(&log_queue);

    if (logger_created)
        pthread_join(logger_thread, NULL);

    if (buffer_initialized) {
        if (exit_status == EXIT_SUCCESS) {
            print_final_statistics(&sensor_buffer,
                                   &alert_statistics,
                                   &jitter_statistics);
        }
        circular_buffer_destroy(&sensor_buffer);
    }

    if (alarm_queue_initialized)
        alert_queue_destroy(&alarm_queue);

    if (log_queue_initialized)
        log_queue_destroy(&log_queue);

    printf("[MAIN] Shutdown complete.\n");

    return exit_status;
}
