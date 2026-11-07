#ifndef SMS_SENSOR_H
#define SMS_SENSOR_H

#include <linux/ioctl.h>
#include <linux/types.h>

#define SMS_DEVICE_NAME "sms_sensor"
#define SMS_DEVICE_PATH "/dev/sms_sensor"
#define SMS_PROC_NAME   "sms_stats"

#define SMS_IOC_MAGIC 'S'

/* Set minimum interval between sensor reads (milliseconds) */
#define SMS_SET_INTERVAL _IOW(SMS_IOC_MAGIC, 1, __u32)

#define SMS_DEFAULT_INTERVAL_MS 100

#endif
