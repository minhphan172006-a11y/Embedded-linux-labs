#ifndef SMS_APP_H
#define SMS_APP_H

#include <stdint.h>
#include <stddef.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdatomic.h>

#define SMS_BUFFER_CAPACITY 64
#define SMS_AVERAGE_WINDOW  10
#define SMS_LOG_PATH        "/tmp/sms.log"

/* Sensor sample */
typedef struct {
    uint64_t timestamp_ms;
    int temperature;
    int humidity;
} SensorData;

/* Alert types */
typedef enum {
    TEMP_HIGH = 0,
    HUMID_HIGH,
    TEMP_SPIKE
} AlertType;

/* Alert information */
typedef struct {
    AlertType type;
    uint64_t timestamp_ms;
    double value;
    double threshold;
} AlertData;

/* Logger message types */
typedef enum {
    LOG_SENSOR = 0,
    LOG_ALERT
} LogMessageType;

/* Message sent to Logger Thread */
typedef struct {
    LogMessageType type;

    union {
        SensorData sensor;
        AlertData alert;
    } data;
} LogMessage;

/* Circular buffer */
typedef struct {
    SensorData data[SMS_BUFFER_CAPACITY];

    size_t head;
    size_t tail;
    size_t size;

    uint64_t total_produced;
    uint64_t total_consumed;
    uint64_t overflow_count;

    bool closed;

    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} CircularBuffer;

/* Application configuration */
typedef struct {
    unsigned int interval_ms;
    double temperature_threshold;
    double humidity_threshold;
    unsigned int duration_sec;
    bool verbose;
} SMSConfig;

/* Application statistics */
typedef struct {
    uint64_t temp_high_count;
    uint64_t humid_high_count;
    uint64_t temp_spike_count;
} AlertStatistics;


/* Sensor scheduling statistics */
typedef struct {
    uint64_t sample_count;
    uint64_t min_jitter_us;
    uint64_t max_jitter_us;
    double average_jitter_us;
    uint64_t p99_jitter_us;

    uint64_t histogram_0_100_us;
    uint64_t histogram_100_500_us;
    uint64_t histogram_500_1000_us;
    uint64_t histogram_over_1000_us;
} JitterStats;



#endif
