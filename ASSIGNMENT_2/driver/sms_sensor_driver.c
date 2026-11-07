/*
 * File: sms_sensor_driver.c
 * Assignment 02 - Sensor Monitoring System
 * Target: Linux Kernel 6.6.21 / ARMv7
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/mutex.h>
#include <linux/random.h>
#include <linux/ktime.h>
#include <linux/delay.h>
#include <linux/slab.h>

#include "sms_sensor.h"
/* Character device */
static int major_number;
static struct class *sms_class;
static struct device *sms_device;

/* Procfs */
static struct proc_dir_entry *proc_entry;

/* Sensor statistics */
static u64 read_count;
static int last_temperature;
static int last_humidity;

/* Sampling interval */
static u32 interval_ms = SMS_DEFAULT_INTERVAL_MS;

/* Timing information */
static u64 start_time_ms;
static u64 last_read_ms;

/* Synchronization */
static DEFINE_MUTEX(sms_mutex);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Student");
MODULE_DESCRIPTION("Assignment 02 - Sensor Monitoring System Driver");
MODULE_VERSION("1.0");
/*
 * Open sensor device.
 */
static int sms_open(struct inode *inode, struct file *file)
{
    pr_info("sms_sensor: device opened\n");
    return 0;
}

/*
 * Release sensor device.
 */
static int sms_release(struct inode *inode, struct file *file)
{
    pr_info("sms_sensor: device closed\n");
    return 0;
}
/*
 * Read sensor data and return CSV format.
 */

static ssize_t sms_read(struct file *file,
                        char __user *buf,
                        size_t len,
                        loff_t *offset)
{
    char data[64];
    int data_len;
    u64 current_time;
    u32 random_value;
    int temperature;
    int humidity;

    (void)file;
    (void)offset;

    if (len < sizeof(data))
        return -EINVAL;

    mutex_lock(&sms_mutex);

    current_time = ktime_to_ms(ktime_get());

    if (read_count > 0 &&
        current_time - last_read_ms < interval_ms) {
        mutex_unlock(&sms_mutex);
        return -EAGAIN;
    }

    random_value = get_random_u32();
    temperature = 15 + (random_value % 31);

    random_value = get_random_u32();
    humidity = 30 + (random_value % 61);

    data_len = scnprintf(data, sizeof(data),
                         "%llu,%d,%d\n",
                         (unsigned long long)current_time,
                         temperature,
                         humidity);

    if (copy_to_user(buf, data, data_len)) {
        mutex_unlock(&sms_mutex);
        return -EFAULT;
    }

    last_temperature = temperature;
    last_humidity = humidity;
    last_read_ms = current_time;
    read_count++;

    mutex_unlock(&sms_mutex);

    return data_len;
}

/*
 * Configure sensor sampling interval through ioctl.
 */
static long sms_ioctl(struct file *file,
                      unsigned int cmd,
                      unsigned long arg)
{
    u32 new_interval;

    (void)file;

    switch (cmd) {
    case SMS_SET_INTERVAL:
        if (copy_from_user(&new_interval,
                           (u32 __user *)arg,
                           sizeof(new_interval)))
            return -EFAULT;

        if (new_interval == 0)
            return -EINVAL;

        mutex_lock(&sms_mutex);
        interval_ms = new_interval;
        mutex_unlock(&sms_mutex);

        pr_info("sms_sensor: interval updated to %u ms\n",
                new_interval);
        return 0;

    default:
        return -ENOTTY;
    }
}

/*
 * Display sensor statistics through /proc/sms_stats.
 */
static int sms_proc_show(struct seq_file *m, void *v)
{
    u64 current_time;
    u64 uptime;

    (void)v;

    mutex_lock(&sms_mutex);

    current_time = ktime_to_ms(ktime_get());
    uptime = div_u64(current_time - start_time_ms, 1000);

    seq_printf(m, "read_count: %llu\n",
               (unsigned long long)read_count);

    seq_printf(m, "last_temperature: %d\n",
               last_temperature);

    seq_printf(m, "last_humidity: %d\n",
               last_humidity);

    seq_printf(m, "uptime: %llu seconds\n",
               (unsigned long long)uptime);

    mutex_unlock(&sms_mutex);

    return 0;
}

static int sms_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, sms_proc_show, NULL);
}

static const struct proc_ops sms_proc_ops = {
    .proc_open    = sms_proc_open,
    .proc_read    = seq_read,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release,
};

/*
 * Character device file operations.
 */
static const struct file_operations sms_fops = {
    .owner          = THIS_MODULE,
    .open           = sms_open,
    .release        = sms_release,
    .read           = sms_read,
    .unlocked_ioctl = sms_ioctl,
    .llseek         = no_llseek,
};

/*
 * Initialize SMS sensor character driver and procfs.
 */
static int __init sms_init(void)
{
    int ret;

    read_count = 0;
    last_temperature = 0;
    last_humidity = 0;
    interval_ms = SMS_DEFAULT_INTERVAL_MS;

    start_time_ms = ktime_to_ms(ktime_get());
    last_read_ms = 0;

    /* Register character device with a dynamic major number. */
    major_number = register_chrdev(0, SMS_DEVICE_NAME, &sms_fops);
    if (major_number < 0) {
        pr_err("sms_sensor: failed to register character device\n");
        return major_number;
    }

    /* Create device class. */
    sms_class = class_create(SMS_DEVICE_NAME);
    if (IS_ERR(sms_class)) {
        ret = PTR_ERR(sms_class);
        pr_err("sms_sensor: failed to create device class\n");
        goto err_unregister;
    }

    /* Create /dev/sms_sensor. */
    sms_device = device_create(sms_class, NULL,
                               MKDEV(major_number, 0),
                               NULL, SMS_DEVICE_NAME);
    if (IS_ERR(sms_device)) {
        ret = PTR_ERR(sms_device);
        pr_err("sms_sensor: failed to create device\n");
        goto err_class;
    }

    /* Create /proc/sms_stats. */
    proc_entry = proc_create(SMS_PROC_NAME, 0444,
                             NULL, &sms_proc_ops);
    if (!proc_entry) {
        ret = -ENOMEM;
        pr_err("sms_sensor: failed to create proc entry\n");
        goto err_device;
    }

    pr_info("sms_sensor: driver initialized, major=%d\n",
            major_number);

    return 0;

err_device:
    device_destroy(sms_class, MKDEV(major_number, 0));

err_class:
    class_destroy(sms_class);

err_unregister:
    unregister_chrdev(major_number, SMS_DEVICE_NAME);
    return ret;
}

/*
 * Remove SMS sensor driver and release registered resources.
 */
static void __exit sms_exit(void)
{
    remove_proc_entry(SMS_PROC_NAME, NULL);

    device_destroy(sms_class, MKDEV(major_number, 0));
    class_destroy(sms_class);

    unregister_chrdev(major_number, SMS_DEVICE_NAME);

    pr_info("sms_sensor: driver removed\n");
}

module_init(sms_init);
module_exit(sms_exit);
