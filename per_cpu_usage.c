#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/timer.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/workqueue.h>
#include <linux/slab.h>

#define CPU_INFO_BUFSIZE 4096

static struct timer_list cpu_timer;
static struct work_struct cpu_work;
static unsigned long *prev_idle, *prev_total;
static int num_cpus;

static void calculate_cpu_usage(struct work_struct *work);

static void cpu_timer_callback(struct timer_list *t)
{
  // 安排工作队列
  schedule_work(&cpu_work);
  // 重新设置定时器
  mod_timer(&cpu_timer, jiffies + msecs_to_jiffies(1000));
}

static void calculate_cpu_usage(struct work_struct *work)
{
  char *buf;
  struct file *f;
  unsigned long user, nice, system, idle, iowait, irq, softirq, steal,guest, guest_nice;
  long usage;
  int cpu;
  loff_t pos = 0;

  buf = kmalloc(CPU_INFO_BUFSIZE, GFP_KERNEL);
  if (!buf)
  {
    printk(KERN_ERR "Failed to allocate memory\n");
    return;
  }

  f = filp_open("/proc/stat", O_RDONLY, 0);
  if (IS_ERR(f))
  {
    printk(KERN_ERR "Cannot open /proc/stat, error %ld\n", PTR_ERR(f));
    return;
  }

  if (kernel_read(f, buf, CPU_INFO_BUFSIZE, &pos) > 0)
  {
    char *line = buf;
    while (*line != '\n' && *line != '\0') line++;
    if (*line == '\n') line++;
    for (cpu = 0; cpu < num_cpus; cpu++)
    {
      unsigned long total, diff_idle, diff_total;

      if (sscanf(line, "cpu%*d %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu", &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal, &guest, &guest_nice) == 10)
        {
        total = user + nice + system + idle;
        diff_idle = idle - prev_idle[cpu];
        diff_total = total - prev_total[cpu];

        prev_idle[cpu] = idle;
        prev_total[cpu] = total;

        if (diff_total != 0)
        {
          usage = 100 * (diff_total - diff_idle) / diff_total;
          printk(KERN_INFO "CPU%d Usage: %ld%%\n", cpu, usage);
        }
        else
        {
          printk(KERN_INFO "CPU%d No CPU usage update\n", cpu);
        }

        // 移动到下一行
        while (*line != '\n' && *line != '\0')
          line++;
        if (*line == '\n')
          line++;
      }
    }
  }
  else
  {
    printk(KERN_ERR "Failed to read from /proc/stat\n");
  }

  filp_close(f, NULL);
}

static int __init cpu_module_init(void)
{
  num_cpus = num_possible_cpus();
  prev_idle = kzalloc(sizeof(unsigned long) * num_cpus, GFP_KERNEL);
  prev_total = kzalloc(sizeof(unsigned long) * num_cpus, GFP_KERNEL);
  if (!prev_idle || !prev_total) {
    printk(KERN_ERR "Failed to allocate memory\n");
    return 0;
  }

  INIT_WORK(&cpu_work, calculate_cpu_usage);
  timer_setup(&cpu_timer, cpu_timer_callback, 0);
  mod_timer(&cpu_timer, jiffies + msecs_to_jiffies(1000));
  printk(KERN_INFO "CPU usage module loaded for %d CPUs.\n", num_cpus);
  return 0;
}

static void __exit cpu_module_exit(void)
{
  del_timer(&cpu_timer);
  flush_work(&cpu_work); // 确保所有工作都已完成
  kfree(prev_idle);
  kfree(prev_total);
  printk(KERN_INFO "CPU usage module unloaded.\n");
}

module_init(cpu_module_init);
module_exit(cpu_module_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Module to estimate CPU usage for each CPU");
MODULE_AUTHOR("Your Name");
