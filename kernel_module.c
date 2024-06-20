// make
// sudo lsmod   # 查看已经安�?�的内核模块
// cat /proc/modules  # 功能和上面命令相�?
// ls /sys/module     # 功能和上面相�?
// sudo insmod kernel_module.ko
// sudo dmesg
// sudo rmmod kernel_module.ko
// sudo dmesg -C

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/pid.h>

static int kernel_module_init(void) {
  struct task_struct *task;

  printk("kernel_module_init\n");

  for_each_process(task) {
    printk("name: %s, pid: %d", task->comm, task->pid);
  }

  return 0;
}

static void kernel_module_exit(void) {
  printk("kernel_module_exit\n");

}

module_init(kernel_module_init);
module_exit(kernel_module_exit);

MODULE_LICENSE("GPL");
