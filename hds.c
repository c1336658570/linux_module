// make
// sudo lsmod
// sudo insmod hds.ko
// sudo dmesg
// sudo rmmod hds.ko
// sudo dmesg -C

// 为方便在当前终端查看日志打印信息，在装载模块前输入此命令
// tail -f /var/log/kern.log &

#include <linux/init.h>       // 必须包含，包含了宏__init(指定初始化函数)和__exit(指定清除函数)
#include <linux/kernel.h>     // 里面包含常用的内核API，例如内核打印函数printk()
#include <linux/module.h>     // 任何模块都必须包含，定义了可动态加载到内核的模块所需要的必要信息

static int __init hds_init(void){   // __init将函数hds_init()标记为初始化函数，在模块被装载到内核时调用hds_init()
  int sum = 0;
  int i;
  for (i = 1; i < 11; i++){       // 函数功能为1-10累加求和
    sum += i;
  }
  printk(KERN_CRIT "Hello kernel\n");  // 注意末尾不要忘记加换行\n,否则打印会出现某些小的错误
  printk(KERN_ALERT "sum is %d\n", sum);
  // 打印级别设为<1>,将求和结果立即打印,可以在插入模块后，在用户态下用命令dmesg查看打印效果
  return 0;
}

static void __exit hds_exit(void) {   // 清除函数,在模块被卸载之前调用
  printk(KERN_ALERT "Goodbye kernel\n");  // 在模块卸载时，将Goodbye kernel这句话打印到日志
}

module_init(hds_init);  // 引导内核 模块从这里进来
module_exit(hds_exit);  // 引导内核 模块从这里出去
MODULE_LICENSE("GPL");  //(必选项) 模块许可证，缺省此句，将导致内核被污染
MODULE_AUTHOR("hds");           //(可选项) 描述模块作者
MODULE_DESCRIPTION("for fun");  //(可选项) 描述模块功能