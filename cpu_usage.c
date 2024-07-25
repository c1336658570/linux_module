#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/timer.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/workqueue.h>
#include <linux/sched.h>

static struct timer_list cpu_timer;
static struct work_struct cpu_work;

static void calculate_cpu_usage(struct work_struct *work);

static void cpu_timer_callback(struct timer_list *t) {
    // 安排工作队列
    schedule_work(&cpu_work);
    // 重新设置定时器
    mod_timer(&cpu_timer, jiffies + msecs_to_jiffies(1000));
}

static void calculate_cpu_usage(struct work_struct *work) {
    char buf[256];
    struct file *f;
    unsigned long user, nice, system, idle, total, diff_idle, diff_total;
    static unsigned long prev_idle = 0, prev_total = 0;
    long usage;
    loff_t pos = 0;

    // 打开 /proc/stat 文件读取CPU时间
    f = filp_open("/proc/stat", O_RDONLY, 0);
    if (IS_ERR(f)) {
        printk(KERN_ERR "Cannot open /proc/stat, error %ld\n", PTR_ERR(f));
        return;
    }

    // 读取数据
    if (kernel_read(f, buf, sizeof(buf), &pos) > 0) {
        sscanf(buf, "cpu %lu %lu %lu %lu", &user, &nice, &system, &idle);

        total = user + nice + system + idle;
        diff_idle = idle - prev_idle;
        diff_total = total - prev_total;

        prev_idle = idle;
        prev_total = total;

        if (diff_total != 0) {
            usage = 100 * (diff_total - diff_idle) / diff_total;
            printk(KERN_INFO "CPU Usage: %ld%%\n", usage);
        } else {
            printk(KERN_INFO "No CPU usage update\n");
        }
    } else {
        printk(KERN_ERR "Failed to read from /proc/stat\n");
    }

    filp_close(f, NULL);
}

static int __init cpu_module_init(void) {
    INIT_WORK(&cpu_work, calculate_cpu_usage);
    timer_setup(&cpu_timer, cpu_timer_callback, 0);
    mod_timer(&cpu_timer, jiffies + msecs_to_jiffies(1000));
    printk(KERN_INFO "CPU usage module loaded.\n");
    return 0;
}

static void __exit cpu_module_exit(void) {
    del_timer(&cpu_timer);
    flush_work(&cpu_work); // 确保所有工作都已完成
    printk(KERN_INFO "CPU usage module unloaded.\n");
}

module_init(cpu_module_init);
module_exit(cpu_module_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Module to estimate CPU usage");
MODULE_AUTHOR("Your Name");
