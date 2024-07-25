#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/swap.h>
#include <linux/vmstat.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("A simple Linux driver to report memory and swap usage.");

static int __init meminfo_init(void)
{
	struct sysinfo si;
	long cached;

	si_meminfo(&si);
	// cached = global_node_page_state(NR_FILE_PAGES) - total_swapcache_pages() - si.bufferram;
	printk(KERN_INFO "Loaded meminfo module.\n");
	printk(KERN_INFO "Total RAM: %lu MB\n",
	       si.totalram * si.mem_unit / 1024 / 1024);
	printk(KERN_INFO "Free RAM: %lu MB\n",
	       si.freeram * si.mem_unit / 1024 / 1024);
	printk(KERN_INFO "Buffer RAM: %lu MB\n",
	       si.bufferram * si.mem_unit / 1024 / 1024);

	// 获取交换空间信息
//     si_swapinfo(&si);
//     printk(KERN_INFO "Total Swap: %lu MB\n", si.totalswap * si.mem_unit / 1024 / 1024);
//     printk(KERN_INFO "Free Swap: %lu MB\n", si.freeswap * si.mem_unit / 1024 / 1024);

	return 0;
}

static void __exit meminfo_exit(void)
{
	printk(KERN_INFO "Unloaded meminfo module.\n");
}

module_init(meminfo_init);
module_exit(meminfo_exit);
