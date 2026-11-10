#ifndef LOGGER_THREAD_H
#define LOGGER_THREAD_H

#include "sms_app.h"
#include "message_queue.h"

typedef struct {
    LogQueue *log_queue;
    const char *log_path;
} LoggerThreadArgs;

void *logger_thread_func(void *arg);

#endif
