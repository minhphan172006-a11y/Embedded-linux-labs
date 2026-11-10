#ifndef PROCESSOR_THREAD_H
#define PROCESSOR_THREAD_H

#include "sms_app.h"
#include "message_queue.h"

typedef struct {
    CircularBuffer *buffer;

    AlertQueue *alarm_queue;
    LogQueue *log_queue;

    double temperature_threshold;
    double humidity_threshold;
} ProcessorThreadArgs;

void *processor_thread_func(void *arg);

#endif
