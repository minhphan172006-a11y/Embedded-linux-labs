#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "alert_thread.h"

static const char *alert_type_name(AlertType type)
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

static void update_statistics(
    AlertStatistics *statistics,
    AlertType type)
{
    switch (type) {
    case TEMP_HIGH:
        statistics->temp_high_count++;
        break;

    case HUMID_HIGH:
        statistics->humid_high_count++;
        break;

    case TEMP_SPIKE:
        statistics->temp_spike_count++;
        break;

    default:
        break;
    }
}

static int forward_alert_to_logger(
    LogQueue *queue,
    const AlertData *alert)
{
    LogMessage message;

    memset(&message, 0, sizeof(message));

    message.type = LOG_ALERT;
    message.data.alert = *alert;

    return log_queue_push(queue, &message);
}

void *alert_thread_func(void *arg)
{
    AlertThreadArgs *args = (AlertThreadArgs *)arg;

    if (args == NULL ||
        args->alarm_queue == NULL ||
        args->log_queue == NULL ||
        args->statistics == NULL) {
        fprintf(stderr, "[ALERT] Invalid thread arguments\n");
        return NULL;
    }

    AlertData alert;

    fprintf(stderr, "[ALERT] Thread started\n");

    while (1) {
        int ret = alert_queue_pop(args->alarm_queue, &alert);

        if (ret == ECANCELED) {
            break;
        }

        if (ret != 0) {
            fprintf(stderr,
                    "[ALERT] Queue error: %d\n", ret);
            continue;
        }

        /* Update alert counters */
        update_statistics(args->statistics, alert.type);

        /* Print alert */
        fprintf(stderr,
                "[ALERT][%llu] %s: "
                "VALUE: %.2f THRESHOLD: %.2f\n",
                (unsigned long long)alert.timestamp_ms,
                alert_type_name(alert.type),
                alert.value,
                alert.threshold);

        /* Forward alert to Logger Thread */
        ret = forward_alert_to_logger(
            args->log_queue,
            &alert);

        if (ret != 0) {
            fprintf(stderr,
                    "[ALERT] Cannot forward alert to logger: %d\n",
                    ret);
        }
    }

    fprintf(stderr, "[ALERT] Thread stopped\n");

    return NULL;
}
