// SPDX-License-Identifier: GPL-2.0
/*
 * Sample kobject implementation
 *
 * Copyright (C) 2004-2007 Greg Kroah-Hartman <greg@kroah.com>
 * Copyright (C) 2007 Novell Inc.
 */

/**
 * 在/sys/kernel下面创建kobject-example目录。并且创建3个文件，
 * 分别是foo, baz bar , 如果向这些文件中写入数字，便可以读出来，
 * 这与我们平时操作sys文件系统来修改内核参数的行文很像，
 * 也就是提供了用户层配置内核层的接口
 */

#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/module.h>
#include <linux/init.h>

/*
 * This module shows how to create a simple subdirectory in sysfs called
 * /sys/kernel/kobject-example  In that directory, 3 files are created:
 * "foo", "baz", and "bar".  If an integer is written to these files, it can be
 * later read out of it.
 */
/*
 * 这个模块展示了如何在sysfs中创建一个名为/sys/kernel/kobject-example的简单子目录。
 * 在该目录中，创建了三个文件：“foo”、“baz”和“bar”。
 * 如果向这些文件写入一个整数，那么可以从中读出它。
 */

static int foo; // 定义静态整数变量foo
static int baz; // 定义静态整数变量baz
static int bar; // 定义静态整数变量bar

/*
 * The "foo" file where a static variable is read from and written to.
 */
/*
 * "foo"文件，其中一个静态变量可以被读取和写入。
 */
static ssize_t foo_show(struct kobject *kobj, struct kobj_attribute *attr,
			char *buf)
{
	// 将foo变量的值读取到buf缓冲区并返回，添加换行符
	return sysfs_emit(buf, "%d\n", foo);
}

static ssize_t foo_store(struct kobject *kobj, struct kobj_attribute *attr,
			 const char *buf, size_t count)
{
	int ret;

	// 将buf中的字符串转换为整数存储在foo变量中
	ret = kstrtoint(buf, 10, &foo);
	if (ret < 0)
		return ret;	// 如果转换失败，返回错误代码

	return count;	// 如果转换成功，返回写入的字节数
}

/* Sysfs attributes cannot be world-writable. */
/* Sysfs属性不能设置为全局可写。 */
// 创建名为foo的文件，权限为0664，关联foo_show和foo_store函数
/**
 * kobj_attribute描述一个kobject的属性，相当于继承了attribute，
 * show函数指针用用户读该属性文件的回调操作，需要填充buf缓冲区，
 * 将内容传递给用户，　store则对应于用户写该属性文件的回调操作，
 * 需要读取该buf缓冲区，了解用户进行何种要求
*/
static struct kobj_attribute foo_attribute =
	__ATTR(foo, 0664, foo_show, foo_store);

/*
 * More complex function where we determine which variable is being accessed by
 * looking at the attribute for the "baz" and "bar" files.
 */
/*
 * 更复杂的函数，我们通过查看属性来确定正在访问的变量，
 * 用于"baz"和"bar"文件。
 */
static ssize_t b_show(struct kobject *kobj, struct kobj_attribute *attr,
		      char *buf)
{
	int var;

	if (strcmp(attr->attr.name, "baz") == 0)
		var = baz;	// 如果属性名为baz，则读取baz变量
	else
		var = bar;	// 否则读取bar变量
	return sysfs_emit(buf, "%d\n", var);	// 将var的值输出到buf缓冲区
}

static ssize_t b_store(struct kobject *kobj, struct kobj_attribute *attr,
		       const char *buf, size_t count)
{
	int var, ret;

	// 将buf中的字符串转换为整数存储在var中
	ret = kstrtoint(buf, 10, &var);
	if (ret < 0)
		return ret;	// 如果转换失败，返回错误代码

	if (strcmp(attr->attr.name, "baz") == 0)
		baz = var;	// 如果属性名为baz，则更新baz变量
	else
		bar = var;		// 否则更新bar变量
	return count;		// 返回写入的字节数
}

/**
 * kobj_attribute描述一个kobject的属性，相当于继承了attribute，
 * show函数指针用用户读该属性文件的回调操作，需要填充buf缓冲区，
 * 将内容传递给用户，　store则对应于用户写该属性文件的回调操作，
 * 需要读取该buf缓冲区，了解用户进行何种要求
*/
// 创建名为baz的文件，权限为0664，关联b_show和b_store函数
static struct kobj_attribute baz_attribute =
	__ATTR(baz, 0664, b_show, b_store);
// 创建名为bar的文件，权限为0664，关联b_show和b_store函数
static struct kobj_attribute bar_attribute =
	__ATTR(bar, 0664, b_show, b_store);


/*
 * Create a group of attributes so that we can create and destroy them all
 * at once.
 */
/*
 * 创建属性组，以便我们可以同时创建和销毁所有属性。
 */
static struct attribute *attrs[] = {
	&foo_attribute.attr,
	&baz_attribute.attr,
	&bar_attribute.attr,
	NULL,	/* need to NULL terminate the list of attributes */
				/* 属性列表的末尾需要使用NULL来终止 */
};

/*
 * An unnamed attribute group will put all of the attributes directly in
 * the kobject directory.  If we specify a name, a subdirectory will be
 * created for the attributes with the directory being the name of the
 * attribute group.
 */
/*
 * 未命名的属性组会将所有属性直接放在kobject目录中。
 * 如果我们指定一个名称，一个子目录会被创建，目录名是属性组的名称。
 */
static struct attribute_group attr_group = {
	.attrs = attrs,	// 将attrs数组设置为属性组的属性
};

static struct kobject *example_kobj;	// 定义kobject对象的指针

static int __init example_init(void)
{
	int retval;

	/*
	 * Create a simple kobject with the name of "kobject_example",
	 * located under /sys/kernel/
	 *
	 * As this is a simple directory, no uevent will be sent to
	 * userspace.  That is why this function should not be used for
	 * any type of dynamic kobjects, where the name and number are
	 * not known ahead of time.
	 */
	/*
	 * 创建一个名为"kobject_example"的简单kobject，
	 * 位于/sys/kernel/下面。
	 *
	 * 由于这是一个简单的目录，不会向用户空间发送uevent。
	 * 因此这个函数不应该用于那些名称和数量未知的动态kobjects。
	 */
	example_kobj = kobject_create_and_add("kobject_example", kernel_kobj);
	if (!example_kobj)
		return -ENOMEM;	// 如果创建失败，返回内存不足的错误代码

	/* Create the files associated with this kobject */
	/* 创建与这个kobject关联的文件 */
	// 调用sysfs_create_group　创建目录下的属性组（keyobject对应文件夹，
	// 而属性就对应文件），一个属性组有多个属性
	retval = sysfs_create_group(example_kobj, &attr_group);
	if (retval)
		// 如果创建属性组失败，撤销kobject的创建并返回错误代码
		kobject_put(example_kobj);

	return retval;	// 返回结果
}

static void __exit example_exit(void)
{
	// 退出时释放kobject对象
	kobject_put(example_kobj);
}

module_init(example_init);
module_exit(example_exit);
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Greg Kroah-Hartman <greg@kroah.com>");
