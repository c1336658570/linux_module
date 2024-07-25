#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>

#define DEVICE_NAME "mychardev"
#define CLASS_NAME "mychardev_class"

static int major;
static struct cdev my_cdev;
static struct class *my_class;
static struct device *my_device;

#define KMAX_LEN 32

static int my_open(struct inode *ind, struct file *fp)
{
	printk("demo open\n");
	return 0;
}

static int my_release(struct inode *ind, struct file *fp)
{
	printk("demo release\n");
	return 0;
}

static ssize_t my_read(struct file *fp, char __user * buf, size_t size,
			 loff_t * pos)
{
	int rc;
	char kbuf[KMAX_LEN] = "read test\n";
	if (size > KMAX_LEN)
		size = KMAX_LEN;

	rc = copy_to_user(buf, kbuf, size);
	if (rc < 0) {
		return -EFAULT;
		pr_err("copy_to_user failed!");
	}
	return size;
}

static ssize_t my_write(struct file *fp, const char __user * buf, size_t size,
			  loff_t * pos)
{
	int rc;
	char kbuf[KMAX_LEN];
	if (size > KMAX_LEN)
		size = KMAX_LEN;

	rc = copy_from_user(kbuf, buf, size);
	if (rc < 0) {
		return -EFAULT;
		pr_err("copy_from_user failed!");
	}
	printk("%s", kbuf);
	return size;
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .read = my_read,
    .write = my_write,
    .open = my_open,
    .release = my_release,
};

static int __init my_init(void) {
    dev_t dev;
    int ret;

    // int register_chrdev_region(dev_t from, unsigned count, const char *name)
    // 用来静态分配一系列字符设备号给字符设备。 from 要分配的这一系列设备号的第一个设备号
    // count 分配的设备号的个数     name 设备的名字
    // 设备分配成功时返回0，分配失败时，例如要分配的设备号已经被占用了，则返回一个负值的错误码。

    // int alloc_chrdev_region(dev_t *dev, unsigned baseminor, unsigned count, const char *name)
    // 动态分配设备号，系统会自动从未分配出去的设备号中选择一个分配给当前设备，而不再需要程序员自己手动找一个未分配的设备号。
    // dev	将分配的一系列设备号的第一个的值通过该参数返回出来
    // baseminor 要分配的这一系列设备号的第一个次设备号 count 分配的设备号的个数    name 设备的名字
    // 设备分配成功时返回0，分配失败时返回一个负值的错误码。

    // 分配设备号
    ret = alloc_chrdev_region(&dev, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        printk(KERN_ALERT "Failed to allocate major number\n");
        return ret;
    }

    major = MAJOR(dev);

    // 初始化并添加 cdev
    cdev_init(&my_cdev, &fops);
    ret = cdev_add(&my_cdev, dev, 1);
    if (ret < 0) {
        unregister_chrdev_region(dev, 1);
        printk(KERN_ALERT "Failed to add cdev\n");
        return ret;
    }

    // 创建 class
    my_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(my_class)) {
        cdev_del(&my_cdev);
        unregister_chrdev_region(dev, 1);
        printk(KERN_ALERT "Failed to create class\n");
        return PTR_ERR(my_class);
    }

    // 创建 device
    my_device = device_create(my_class, NULL, dev, NULL, DEVICE_NAME);
    if (IS_ERR(my_device)) {
        class_destroy(my_class);
        cdev_del(&my_cdev);
        unregister_chrdev_region(dev, 1);
        printk(KERN_ALERT "Failed to create device\n");
        return PTR_ERR(my_device);
    }

    printk(KERN_INFO "Device registered with major number %d\n", major);
    return 0;
}

static void __exit my_exit(void) {
    device_destroy(my_class, MKDEV(major, 0));
    class_unregister(my_class);
    class_destroy(my_class);
    cdev_del(&my_cdev);
    unregister_chrdev_region(MKDEV(major, 0), 1);
    printk(KERN_INFO "Device unregistered\n");
}

module_init(my_init);
module_exit(my_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Author");
MODULE_DESCRIPTION("A simple character device driver with device and cdev association");
