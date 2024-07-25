#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/tick.h>

static struct task_struct *task;

int cpu_usage(void *data)
{
  u64 prev_idle_time = 0, idle_time = 0;
  u64 usage;

  while (!kthread_should_stop())
  {
    idle_time = get_cpu_idle_time_us(0, NULL); // Assuming single-core CPU
    if (prev_idle_time)
    {
      usage = 100 - ((idle_time - prev_idle_time) * 100 / 1000000); // Assuming 1-second interval
      printk(KERN_INFO "CPU Usage: %llu%%\n", usage);
    }
    prev_idle_time = idle_time;
    msleep(1000); // 1-second interval
  }
  return 0;
}

static int __init cpu_load_init(void)
{
  printk(KERN_INFO "Starting CPU Usage Monitor Module\n");
  task = kthread_run(cpu_usage, NULL, "cpu_usage_monitor");
  return 0;
}

static void __exit cpu_load_exit(void)
{
  printk(KERN_INFO "Stopping CPU Usage Monitor Module\n");
  kthread_stop(task);
}

module_init(cpu_load_init);
module_exit(cpu_load_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("A simple Linux kernel module to monitor CPU usage");