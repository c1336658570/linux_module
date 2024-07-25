#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/sched/task.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");

static int __init list_runqueue_init(void)
{
    int cpu;
    struct task_struct *g, *p;

    for_each_possible_cpu(cpu) {
        printk(KERN_INFO "CPU: %d\n", cpu);
        rcu_read_lock();  // 使用RCU锁以安全方式遍历任务
        for_each_process_thread(g, p) {
            // 这将迭代系统中的每个进程和它们的线程
            if (task_cpu(p) == cpu) { // 检查是否在当前CPU上运行
                printk(KERN_INFO "  %s [%d]\n", p->comm, task_pid_nr(p));
            }
        }
        rcu_read_unlock();
    }

    return 0;
}

static void __exit list_runqueue_exit(void)
{
    printk(KERN_INFO "Removing module\n");
}

module_init(list_runqueue_init);
module_exit(list_runqueue_exit);
