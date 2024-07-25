#include <linux/module.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/sched/signal.h>
#include <linux/pid.h>

static int __init list_tasks_init(void)
{
  struct task_struct *task;

  printk(KERN_INFO "Listing current tasks:\n");
  for_each_process(task) {
    // 获取进程CPU使用时间
    unsigned long utime = task->utime; // 用户态时间
    unsigned long stime = task->stime; // 系统态时间

    // 打印进程信息
    printk(KERN_INFO "PID: %d | Name: %s | CPU: %d | Utime: %lu | Stime: %lu\n",
           task->pid, task->comm, task_cpu(task), utime, stime);
  }

  return 0;
}

static void __exit list_tasks_exit(void)
{
  printk(KERN_INFO "Unloading module.\n");
}

module_init(list_tasks_init);
module_exit(list_tasks_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("A simple Linux module to list tasks");
