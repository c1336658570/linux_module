// make
// sudo lsmod
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
  printk("kernel_module_init\n");

  struct task_struct *task;

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
