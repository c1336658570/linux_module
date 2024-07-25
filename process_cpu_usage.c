#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/sched/signal.h>
#include <linux/slab.h>
#include <linux/hashtable.h>
#include <linux/delay.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("Module to track CPU usage over 100ms intervals.");

// 定义数据结构
struct proc_info {
	pid_t pid;
	unsigned long long cpu_time;
	char comm[TASK_COMM_LEN];  // 存储进程名，TASK_COMM_LEN 是进程名的长度，定义在 include/linux/sched.h
	struct hlist_node hnode;
};

// 定义哈希表
DEFINE_HASHTABLE(hash_table_a, 10);	// 2^10 buckets
DEFINE_HASHTABLE(hash_table_b, 10);

// 交换哈希表并计算结果
static void process_data(void)
{
	struct proc_info *pos_a, *pos_b;
	struct hlist_node *tmp;
	unsigned bkt;
	hash_for_each_safe(hash_table_a, bkt, tmp, pos_a, hnode) {
		bool found = false;
		hash_for_each_possible(hash_table_b, pos_b, hnode, pos_a->pid) {
			if (pos_a->pid == pos_b->pid) {
				printk(KERN_INFO "PID: %u (%s), CPU Time Delta: %llu\n",
                       pos_a->pid, pos_a->comm, pos_b->cpu_time - pos_a->cpu_time);
				found = true;
				break;
			}
		}
		if (!found) {
			printk(KERN_INFO
			       "PID: %u, No data found in second sample\n",
			       pos_a->pid);
		}
		hash_del(&pos_a->hnode);
		kfree(pos_a);
	}
	// Move data from hash_table_b to hash_table_a
	hash_for_each_safe(hash_table_b, bkt, tmp, pos_b, hnode) {
		hash_del(&pos_b->hnode);
		hash_add(hash_table_a, &pos_b->hnode, pos_b->pid);
	}
}

// 采集进程的CPU时间并存储到哈希表中
static int collect_cpu_usage(void)
{
	struct task_struct *task;
	struct proc_info *info;
	struct proc_info *pos_a, *pos_b;
	struct hlist_node *tmp;
	unsigned bkt;
	// Move data from hash_table_b to hash_table_a
	hash_for_each_safe(hash_table_b, bkt, tmp, pos_b, hnode) {
		hash_del(&pos_b->hnode);
		hash_add(hash_table_a, &pos_b->hnode, pos_b->pid);
	}
	rcu_read_lock();
	for_each_process(task) {
		info = kmalloc(sizeof(*info), GFP_ATOMIC);
		if (!info)
			continue;
		info->pid = task->pid;
		info->cpu_time = task->utime + task->stime;	// 用户空间和内核空间的CPU时间
		get_task_comm(info->comm, task);  // 获取进程名
		hash_add(hash_table_b, &info->hnode, info->pid);
	}
	rcu_read_unlock();
	return 0;
}

// 初始化函数：首次采集数据
static int __init cpu_usage_init(void)
{
	hash_init(hash_table_a);
	hash_init(hash_table_b);
	collect_cpu_usage();
	msleep(1000);
	collect_cpu_usage();
	process_data();
	return 0;
}

// 清理函数：清空哈希表
static void __exit cpu_usage_exit(void)
{
	struct proc_info *info;
	struct hlist_node *tmp;
	unsigned bkt;
	hash_for_each_safe(hash_table_a, bkt, tmp, info, hnode) {
		hash_del(&info->hnode);
		kfree(info);
	}
	printk(KERN_INFO "CPU usage module unloaded.\n");
}

module_init(cpu_usage_init);
module_exit(cpu_usage_exit);
