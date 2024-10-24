// interrupt_trigger_module.c

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

extern void trigger_virtual_irq(void);

static int __init trigger_module_init(void)
{
    printk(KERN_INFO "Trigger module loaded, triggering virtual IRQ.\n");
    trigger_virtual_irq(); // 调用第一个模块中定义的函数来触发中断
    return 0;
}

static void __exit trigger_module_exit(void)
{
    printk(KERN_INFO "Trigger module unloaded.\n");
}

module_init(trigger_module_init);
module_exit(trigger_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("A module that triggers a virtual IRQ.");
