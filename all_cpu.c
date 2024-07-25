#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/smp.h>
#include <linux/sched/signal.h>
#include <linux/timer.h>
#include <linux/delay.h>  // 如果需要使用udelay或mdelay

void get_current_task_info(void *info)
{
  unsigned int cpu;
  struct task_struct *task;

  cpu = get_cpu();  // 获取当前CPU的编号，并禁止抢占
  task = current;   // 获取当前CPU的任务

  printk(KERN_INFO "CPU: %u, Current Task PID: %d, Name: %s\n", smp_processor_id(), task->pid, task->comm);
  
  put_cpu();  // 重新允许抢占
}

static int __init my_module_init(void)
{
  printk(KERN_INFO "Module to get current tasks on all CPUs.\n");

  while (1) {
    // 调用所有CPU执行函数，NULL表示没有传递给函数的参数
    smp_call_function(get_current_task_info, NULL, 1);

    // 也在当前CPU上执行一次，以确保当前CPU也被覆盖
    get_current_task_info(NULL);
    // 如果你需要使用udelay或mdelay，请在这里加上相应的函数调用
    // udelay(1000); // 微秒延迟
    mdelay(1000); // 毫秒延迟

    // msleep(1000);
  }

  return 0; // 非0值将不加载模块
}

static void __exit my_module_exit(void)
{
  printk(KERN_INFO "Cleaning up module.\n");
}

module_init(my_module_init);
module_exit(my_module_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Module to print the current task on all CPUs");
