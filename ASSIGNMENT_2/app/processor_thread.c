#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "processor_thread.h"
#include "circular_buffer.h"

/* Rolling average */
typedef struct {
    double values[SMS_AVERAGE_WINDOW];

    size_t index;
    size_t count;

    double sum;
} RollingAverage;

static void rolling_average_init(RollingAverage *avg)
{
    avg->index = 0;
    avg->count = 0;
    avg->sum = 0.0;
}

static double rolling_average_update(
    RollingAverage *avg,
    double value)
{
    if (avg->count == SMS_AVERAGE_WINDOW) {
        avg->sum -= avg->values[avg->index];
    } else {
        avg->count++;
    }

    avg->values[avg->index] = value;
    avg->sum += value;

    avg->index = (avg->index + 1) % SMS_AVERAGE_WINDOW;

    return avg->sum / (double)avg->count;
}

/* Send one alert to Alert Thread */
static int send_alert(
    ProcessorThreadArgs *args,
    AlertType type,
    const SensorData *sample,
    double value,
    double threshold)
{
    AlertData alert = {
        .type = type,
        .timestamp_ms = sample->timestamp_ms,
        .value = value,
        .threshold = threshold
    };

    return alert_queue_push(args->alarm_queue, &alert);
}

/* Send sensor sample to Logger Thread */
static int send_sensor_log(
    ProcessorThreadArgs *args,
    const SensorData *sample)
{
    LogMessage message = {
        .type = LOG_SENSOR,
        .data.sensor = *sample
    };

    return log_queue_push(args->log_queue, &message);
}

void *processor_thread_func(void *arg)
{
    ProcessorThreadArgs *args = (ProcessorThreadArgs *)arg;

    if (args == NULL ||
        args->buffer == NULL ||
        args->alarm_queue == NULL ||
        args->log_queue == NULL) {
        fprintf(stderr, "[PROCESSOR] Invalid arguments\n");
        return NULL;
    }

    RollingAverage temperature_avg;
    RollingAverage humidity_avg;

    rolling_average_init(&temperature_avg);
    rolling_average_init(&humidity_avg);

    SensorData sample;
    int previous_temperature = 0;
    int has_previous_temperature = 0;

    printf("[PROCESSOR] Thread started\n");

    while (1) {
        int ret = circular_buffer_pop(args->buffer, &sample);

        if (ret == ECANCELED) {
            break;
        }

        if (ret != 0) {
            fprintf(stderr,
                    "[PROCESSOR] Buffer error: %d\n", ret);
            continue;
        }

        /* Update rolling averages */
        double avg_temperature =
            rolling_average_update(
                &temperature_avg,
                (double)sample.temperature);

        double avg_humidity =
            rolling_average_update(
                &humidity_avg,
                (double)sample.humidity);

        printf("[PROCESSOR] "
               "Time=%llu ms "
               "Temp=%d C "
               "Humidity=%d %% "
               "AvgTemp=%.2f "
               "AvgHumidity=%.2f\n",
               (unsigned long long)sample.timestamp_ms,
               sample.temperature,
               sample.humidity,
               avg_temperature,
               avg_humidity);

        /* Temperature high alert */
        if ((double)sample.temperature >
            args->temperature_threshold) {

            ret = send_alert(
                args,
                TEMP_HIGH,
                &sample,
                (double)sample.temperature,
                args->temperature_threshold);

            if (ret != 0) {
                fprintf(stderr,
                        "[PROCESSOR] Cannot send TEMP_HIGH alert\n");
            }
        }

        /* Humidity high alert */
        if ((double)sample.humidity >
            args->humidity_threshold) {

            ret = send_alert(
                args,
                HUMID_HIGH,
                &sample,
                (double)sample.humidity,
                args->humidity_threshold);

            if (ret != 0) {
                fprintf(stderr,
                        "[PROCESSOR] Cannot send HUMID_HIGH alert\n");
            }
        }

        /* Temperature spike alert: absolute change > 5 C */
        if (has_previous_temperature) {
            int delta = abs(
                sample.temperature - previous_temperature);

            if (delta > 5) {
                ret = send_alert(
                    args,
                    TEMP_SPIKE,
                    &sample,
                    (double)delta,
                    5.0);

                if (ret != 0) {
                    fprintf(stderr,
                            "[PROCESSOR] Cannot send TEMP_SPIKE alert\n");
                }
            }
        }

        previous_temperature = sample.temperature;
        has_previous_temperature = 1;

        /* Send processed sample to Logger Thread */
        ret = send_sensor_log(args, &sample);

        if (ret != 0) {
            fprintf(stderr,
                    "[PROCESSOR] Cannot send sensor log: %d\n",
                    ret);
        }
    }

    printf("[PROCESSOR] Thread stopped\n");

    return NULL;
}
