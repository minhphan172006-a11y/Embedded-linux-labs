#include <string.h>
#include <errno.h>
#include <time.h>

#include "message_queue.h"

/* =========================================================
 * ALERT QUEUE
 * ========================================================= */

int alert_queue_init(AlertQueue *queue)
{
    if (queue == NULL)
        return EINVAL;

    memset(queue, 0, sizeof(*queue));

    int ret = pthread_mutex_init(&queue->mutex, NULL);
    if (ret != 0)
        return ret;

    ret = pthread_cond_init(&queue->not_empty, NULL);
    if (ret != 0) {
        pthread_mutex_destroy(&queue->mutex);
        return ret;
    }

    ret = pthread_cond_init(&queue->not_full, NULL);
    if (ret != 0) {
        pthread_cond_destroy(&queue->not_empty);
        pthread_mutex_destroy(&queue->mutex);
        return ret;
    }

    return 0;
}

int alert_queue_push(AlertQueue *queue, const AlertData *data)
{
    if (queue == NULL || data == NULL)
        return EINVAL;

    pthread_mutex_lock(&queue->mutex);

    while (queue->size == MESSAGE_QUEUE_CAPACITY &&
           !queue->closed) {
        pthread_cond_wait(&queue->not_full, &queue->mutex);
    }

    if (queue->closed) {
        pthread_mutex_unlock(&queue->mutex);
        return ECANCELED;
    }

    queue->items[queue->head] = *data;
    queue->head = (queue->head + 1) % MESSAGE_QUEUE_CAPACITY;
    queue->size++;

    pthread_cond_signal(&queue->not_empty);
    pthread_mutex_unlock(&queue->mutex);

    return 0;
}

int alert_queue_pop(AlertQueue *queue, AlertData *data)
{
    if (queue == NULL || data == NULL)
        return EINVAL;

    pthread_mutex_lock(&queue->mutex);

    while (queue->size == 0 && !queue->closed) {
        pthread_cond_wait(&queue->not_empty, &queue->mutex);
    }

    if (queue->size == 0 && queue->closed) {
        pthread_mutex_unlock(&queue->mutex);
        return ECANCELED;
    }

    *data = queue->items[queue->tail];
    queue->tail = (queue->tail + 1) % MESSAGE_QUEUE_CAPACITY;
    queue->size--;

    pthread_cond_signal(&queue->not_full);
    pthread_mutex_unlock(&queue->mutex);

    return 0;
}

void alert_queue_close(AlertQueue *queue)
{
    if (queue == NULL)
        return;

    pthread_mutex_lock(&queue->mutex);

    queue->closed = true;

    pthread_cond_broadcast(&queue->not_empty);
    pthread_cond_broadcast(&queue->not_full);

    pthread_mutex_unlock(&queue->mutex);
}

void alert_queue_destroy(AlertQueue *queue)
{
    if (queue == NULL)
        return;

    pthread_cond_destroy(&queue->not_empty);
    pthread_cond_destroy(&queue->not_full);
    pthread_mutex_destroy(&queue->mutex);
}

/* =========================================================
 * LOG QUEUE
 * ========================================================= */

int log_queue_init(LogQueue *queue)
{
    if (queue == NULL)
        return EINVAL;

    memset(queue, 0, sizeof(*queue));

    int ret = pthread_mutex_init(&queue->mutex, NULL);
    if (ret != 0)
        return ret;

    ret = pthread_cond_init(&queue->not_empty, NULL);
    if (ret != 0) {
        pthread_mutex_destroy(&queue->mutex);
        return ret;
    }

    ret = pthread_cond_init(&queue->not_full, NULL);
    if (ret != 0) {
        pthread_cond_destroy(&queue->not_empty);
        pthread_mutex_destroy(&queue->mutex);
        return ret;
    }

    return 0;
}

int log_queue_push(LogQueue *queue, const LogMessage *message)
{
    if (queue == NULL || message == NULL)
        return EINVAL;

    pthread_mutex_lock(&queue->mutex);

    while (queue->size == MESSAGE_QUEUE_CAPACITY &&
           !queue->closed) {
        pthread_cond_wait(&queue->not_full, &queue->mutex);
    }

    if (queue->closed) {
        pthread_mutex_unlock(&queue->mutex);
        return ECANCELED;
    }

    queue->items[queue->head] = *message;
    queue->head = (queue->head + 1) % MESSAGE_QUEUE_CAPACITY;
    queue->size++;

    pthread_cond_signal(&queue->not_empty);
    pthread_mutex_unlock(&queue->mutex);

    return 0;
}

int log_queue_pop(LogQueue *queue, LogMessage *message)
{
    if (queue == NULL || message == NULL)
        return EINVAL;

    pthread_mutex_lock(&queue->mutex);

    while (queue->size == 0 && !queue->closed) {
        pthread_cond_wait(&queue->not_empty, &queue->mutex);
    }

    if (queue->size == 0 && queue->closed) {
        pthread_mutex_unlock(&queue->mutex);
        return ECANCELED;
    }

    *message = queue->items[queue->tail];
    queue->tail = (queue->tail + 1) % MESSAGE_QUEUE_CAPACITY;
    queue->size--;

    pthread_cond_signal(&queue->not_full);
    pthread_mutex_unlock(&queue->mutex);

    return 0;
}

void log_queue_close(LogQueue *queue)
{
    if (queue == NULL)
        return;

    pthread_mutex_lock(&queue->mutex);

    queue->closed = true;

    pthread_cond_broadcast(&queue->not_empty);
    pthread_cond_broadcast(&queue->not_full);

    pthread_mutex_unlock(&queue->mutex);
}

void log_queue_destroy(LogQueue *queue)
{
    if (queue == NULL)
        return;

    pthread_cond_destroy(&queue->not_empty);
    pthread_cond_destroy(&queue->not_full);
    pthread_mutex_destroy(&queue->mutex);
}

/* Pop a log message with timeout */
int log_queue_timed_pop(
    LogQueue *queue,
    LogMessage *message,
    unsigned int timeout_ms)
{
    if (queue == NULL || message == NULL)
        return EINVAL;

    struct timespec deadline;

    int ret = clock_gettime(CLOCK_REALTIME, &deadline);
    if (ret != 0)
        return errno;

    deadline.tv_sec += timeout_ms / 1000;
    deadline.tv_nsec += (long)(timeout_ms % 1000) * 1000000L;

    if (deadline.tv_nsec >= 1000000000L) {
        deadline.tv_sec++;
        deadline.tv_nsec -= 1000000000L;
    }

    pthread_mutex_lock(&queue->mutex);

    while (queue->size == 0 && !queue->closed) {
        ret = pthread_cond_timedwait(
            &queue->not_empty,
            &queue->mutex,
            &deadline);

        if (ret == ETIMEDOUT) {
            pthread_mutex_unlock(&queue->mutex);
            return ETIMEDOUT;
        }

        if (ret != 0) {
            pthread_mutex_unlock(&queue->mutex);
            return ret;
        }
    }

    if (queue->size == 0 && queue->closed) {
        pthread_mutex_unlock(&queue->mutex);
        return ECANCELED;
    }

    *message = queue->items[queue->tail];

    queue->tail = (queue->tail + 1) % MESSAGE_QUEUE_CAPACITY;
    queue->size--;

    pthread_cond_signal(&queue->not_full);
    pthread_mutex_unlock(&queue->mutex);

    return 0;
}
