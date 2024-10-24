// interrupt_register_module.c

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>

#define VIRTUAL_IRQ 13 // 选择一个不与其他设备冲突的IRQ号

// 中断处理函数，仅打印信息
static irqreturn_t irq_handler(int irq, void *dev_id)
{
    printk(KERN_INFO "Virtual IRQ %d handler executed.\n", irq);
    return IRQ_HANDLED;
}

// 提供一个接口用来触发中断
void trigger_virtual_irq(void)
{
    // 假设硬件中断号为33已经注册
    // 这里我们模拟硬件中断的触发
    // 请注意，这不是标准的做法，且在实际硬件中断编程中通常不这样处理
    printk(KERN_INFO "Simulating IRQ %d trigger.\n", VIRTUAL_IRQ);
    asm volatile ("int $0x21" : : "a"(VIRTUAL_IRQ)); // 使用汇编语言触发中断
}

EXPORT_SYMBOL(trigger_virtual_irq);

static int __init irq_module_init(void)
{
    int result;
    result = request_irq(VIRTUAL_IRQ, irq_handler, 0, "A_New_Device", NULL);

    if (result) {
        printk(KERN_ERR "Failed to register virtual IRQ %d %d\n", VIRTUAL_IRQ, result);
        return -EIO;
    }
    printk(KERN_INFO "Virtual IRQ %d registered\n", VIRTUAL_IRQ);
    return 0;
}

static void __exit irq_module_exit(void)
{
    free_irq(VIRTUAL_IRQ, (void *)(irq_handler));
    printk(KERN_INFO "Virtual IRQ %d unregistered\n", VIRTUAL_IRQ);
}

module_init(irq_module_init);
module_exit(irq_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("A module that registers a virtual IRQ and provides a trigger function.");
