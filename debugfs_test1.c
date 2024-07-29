#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>

#define BUF_SIZE 100

static struct dentry *dir, *file;
static char buf[BUF_SIZE];
static ssize_t file_read(struct file *filp, char __user *user_buf, size_t count, loff_t *ppos)
{
    return simple_read_from_buffer(user_buf, count, ppos, buf, strlen(buf));
}

static ssize_t file_write(struct file *filp, const char __user *user_buf, size_t count, loff_t *ppos)
{
    if (count > BUF_SIZE - 1)
        return -EINVAL;

    if (copy_from_user(buf, user_buf, count))
        return -EFAULT;

    buf[count] = '\0';
    return count;
}

static const struct file_operations fops = {
    .owner = THIS_MODULE,
    .read = file_read,
    .write = file_write,
};

static int __init mymodule_init(void)
{
    dir = debugfs_create_dir("mymodule", NULL);
    if (!dir)
        return -ENOMEM;

    file = debugfs_create_file("myfile", 0666, dir, NULL, &fops);
    if (!file) {
        debugfs_remove(dir);
        return -ENOMEM;
    }

    return 0;
}

static void __exit mymodule_exit(void)
{
    debugfs_remove_recursive(dir);
}

module_init(mymodule_init);
module_exit(mymodule_exit);
MODULE_LICENSE("GPL");
