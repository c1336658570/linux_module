// sudo lsmod   # 查看已经安装的内核模块
// cat /proc/modules  # 功能和上面命令相同
// ls /sys/module     # 功能和上面相同
#include <linux/module.h>
#include <linux/init.h>

static int __init hello_init(void) {
  printk(KERN_INFO "HELLO LINUX MODULE\n");
  
  return 0;
}

static void __exit hello_exit(void) {
  printk(KERN_INFO "GOODBYE LINUX\n");
}

module_init(hello_init);
module_exit(hello_exit);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("CCC");   // 作者
MODULE_VERSION("V1.0"); // 版本
