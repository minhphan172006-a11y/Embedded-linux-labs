#ifndef SENSOR_THREAD_H
#define SENSOR_THREAD_H

#include <stdatomic.h>

#include "sms_app.h"

typedef struct {
    CircularBuffer *buffer;
    const char *device_path;

    unsigned int interval_ms;

    atomic_bool *running;
    JitterStats *jitter_stats;

    int device_fd;
} SensorThreadArgs;

void *sensor_thread_func(void *arg);

#endif
