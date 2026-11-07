/*
 * asgn1_driver.c
 * Assignment 01 - Character Device Driver with IOCTL
 * Target: Linux 6.6.21, ARMv7, QEMU vexpress-a9
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/errno.h>

#include "asgn1_ioctl.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Embedded Linux Assignment 01");
MODULE_DESCRIPTION("Character Driver with IOCTL");
MODULE_VERSION("1.0");

#define DEVICE_NAME "asgn1"
#define CLASS_NAME  "asgn1_class"
#define DRIVER_MAJOR 241
#define BUFFER_SIZE 1024
#define VERSION_SIZE 64

#define MODE_KERNEL 0
#define MODE_ECHO   1

static int major_number = DRIVER_MAJOR;
static struct class *asgn1_class;
static struct device *asgn1_device;

static char device_buffer[BUFFER_SIZE];
static size_t buffer_len;

static unsigned int open_count;
static unsigned int write_count;
static unsigned int read_count;
static unsigned int last_write_size;

static int driver_mode = MODE_ECHO;

static DEFINE_MUTEX(asgn1_mutex);

/*
 * asgn1_open - Handle device open.
 * @inode: Device inode.
 * @file: File structure.
 * Return: 0 on success.
 */
static int asgn1_open(struct inode *inode, struct file *file)
{
    mutex_lock(&asgn1_mutex);
    open_count++;
    mutex_unlock(&asgn1_mutex);

    pr_info("asgn1: device opened\n");
    return 0;
}

/*
 * asgn1_release - Handle device close.
 * @inode: Device inode.
 * @file: File structure.
 * Return: 0 on success.
 */
static int asgn1_release(struct inode *inode, struct file *file)
{
    pr_info("asgn1: device closed\n");
    return 0;
}

/*
 * asgn1_read - Copy stored data to userspace.
 * @file: File structure.
 * @buf: Userspace destination buffer.
 * @len: Requested number of bytes.
 * @offset: Current file position.
 * Return: Number of bytes read, 0 at EOF, or negative error.
 */
static ssize_t asgn1_read(struct file *file, char __user *buf,
                          size_t len, loff_t *offset)
{
    size_t bytes_to_read;

    if (len == 0)
        return 0;

    mutex_lock(&asgn1_mutex);

    if (*offset >= buffer_len) {
        mutex_unlock(&asgn1_mutex);
        return 0;
    }

    bytes_to_read = min(len, buffer_len - (size_t)*offset);

    if (copy_to_user(buf, device_buffer + *offset, bytes_to_read)) {
        mutex_unlock(&asgn1_mutex);
        return -EFAULT;
    }

    *offset += bytes_to_read;
    read_count++;

    mutex_unlock(&asgn1_mutex);

    pr_info("asgn1: read %zu bytes\n", bytes_to_read);
    return bytes_to_read;
}

/*
 * asgn1_write - Store userspace data in the device buffer.
 * @file: File structure.
 * @buf: Userspace source buffer.
 * @len: Number of bytes to write.
 * @offset: Current file position.
 * Return: Number of accepted input bytes or negative error.
 */
static ssize_t asgn1_write(struct file *file, const char __user *buf,
                           size_t len, loff_t *offset)
{
    char *temp;
    size_t input_len;
    size_t prefix_len = 0;
    size_t payload_len;
    int mode;

    if (len == 0)
        return 0;

    input_len = min(len, (size_t)(BUFFER_SIZE - 1));

    /* Allocate temporary memory outside the kernel stack. */
    temp = kmalloc(BUFFER_SIZE, GFP_KERNEL);
    if (!temp)
        return -ENOMEM;

    if (copy_from_user(temp, buf, input_len)) {
        kfree(temp);
        return -EFAULT;
    }

    mutex_lock(&asgn1_mutex);

    mode = driver_mode;

    if (mode == MODE_KERNEL) {
        prefix_len = strlen("[KERNEL]");
        payload_len = min(input_len,
                          (size_t)(BUFFER_SIZE - 1 - prefix_len));

        memcpy(device_buffer, "[KERNEL]", prefix_len);
        memcpy(device_buffer + prefix_len, temp, payload_len);
    } else {
        payload_len = input_len;
        memcpy(device_buffer, temp, payload_len);
    }

    buffer_len = prefix_len + payload_len;
    device_buffer[buffer_len] = '\0';

    write_count++;
    last_write_size = input_len;
    *offset = 0;

    mutex_unlock(&asgn1_mutex);

    kfree(temp);

    pr_info("asgn1: accepted %zu input bytes, mode=%d\n",
            input_len, mode);

    return input_len;
}

