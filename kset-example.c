// SPDX-License-Identifier: GPL-2.0
/*
 * Sample kset and ktype implementation
 *
 * Copyright (C) 2004-2007 Greg Kroah-Hartman <greg@kroah.com>
 * Copyright (C) 2007 Novell Inc.
 */
#include <linux/kobject.h>  // 包含kobject结构和相关函数的头文件
#include <linux/string.h>   // 包含字符串操作函数的头文件
#include <linux/sysfs.h>    // 包含创建sysfs文件和目录的函数
#include <linux/slab.h>     // 包含内存分配函数的头文件
#include <linux/module.h>   // 包含创建模块所需的函数和宏定义
#include <linux/init.h>     // 包含模块初始化和退出宏

/*
 * This module shows how to create a kset in sysfs called
 * /sys/kernel/kset-example
 * Then tree kobjects are created and assigned to this kset, "foo", "baz",
 * and "bar".  In those kobjects, attributes of the same name are also
 * created and if an integer is written to these files, it can be later
 * read out of it.
 */


/*
 * This is our "object" that we will create a few of and register them with
 * sysfs.
 */
// 声明foo_obj结构体，包含一个kobject和三个整型变量
struct foo_obj {
	struct kobject kobj;  // 内嵌的kobject结构体
	int foo;              // 对应"foo"属性的整型变量
	int baz;              // 对应"baz"属性的整型变量
	int bar;              // 对应"bar"属性的整型变量
};
// 容器宏，用于从kobject获取包含它的foo_obj结构体的指针
#define to_foo_obj(x) container_of(x, struct foo_obj, kobj)

/* a custom attribute that works just for a struct foo_obj. */
// 自定义属性结构体，包括一个attribute和两个操作函数指针
struct foo_attribute {
	struct attribute attr;
	ssize_t (*show)(struct foo_obj *foo, struct foo_attribute *attr, char *buf);
	ssize_t (*store)(struct foo_obj *foo, struct foo_attribute *attr, const char *buf, size_t count);
};
// 容器宏，用于从attribute获取包含它的foo_attribute结构体的指针
#define to_foo_attr(x) container_of(x, struct foo_attribute, attr)

/*
 * The default show function that must be passed to sysfs.  This will be
 * called by sysfs for whenever a show function is called by the user on a
 * sysfs file associated with the kobjects we have registered.  We need to
 * transpose back from a "default" kobject to our custom struct foo_obj and
 * then call the show function for that specific object.
 */
// 默认的show函数，用于从sysfs读取属性时调用
static ssize_t foo_attr_show(struct kobject *kobj,
			     struct attribute *attr,
			     char *buf)
{
	struct foo_attribute *attribute;
	struct foo_obj *foo;

	attribute = to_foo_attr(attr);	// 从attribute获取foo_attribute
	foo = to_foo_obj(kobj);		// 从kobject获取foo_obj

	if (!attribute->show)
		return -EIO;	// 如果没有定义show函数，返回错误

	// 调用特定的show函数
	return attribute->show(foo, attribute, buf);
}

/*
 * Just like the default show function above, but this one is for when the
 * sysfs "store" is requested (when a value is written to a file.)
 */
// 默认的store函数，用于向sysfs写入属性时调用
static ssize_t foo_attr_store(struct kobject *kobj,
			      struct attribute *attr,
			      const char *buf, size_t len)
{
	struct foo_attribute *attribute;
	struct foo_obj *foo;

	attribute = to_foo_attr(attr);	// 从attribute获取foo_attribute
	foo = to_foo_obj(kobj);		// 从kobject获取foo_obj

	if (!attribute->store)
		return -EIO;		// 如果没有定义store函数，返回错误

	// 调用特定的store函数
	return attribute->store(foo, attribute, buf, len);
}

/* Our custom sysfs_ops that we will associate with our ktype later on */
// 自定义的sysfs操作结构体
static const struct sysfs_ops foo_sysfs_ops = {
	.show = foo_attr_show,		// 设置show函数
	.store = foo_attr_store,	// 设置store函数
};

/*
 * The release function for our object.  This is REQUIRED by the kernel to
 * have.  We free the memory held in our object here.
 *
 * NEVER try to get away with just a "blank" release function to try to be
 * smarter than the kernel.  Turns out, no one ever is...
 */
// foo_obj对象的释放函数
static void foo_release(struct kobject *kobj)
{
	struct foo_obj *foo;

	foo = to_foo_obj(kobj);	// 从kobject获取foo_obj
	kfree(foo);							// 释放foo_obj结构体占用的内存
}

/*
 * The "foo" file where the .foo variable is read from and written to.
 */
// foo属性的读函数
static ssize_t foo_show(struct foo_obj *foo_obj, struct foo_attribute *attr,
			char *buf)
{
	// 输出foo变量的值
	return sysfs_emit(buf, "%d\n", foo_obj->foo);
}

// foo属性的写函数
static ssize_t foo_store(struct foo_obj *foo_obj, struct foo_attribute *attr,
			 const char *buf, size_t count)
{
	int ret;

	// 将字符串转换为整数并存储到foo变量中
	ret = kstrtoint(buf, 10, &foo_obj->foo);
	if (ret < 0)
		return ret;	// 如果转换失败，返回错误代码

	return count;	// 如果成功，返回写入的字节数
}

/* Sysfs attributes cannot be world-writable. */
// foo属性，不可全局可写
// 创建名为foo的文件，权限为0664
static struct foo_attribute foo_attribute =
	__ATTR(foo, 0664, foo_show, foo_store);

