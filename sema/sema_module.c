// 内核信号量测试
// ls /sys/class        查看所有的设备类
// ls /sys/class/tty		对于特定的设备类（例如 tty），你可以查看该类下的设备

// cat /proc/devices    查看设备号

// lsmod								查看已加载的内核模块

// 加载一个内核模块
// sudo insmod [模块文件名]
// sudo insmod my_module.ko

// sudo rmmod [模块名]					卸载一个内核模块
// sudo rmmod my_module

// 查看模块信息和设置模块参数
// modinfo [模块名]
// modinfo my_module

// sudo insmod my_module.ko param1=value1 param2=value2		在加载模块时设置参数


#include <linux/init.h>
#include <linux/module.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/semaphore.h>

static int major = 262; // 主设备号
static int minor = 0;		// 次设备号
static dev_t devno;			// 设备号

static struct cdev cdev; // 字符设备结构体

static struct class *cls;					 // 类
static struct device *test_device; // 设备

static struct semaphore sem; // 信号量，用于同步访问

// 打开设备文件
static int hello_open(struct inode *inode, struct file *filep)
{
	// 获取信号量，若失败则可能是由于进程被信号中断
	if (down_interruptible(&sem))
	{
		return -ERESTARTSYS; // 返回错误码
	}
	return 0; // 成功打开设备
}

// 关闭设备文件
static int hello_release(struct inode *inode, struct file *filep)
{
	up(&sem); // 释放信号量
	return 0; // 关闭成功
}

// 文件操作结构体
static struct file_operations hello_ops =
		{
				.open = hello_open,			 // 打开设备文件操作
				.release = hello_release // 关闭设备文件操作
};

// 模块初始化函数
static int hello_init(void)
{
	int result;
	printk("hello_init \n");

	// 注册字符设备
	result = register_chrdev(major, "hello", &hello_ops);
	if (result < 0)
	{
		printk("register_chrdev fail \n");
		return result;
	}

	// 获取设备号
	devno = MKDEV(major, minor);

	// 创建设备类
	cls = class_create("helloclass");
	// cls = class_create(THIS_MODULE, "helloclass");
	if (IS_ERR(cls))
	{
		unregister_chrdev(major, "hello");
		return PTR_ERR(cls);
	}

	// 创建设备
	test_device = device_create(cls, NULL, devno, NULL, "test");
	if (IS_ERR(test_device))
	{
		class_destroy(cls);
		unregister_chrdev(major, "hello");
		return PTR_ERR(test_device);
	}

	// 初始化信号量
	sema_init(&sem, 1);

	return 0;
}

// 模块退出函数
static void hello_exit(void)
{
	printk("hello_exit \n");

	// 销毁设备
	device_destroy(cls, devno);

	// 销毁类
	class_destroy(cls);

	// 注销字符设备
	unregister_chrdev(major, "hello");
}

// 指定模块的初始化和退出函数
module_init(hello_init);
module_exit(hello_exit);

// 模块的许可证和作者信息
MODULE_LICENSE("GPL");
MODULE_AUTHOR("ccc");
