// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2017 Joe Lawrence <joe.lawrence@redhat.com>
 */

/*
 * livepatch-callbacks-mod.c - (un)patching callbacks demo support module
 *
 *
 * Purpose
 * -------
 *
 * Simple module to demonstrate livepatch (un)patching callbacks.
 *
 *
 * Usage
 * -----
 *
 * This module is not intended to be standalone.  See the "Usage"
 * section of livepatch-callbacks-demo.c.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/kernel.h>

static int livepatch_callbacks_mod_init(void)
{
	printk("livepatch_callbacks_mod_init1");
	pr_info("%s\n", __func__);
	printk("livepatch_callbacks_mod_init2");
	return 0;
}

static void livepatch_callbacks_mod_exit(void)
{
	printk("livepatch_callbacks_mod_exit1");
	pr_info("%s\n", __func__);
	printk("livepatch_callbacks_mod_exit2");
}

module_init(livepatch_callbacks_mod_init);
module_exit(livepatch_callbacks_mod_exit);
MODULE_LICENSE("GPL");
