#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/atomic.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/string.h>
#include <linux/errno.h>

#define DEVICE_NAME "counter"
#define PROC_NAME "counter_info"
#define MAX_INPUT_SIZE 32

/* Driver statistics */
static atomic_t counter_value = ATOMIC_INIT(0);
static atomic_t total_reads = ATOMIC_INIT(0);
static atomic_t total_writes = ATOMIC_INIT(0);

/*
 * Read the current counter value.
 * The counter increases by one after each successful read.
 */
static ssize_t counter_read(struct file *file,
                            char __user *user_buf,
                            size_t count,
                            loff_t *ppos)
{
    char buffer[32];
    int value;
    int len;

    if (*ppos > 0)
        return 0;

    value = atomic_read(&counter_value);
    len = scnprintf(buffer, sizeof(buffer), "%d\n", value);

    if (count < len)
        return -EINVAL;

    if (copy_to_user(user_buf, buffer, len))
        return -EFAULT;

    *ppos += len;
    atomic_inc(&counter_value);
    atomic_inc(&total_reads);

    return len;
}

/*
 * Write a non-negative integer to the counter.
 * Writing 0 resets the counter.
 */
static ssize_t counter_write(struct file *file,
                             const char __user *user_buf,
                             size_t count,
                             loff_t *ppos)
{
    char buffer[MAX_INPUT_SIZE];
    int value;
    size_t len;

    if (count == 0 || count >= sizeof(buffer))
        return -EINVAL;

    len = count;

    if (copy_from_user(buffer, user_buf, len))
        return -EFAULT;

    buffer[len] = '\0';
    strim(buffer);

    if (kstrtoint(buffer, 10, &value) != 0)
        return -EINVAL;

    if (value < 0)
        return -EINVAL;

    atomic_set(&counter_value, value);
    atomic_inc(&total_writes);

    pr_info("counter: value set to %d\n", value);

    return count;
}

/*
 * Display counter statistics through /proc/counter_info.
 */
static int counter_proc_show(struct seq_file *m, void *v)
{
    seq_printf(m, "Current counter: %d\n",
               atomic_read(&counter_value));

    seq_printf(m, "Total reads: %d\n",
               atomic_read(&total_reads));

    seq_printf(m, "Total writes/resets: %d\n",
               atomic_read(&total_writes));

    return 0;
}

/*
 * Open the proc statistics file.
 */
static int counter_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, counter_proc_show, NULL);
}

/* Device file operations */
static const struct file_operations counter_fops = {
    .owner = THIS_MODULE,
    .read = counter_read,
    .write = counter_write,
    .llseek = default_llseek,
};

/* Proc file operations */
static const struct proc_ops counter_proc_ops = {
    .proc_open = counter_proc_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};

/* Misc device definition */
static struct miscdevice counter_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = DEVICE_NAME,
    .fops = &counter_fops,
    .mode = 0666,
};

/*
 * Register the misc device and create the proc statistics file.
 */
static int __init counter_init(void)
{
    int ret;

    atomic_set(&counter_value, 0);
    atomic_set(&total_reads, 0);
    atomic_set(&total_writes, 0);

    ret = misc_register(&counter_device);
    if (ret) {
        pr_err("counter: failed to register misc device\n");
        return ret;
    }

    if (!proc_create(PROC_NAME, 0444, NULL, &counter_proc_ops)) {
        misc_deregister(&counter_device);
        pr_err("counter: failed to create proc entry\n");
        return -ENOMEM;
    }

    pr_info("counter: driver loaded successfully\n");
    return 0;
}

/*
 * Remove the proc entry and deregister the misc device.
 */
static void __exit counter_exit(void)
{
    remove_proc_entry(PROC_NAME, NULL);
    misc_deregister(&counter_device);

    pr_info("counter: driver unloaded\n");
}

module_init(counter_init);
module_exit(counter_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Embedded Linux Assignment");
MODULE_DESCRIPTION("Misc device counter driver with procfs statistics");
MODULE_VERSION("1.0");
