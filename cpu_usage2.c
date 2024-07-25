#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/timer.h>
#include <linux/sched.h>
#include <linux/sched/stat.h>
#include <linux/kernel_stat.h>

static struct timer_list cpu_timer;
static u64 prev_idle = 0;
static u64 prev_total = 0;

// 获取CPU的空闲时间和总时间
void get_cpu_times(u64 *idle, u64 *total)
{
  int i;
  u64 user, nice, system, idle_time, iowait, irq, softirq, steal;
  user = nice = system = idle_time = iowait = irq = softirq = steal = 0;

  for_each_possible_cpu(i)
  {
    user += kcpustat_cpu(i).cpustat[CPUTIME_USER];
    nice += kcpustat_cpu(i).cpustat[CPUTIME_NICE];
    system += kcpustat_cpu(i).cpustat[CPUTIME_SYSTEM];
    idle_time += kcpustat_cpu(i).cpustat[CPUTIME_IDLE];
    iowait += kcpustat_cpu(i).cpustat[CPUTIME_IOWAIT];
    irq += kcpustat_cpu(i).cpustat[CPUTIME_IRQ];
    softirq += kcpustat_cpu(i).cpustat[CPUTIME_SOFTIRQ];
    steal += kcpustat_cpu(i).cpustat[CPUTIME_STEAL];
  }

  *idle = idle_time;
  *total = user + nice + system + idle_time + iowait + irq + softirq + steal;
}

void get_cpu_usage(void)
{
  u64 idle, total;
  u64 delta_idle, delta_total;
  long usage;

  get_cpu_times(&idle, &total);

  delta_idle = idle - prev_idle;
  delta_total = total - prev_total;

  prev_idle = idle;
  prev_total = total;

  if (delta_total != 0)
  {
    usage = 100 * (delta_total - delta_idle) / delta_total;
    printk(KERN_INFO "CPU Usage: %ld%%\n", usage);
  }
}

void timer_callback(struct timer_list *t)
{
  get_cpu_usage();
  mod_timer(&cpu_timer, jiffies + msecs_to_jiffies(1000));
}

static int __init cpu_usage_init(void)
{
  printk(KERN_INFO "CPU usage module init.\n");
  timer_setup(&cpu_timer, timer_callback, 0);
  mod_timer(&cpu_timer, jiffies + msecs_to_jiffies(1000));
  return 0;
}

static void __exit cpu_usage_exit(void)
{
  del_timer(&cpu_timer);
  printk(KERN_INFO "CPU usage module exit.\n");
}

module_init(cpu_usage_init);
module_exit(cpu_usage_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("A simple Linux driver to monitor CPU usage.");
