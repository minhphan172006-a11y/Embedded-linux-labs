#ifndef CIRCULAR_BUFFER_H
#define CIRCULAR_BUFFER_H

#include "sms_app.h"

int circular_buffer_init(CircularBuffer *cb);
int circular_buffer_push(CircularBuffer *cb, const SensorData *data);
int circular_buffer_pop(CircularBuffer *cb, SensorData *data);

void circular_buffer_close(CircularBuffer *cb);
void circular_buffer_destroy(CircularBuffer *cb);

size_t circular_buffer_get_size(CircularBuffer *cb);
uint64_t circular_buffer_get_total_produced(CircularBuffer *cb);
uint64_t circular_buffer_get_total_consumed(CircularBuffer *cb);
uint64_t circular_buffer_get_overflow_count(CircularBuffer *cb);

#endif
