#include <string.h>
#include <errno.h>

#include "circular_buffer.h"

int circular_buffer_init(CircularBuffer *cb)
{
    if (cb == NULL)
        return EINVAL;

    memset(cb, 0, sizeof(*cb));

    int ret = pthread_mutex_init(&cb->mutex, NULL);
    if (ret != 0)
        return ret;

    ret = pthread_cond_init(&cb->not_empty, NULL);
    if (ret != 0) {
        pthread_mutex_destroy(&cb->mutex);
        return ret;
    }

    ret = pthread_cond_init(&cb->not_full, NULL);
    if (ret != 0) {
        pthread_cond_destroy(&cb->not_empty);
        pthread_mutex_destroy(&cb->mutex);
        return ret;
    }

    return 0;
}

int circular_buffer_push(CircularBuffer *cb, const SensorData *data)
{
    if (cb == NULL || data == NULL)
        return EINVAL;

    pthread_mutex_lock(&cb->mutex);

    if (cb->closed) {
        pthread_mutex_unlock(&cb->mutex);
        return ECANCELED;
    }

    if (cb->size == SMS_BUFFER_CAPACITY) {
        cb->overflow_count++;
        pthread_mutex_unlock(&cb->mutex);
        return ENOSPC;
    }

    cb->data[cb->head] = *data;
    cb->head = (cb->head + 1) % SMS_BUFFER_CAPACITY;
    cb->size++;
    cb->total_produced++;

    pthread_cond_signal(&cb->not_empty);
    pthread_mutex_unlock(&cb->mutex);

    return 0;
}

int circular_buffer_pop(CircularBuffer *cb, SensorData *data)
{
    if (cb == NULL || data == NULL)
        return EINVAL;

    pthread_mutex_lock(&cb->mutex);

    while (cb->size == 0 && !cb->closed) {
        pthread_cond_wait(&cb->not_empty, &cb->mutex);
    }

    if (cb->size == 0 && cb->closed) {
        pthread_mutex_unlock(&cb->mutex);
        return ECANCELED;
    }

    *data = cb->data[cb->tail];
    cb->tail = (cb->tail + 1) % SMS_BUFFER_CAPACITY;
    cb->size--;
    cb->total_consumed++;

    pthread_cond_signal(&cb->not_full);
    pthread_mutex_unlock(&cb->mutex);

    return 0;
}

void circular_buffer_close(CircularBuffer *cb)
{
    if (cb == NULL)
        return;

    pthread_mutex_lock(&cb->mutex);

    cb->closed = true;

    pthread_cond_broadcast(&cb->not_empty);
    pthread_cond_broadcast(&cb->not_full);

    pthread_mutex_unlock(&cb->mutex);
}

void circular_buffer_destroy(CircularBuffer *cb)
{
    if (cb == NULL)
        return;

    pthread_cond_destroy(&cb->not_empty);
    pthread_cond_destroy(&cb->not_full);
    pthread_mutex_destroy(&cb->mutex);
}

size_t circular_buffer_get_size(CircularBuffer *cb)
{
    if (cb == NULL)
        return 0;

    pthread_mutex_lock(&cb->mutex);
    size_t value = cb->size;
    pthread_mutex_unlock(&cb->mutex);

    return value;
}

uint64_t circular_buffer_get_total_produced(CircularBuffer *cb)
{
    if (cb == NULL)
        return 0;

    pthread_mutex_lock(&cb->mutex);
    uint64_t value = cb->total_produced;
    pthread_mutex_unlock(&cb->mutex);

    return value;
}

uint64_t circular_buffer_get_total_consumed(CircularBuffer *cb)
{
    if (cb == NULL)
        return 0;

    pthread_mutex_lock(&cb->mutex);
    uint64_t value = cb->total_consumed;
    pthread_mutex_unlock(&cb->mutex);

    return value;
}

uint64_t circular_buffer_get_overflow_count(CircularBuffer *cb)
{
    if (cb == NULL)
        return 0;

    pthread_mutex_lock(&cb->mutex);
    uint64_t value = cb->overflow_count;
    pthread_mutex_unlock(&cb->mutex);

    return value;
}
