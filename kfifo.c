// make
// sudo lsmod   # 查看已经安装的内核模块
// cat /proc/modules  # 功能和上面命令相同
// ls /sys/module     # 功能和上面相同
// sudo insmod hds.ko
// sudo dmesg
// sudo rmmod hds.ko
// sudo dmesg -C

// 为方便在当前终端查看日志打印信息，在装载模块前输入此命令
// tail -f /var/log/kern.log &


// kfifo的使用

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/kfifo.h>

MODULE_LICENSE("GPL");

struct node {
  int val;
};

static int __init testkfifo_init(void) {
  int a[7] = {3, 56, 34, 79, 28, 1, 7};
  struct kfifo q;
  int ret, i;
  struct node buf[10];

  ret = kfifo_alloc(&q, sizeof(struct node) * 7, GFP_KERNEL);
  if (ret) {
    printk("kfifo_alloc fail ret=%d\n", ret);
    return 0;
  }

  for (i = 0; i < 7; ++i) {
    buf[0].val = a[i];
    ret = kfifo_in(&q, buf, sizeof(struct node));
    printk("in_ret = %d Bytes\n", ret);
    if (!ret) printk("kfifo_put fail, fifo is full\n");
  }

  printk("size = %d\n", kfifo_size(&q));
  printk("len = %d\n", kfifo_len(&q));
  printk("avail = %d\n", kfifo_avail(&q));

  ret = kfifo_out_peek(&q, buf, sizeof(struct node) * 7);
  printk("peek_ret = %d Bytes\n", ret);
  for (i = 0; i < ret / sizeof(struct node); ++i)
    printk("peek: %d\n", buf[i].val);

  while (!kfifo_is_empty(&q)) {
    ret = kfifo_out(&q, buf, sizeof(struct node) * 2);
    printk("out_ret = %d Bytes\n", ret);
    for (i = 0; i < ret / sizeof(struct node); ++i)
      printk("out: %d\n", buf[i].val);
  }

  kfifo_free(&q);
  return 0;
}

static void __exit testkfifo_exit(void) { printk("exit\n"); }

module_init(testkfifo_init);
module_exit(testkfifo_exit);