/*
 * More complex function where we determine which variable is being accessed by
 * looking at the attribute for the "baz" and "bar" files.
 */
// 读取和写入baz或bar属性的函数
static ssize_t b_show(struct foo_obj *foo_obj, struct foo_attribute *attr,
		      char *buf)
{
	int var;

	if (strcmp(attr->attr.name, "baz") == 0)
		var = foo_obj->baz;	// 如果属性名为baz，则读取baz变量
	else
		var = foo_obj->bar;	// 如果属性名为bar，则读取bar变量
	return sysfs_emit(buf, "%d\n", var);	// 输出变量的值
}

static ssize_t b_store(struct foo_obj *foo_obj, struct foo_attribute *attr,
		       const char *buf, size_t count)
{
	int var, ret;

	ret = kstrtoint(buf, 10, &var);	// 将字符串转换为整数
	if (ret < 0)
		return ret;	// 如果转换失败，返回错误代码

	if (strcmp(attr->attr.name, "baz") == 0)
		foo_obj->baz = var;	// 如果属性名为baz，则更新baz变量
	else
		foo_obj->bar = var;	// 如果属性名为bar，则更新bar变量
	return count;		// 返回写入的字节数
}

// baz和bar属性
 // 创建名为baz的文件，权限为0664
static struct foo_attribute baz_attribute =
	__ATTR(baz, 0664, b_show, b_store);
// 创建名为bar的文件，权限为0664
static struct foo_attribute bar_attribute =
	__ATTR(bar, 0664, b_show, b_store);

/*
 * Create a group of attributes so that we can create and destroy them all
 * at once.
 */
// 创建一个属性组，如果某个kobject包含该属性组，就会在那个kobject
// 目录下产生三个属性文件
static struct attribute *foo_default_attrs[] = {
	&foo_attribute.attr,
	&baz_attribute.attr,
	&bar_attribute.attr,
	NULL,	/* need to NULL terminate the list of attributes */
				// 结尾需要NULL
};
ATTRIBUTE_GROUPS(foo_default);

/*
 * Our own ktype for our kobjects.  Here we specify our sysfs ops, the
 * release function, and the set of default attributes we want created
 * whenever a kobject of this type is registered with the kernel.
 */
// 定义foo_obj的ktype
static struct kobj_type foo_ktype = {
	.sysfs_ops = &foo_sysfs_ops,	// 设置sysfs操作
	.release = foo_release,				// 设置释放函数
	.default_groups = foo_default_groups,	// 设置默认属性组
};

// 声明一个kset变量
static struct kset *example_kset;
static struct foo_obj *foo_obj;
static struct foo_obj *bar_obj;
static struct foo_obj *baz_obj;

// 创建foo_obj的函数
static struct foo_obj *create_foo_obj(const char *name)
{
	struct foo_obj *foo;
	int retval;

	/* allocate the memory for the whole object */
	// 为整个对象分配内存
	foo = kzalloc(sizeof(*foo), GFP_KERNEL);
	if (!foo)
		return NULL;

	/*
	 * As we have a kset for this kobject, we need to set it before calling
	 * the kobject core.
	 */
	// 设置kset
	foo->kobj.kset = example_kset;

	/*
	 * Initialize and add the kobject to the kernel.  All the default files
	 * will be created here.  As we have already specified a kset for this
	 * kobject, we don't have to set a parent for the kobject, the kobject
	 * will be placed beneath that kset automatically.
	 */
	// 初始化并添加kobject到内核
	retval = kobject_init_and_add(&foo->kobj, &foo_ktype, NULL, "%s", name);
	if (retval) {
		kobject_put(&foo->kobj);
		return NULL;
	}

	/*
	 * We are always responsible for sending the uevent that the kobject
	 * was added to the system.
	 */
	// 负责发送kobject添加到系统的uevent
	kobject_uevent(&foo->kobj, KOBJ_ADD);

	return foo;
}

// 销毁foo_obj的函数
static void destroy_foo_obj(struct foo_obj *foo)
{
	kobject_put(&foo->kobj);
}

// 初始化函数
static int __init example_init(void)
{
	// 创建的example_kset并没有属性，所以在/sys/kenrel/example_kset
	// 下并没有属性文件，create_foo_obj是在example_kset下创建
	// kobject，然后该对象有属性，所以/sys/kernel/example_kset/foo/
	// 或/bar或/baz下有属性文件foo，bar，zar

	/*
	 * Create a kset with the name of "kset_example",
	 * located under /sys/kernel/
	 */
	// 在/sys/kernel/下创建一个名为"kset_example"的kset
	example_kset = kset_create_and_add("kset_example", NULL, kernel_kobj);
	if (!example_kset)
		return -ENOMEM;

	/*
	 * Create three objects and register them with our kset
	 */
	// 创建三个对象并注册到我们的kset
	foo_obj = create_foo_obj("foo");
	if (!foo_obj)
		goto foo_error;

	bar_obj = create_foo_obj("bar");
	if (!bar_obj)
		goto bar_error;

	baz_obj = create_foo_obj("baz");
	if (!baz_obj)
		goto baz_error;

	return 0;

baz_error:
	destroy_foo_obj(bar_obj);
bar_error:
	destroy_foo_obj(foo_obj);
foo_error:
	kset_unregister(example_kset);
	return -EINVAL;
}

// 退出函数
static void __exit example_exit(void)
{
	destroy_foo_obj(baz_obj);
	destroy_foo_obj(bar_obj);
	destroy_foo_obj(foo_obj);
	kset_unregister(example_kset);
}

module_init(example_init);
module_exit(example_exit);
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Greg Kroah-Hartman <greg@kroah.com>");