/*
 * asgn1_ioctl - Process device control commands.
 * @file: File structure.
 * @cmd: IOCTL command.
 * @arg: Userspace argument address.
 * Return: 0 on success or negative error.
 */
static long asgn1_ioctl(struct file *file,
                        unsigned int cmd,
                        unsigned long arg)
{
    int mode;
    struct asgn1_stats stats;
    char version[VERSION_SIZE] = {0};

    pr_info("asgn1: ioctl command 0x%x\n", cmd);

    switch (cmd) {
    case ASGN1_RESET_BUFFER:
        mutex_lock(&asgn1_mutex);

        memset(device_buffer, 0, sizeof(device_buffer));
        buffer_len = 0;
        write_count = 0;
        read_count = 0;
        last_write_size = 0;

        mutex_unlock(&asgn1_mutex);

        pr_info("asgn1: buffer and read/write counters reset\n");
        return 0;

    case ASGN1_GET_STATS:
        mutex_lock(&asgn1_mutex);

        stats.open_count = open_count;
        stats.write_count = write_count;
        stats.read_count = read_count;
        stats.buffer_len = buffer_len;
        stats.last_write_size = last_write_size;

        mutex_unlock(&asgn1_mutex);

        if (copy_to_user((void __user *)arg, &stats, sizeof(stats)))
            return -EFAULT;

        pr_info("asgn1: statistics returned to userspace\n");
        return 0;

    case ASGN1_SET_MODE:
        if (get_user(mode, (int __user *)arg))
            return -EFAULT;

        if (mode != MODE_KERNEL && mode != MODE_ECHO)
            return -EINVAL;

        mutex_lock(&asgn1_mutex);
        driver_mode = mode;
        mutex_unlock(&asgn1_mutex);

        pr_info("asgn1: mode changed to %d\n", mode);
        return 0;

    case ASGN1_GET_VERSION:
        strscpy(version, "asgn1_driver v1.0 - SE204664",
                sizeof(version));

        if (copy_to_user((void __user *)arg,
                         version, sizeof(version)))
            return -EFAULT;

        pr_info("asgn1: version returned to userspace\n");
        return 0;

    default:
        pr_warn("asgn1: unsupported ioctl command 0x%x\n", cmd);
        return -ENOTTY;
    }
}

static const struct file_operations asgn1_fops = {
    .owner          = THIS_MODULE,
    .open            = asgn1_open,
    .release         = asgn1_release,
    .read             = asgn1_read,
    .write            = asgn1_write,
    .unlocked_ioctl   = asgn1_ioctl,
};

/*
 * asgn1_init - Register character device and create /dev/asgn1.
 * Return: 0 on success or negative error.
 */
static int __init asgn1_init(void)
{
    int ret;

    memset(device_buffer, 0, sizeof(device_buffer));
    buffer_len = 0;
    open_count = 0;
    write_count = 0;
    read_count = 0;
    last_write_size = 0;
    driver_mode = MODE_ECHO;

    ret = register_chrdev(major_number, DEVICE_NAME, &asgn1_fops);
    if (ret < 0) {
        pr_err("asgn1: register_chrdev failed: %d\n", ret);
        return ret;
    }

    asgn1_class = class_create(CLASS_NAME);
    if (IS_ERR(asgn1_class)) {
        ret = PTR_ERR(asgn1_class);
        unregister_chrdev(major_number, DEVICE_NAME);
        return ret;
    }

    asgn1_device = device_create(asgn1_class, NULL,
                                 MKDEV(major_number, 0),
                                 NULL, DEVICE_NAME);
    if (IS_ERR(asgn1_device)) {
        ret = PTR_ERR(asgn1_device);
        class_destroy(asgn1_class);
        unregister_chrdev(major_number, DEVICE_NAME);
        return ret;
    }

    pr_info("asgn1: driver loaded, major=%d, default mode=ECHO\n",
            major_number);

    return 0;
}

/*
 * asgn1_exit - Remove device and unregister character driver.
 * Return: None.
 */
static void __exit asgn1_exit(void)
{
    device_destroy(asgn1_class, MKDEV(major_number, 0));
    class_destroy(asgn1_class);
    unregister_chrdev(major_number, DEVICE_NAME);

    pr_info("asgn1: driver unloaded\n");
}

module_init(asgn1_init);
module_exit(asgn1_exit);
