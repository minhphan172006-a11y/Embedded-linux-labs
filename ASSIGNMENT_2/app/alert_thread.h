#ifndef ALERT_THREAD_H
#define ALERT_THREAD_H

#include "sms_app.h"
#include "message_queue.h"

typedef struct {
    AlertQueue *alarm_queue;
    LogQueue *log_queue;
    AlertStatistics *statistics;
} AlertThreadArgs;

void *alert_thread_func(void *arg);

#endif
