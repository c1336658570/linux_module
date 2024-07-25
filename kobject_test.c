#include <linux/module.h> // 包含创建模块所需的函数和宏定义
#include <linux/kernel.h> // 包含内核使用的常见宏和函数
#include <linux/kobject.h> // 提供kobject结构和相关函数
#include <linux/sysfs.h> // 提供sysfs接口的函数
#include <linux/init.h> // 提供模块初始化和退出宏
#include <linux/file.h> // 文件操作相关的结构和函数（此处可能未使用）
#include <linux/fs.h> // 文件系统相关的结构和函数

static struct kobject kobj; // 定义一个kobject结构体
static struct kobject* pkobj = NULL; // 定义一个指向kobject的指针

static void test_kobj_release(struct kobject *kobj)
{
	printk(KERN_EMERG "kobject: test_kobj_release!\n"); // 在释放kobject时打印信息
}

static ssize_t test_attr_show(struct kobject *kobj, struct attribute *attr, char *page)
{
	printk(KERN_EMERG "kobject: test_attr_show!\n"); // 读取属性时打印信息
	return 0; // 读取函数需要返回读取的字节数，这里简化处理，返回0
}

static ssize_t test_attr_store(struct kobject *kobj, struct attribute *attr, const char *page, size_t length)
{
	printk(KERN_EMERG "kobject: test_attr_shore!\n"); // 写入属性时打印信息
	return 123; // 写入函数应返回写入的字节数，这里简化处理，返回123
}

static const struct sysfs_ops test_sysfs_ops = {
	.show  = test_attr_show, // 关联显示（读取）函数
	.store = test_attr_store, // 关联存储（写入）函数
};

static struct kobj_type test_ktype = {
	.sysfs_ops = &test_sysfs_ops, // 设置sysfs操作函数
	.release = test_kobj_release, // 设置释放函数
};

static struct attribute test_attr ={
	.name = "test", // 属性的名称
	.mode = S_IWUSR | S_IRUGO, // 属性的权限，用户可写和全局可读
};

static int kobject_test_init(void)
{
	int ret = 0; // 定义返回值变量，初始为0

	printk(KERN_EMERG "kobject: kobject_test_init!\n"); // 打印初始化信息
	pkobj = kobject_create_and_add("123_test", NULL); // 在/sys/下创建一个名为123_test的kobject
	ret = kobject_init_and_add(&kobj, &test_ktype, pkobj, "%s", "456_test"); // 初始化并添加一个名为456_test的子kobject

	ret = sysfs_create_file(&kobj, &test_attr); // 在456_test目录下创建一个名为test的属性文件
	ret = sysfs_create_file(pkobj, &test_attr); // 在456_test目录下创建一个名为test的属性文件
	return ret; // 返回操作结果
}

static void kobject_test_exit(void)
{
	printk(KERN_EMERG "kobject: kobject_test_exit!\n"); // 打印退出信息
	sysfs_remove_file(&kobj, &test_attr); // 移除test属性文件
	kobject_del(&kobj); // 删除kobject
	kobject_del(pkobj); // 删除父kobject
}

module_init(kobject_test_init); // 指定模块初始化函数
module_exit(kobject_test_exit); // 指定模块退出函数
MODULE_LICENSE("GPL v2"); // 指定模块的许可证
