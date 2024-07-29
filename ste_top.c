#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/init.h>
#include <linux/smp.h>
#include <linux/cpumask.h>
#include <linux/cpu.h>
#include <linux/delay.h>
#include <linux/kernel_stat.h>
#include <linux/slab.h>
#include <linux/hashtable.h>
#include <linux/timekeeping.h>
#include <linux/time64.h>
#include <linux/version.h>
#include <linux/printk.h>
#include <linux/mm.h>
#include <linux/sched/prio.h>
#include <linux/types.h>
#include <linux/sort.h>
#include <linux/sched/debug.h>
#include <linux/swap.h>
#include <linux/vmstat.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/smp.h>
#include <linux/swap.h>
#include <linux/compiler.h>
#include <linux/swapfile.h>
#include <linux/sched/cputime.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,6,0)
#include <linux/time_namespace.h>
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(5,6,0)
#define PROC_OPS_USE_FILE_OPERATIONS
#endif

MODULE_LICENSE("GPL");

#ifndef NSEC_PER_SEC
#define NSEC_PER_SEC	1000000000L
#endif

DEFINE_HASHTABLE(hash_table_old, 10);	// 2^10 buckets
DEFINE_HASHTABLE(hash_table_new, 10);

struct hst {
	pid_t pid;
	u64 cpu_time;
	char comm[TASK_COMM_LEN];
	struct hlist_node hnode;
};

struct proc_info {
	pid_t pid;
	uid_t uid;
	int priority;
	int nice;
	u64 per_cpu_time;
	unsigned long resident;
	unsigned long virtual_mem;
	unsigned long shared_mem;
	long state;
	char state_c;
	int exit_state;
	u64 utime;
	u64 stime;
	u64 cutime;
	u64 cstime;
	char comm[TASK_COMM_LEN];
	int num_threads;
};

struct all_cpu_time {
	unsigned long user;
	unsigned long system;
	unsigned long nice;
	unsigned long idle;
	unsigned long iowait;
	unsigned long irq;
	unsigned long softirq;
	unsigned long steal;
};

static struct proc_info **process_info;
static unsigned total, running, sleeping, stopped, zombie, d_process;
static unsigned long kb_mem_total;
static int n_alloc;
static time64_t et;
static struct all_cpu_time old_all_cpu_time;
static struct all_cpu_time new_all_cpu_time;

static void update_cpu_times(void)
{
	int cpu;
	struct kernel_cpustat kstat;

	old_all_cpu_time = new_all_cpu_time;
	memset(&new_all_cpu_time, 0, sizeof(new_all_cpu_time));
	for_each_online_cpu(cpu) {
		kstat = kcpustat_cpu(cpu);
		new_all_cpu_time.user += kstat.cpustat[CPUTIME_USER];
		new_all_cpu_time.system += kstat.cpustat[CPUTIME_SYSTEM];
		new_all_cpu_time.nice += kstat.cpustat[CPUTIME_NICE];
		new_all_cpu_time.idle += kstat.cpustat[CPUTIME_IDLE];
		new_all_cpu_time.iowait += kstat.cpustat[CPUTIME_IOWAIT];
		new_all_cpu_time.irq += kstat.cpustat[CPUTIME_IRQ];
		new_all_cpu_time.softirq += kstat.cpustat[CPUTIME_SOFTIRQ];
		new_all_cpu_time.steal += kstat.cpustat[CPUTIME_STEAL];
	}
}

static void print_cpu_usage_statistics(void)
{
	unsigned long diff_user, diff_system, diff_nice, diff_idle, diff_iowait,
	    diff_irq, diff_softirq, diff_steal;
	unsigned long total_diff, total_active_usage;

	diff_user = new_all_cpu_time.user - old_all_cpu_time.user;
	diff_system = new_all_cpu_time.system - old_all_cpu_time.system;
	diff_nice = new_all_cpu_time.nice - old_all_cpu_time.nice;
	diff_idle = new_all_cpu_time.idle - old_all_cpu_time.idle;
	diff_iowait = new_all_cpu_time.iowait - old_all_cpu_time.iowait;
	diff_irq = new_all_cpu_time.irq - old_all_cpu_time.irq;
	diff_softirq = new_all_cpu_time.softirq - old_all_cpu_time.softirq;
	diff_steal = new_all_cpu_time.steal - old_all_cpu_time.steal;

	total_diff =
	    diff_user + diff_system + diff_nice + diff_idle + diff_iowait +
	    diff_irq + diff_softirq + diff_steal;

	if (total_diff != 0) {
		// Calculate the percentages for each category
		unsigned long user_pct = (1000 * diff_user) / total_diff;
		unsigned long system_pct = (1000 * diff_system) / total_diff;
		unsigned long nice_pct = (1000 * diff_nice) / total_diff;
		unsigned long iowait_pct = (1000 * diff_iowait) / total_diff;
		unsigned long irq_pct = (1000 * diff_irq) / total_diff;
		unsigned long softirq_pct = (1000 * diff_softirq) / total_diff;
		unsigned long steal_pct = (1000 * diff_steal) / total_diff;
		unsigned long idle_pct;

		total_active_usage =
		    user_pct + system_pct + nice_pct + iowait_pct + irq_pct +
		    softirq_pct + steal_pct;
		idle_pct = 1000 - total_active_usage;	// Ensuring total sums to 100%

		pr_info
		    ("ste_top: CPU(s): %lu.%lu us, %lu.%lu sy, %lu.%lu ni, %lu.%lu id, %lu.%lu wa, %lu.%lu hi, %lu.%lu si, %lu.%lu st\n",
		     user_pct / 10, user_pct % 10, system_pct / 10,
		     system_pct % 10, nice_pct / 10, nice_pct % 10,
		     idle_pct / 10, idle_pct % 10, iowait_pct / 10,
		     iowait_pct % 10, irq_pct / 10, irq_pct % 10,
		     softirq_pct / 10, softirq_pct % 10, steal_pct / 10,
		     steal_pct % 10);
	}
}

static void ste_mem_usage(void)
{
	struct sysinfo si;
	long cached, sreclaimable;

	si_meminfo(&si);
	cached =
	    global_node_page_state(NR_FILE_PAGES) - total_swapcache_pages() -
	    si.bufferram;
	sreclaimable = global_node_page_state(NR_SLAB_RECLAIMABLE);
	kb_mem_total = si.totalram * si.mem_unit / 1024;
	pr_info
	    ("ste_top: MiB Mem: total: %lu MB, free: %lu MB, buffer/cache: %lu MB\n",
	     si.totalram * si.mem_unit / 1024 / 1024,
	     si.freeram * si.mem_unit / 1024 / 1024,
	     (si.bufferram + cached +
	      sreclaimable) * si.mem_unit / 1024 / 1024);

	si_swapinfo(&si);
	pr_info("ste_top: MiB Swap: total: %lu MB, free: %lu MB\n",
		si.totalswap * si.mem_unit / 1024 / 1024,
		si.freeswap * si.mem_unit / 1024 / 1024);
}

static void print_swap_info(void)
{
	struct swap_info_struct *sis;
	int type;

	pr_info
	    ("ste_top: %-40.40s %-15s %-12s %-12s %-6s\n", "Filename", "Type", "Size", "Used", "Priority");

	for (type = 0; type < MAX_SWAPFILES; type++) {
		smp_rmb();
		sis = READ_ONCE(swap_info[type]);
		if (!sis)
			continue;

		if (!(sis->flags & SWP_USED) || !sis->swap_map)
			continue;

		pr_info("ste_top: %-40.40s %-15s %-12u %-12u %-6d\n",
			sis->swap_file->f_path.dentry->d_name.name,
			(S_ISBLK(file_inode(sis->swap_file)->i_mode) ?
			 "partition" : "file"), sis->pages << (PAGE_SHIFT - 10),
			sis->inuse_pages << (PAGE_SHIFT - 10), sis->prio);
	}

}

static void show(void)
{
	int i;
	pr_info
	    ("ste_top: Tasks: %d total, %d running, %d sleeping, %d stopped, %d zombie, %d D\n",
	     total, running, sleeping, stopped, zombie, d_process);
	print_cpu_usage_statistics();
	ste_mem_usage();
	print_swap_info();
	pr_info
	    ("  PID   UID    PR   NI     VIRT     RES     SHR S    %%CPU    %%MEM   COMMAND\n");
	for (i = 0; i < total; ++i) {
		pr_info("%5d %5u %5d %4d %8lu %7lu %7lu %c %5llu.%1llu %5ld.%1ld   %s\n", 
			process_info[i]->pid, process_info[i]->uid, process_info[i]->priority, process_info[i]->nice, 
			process_info[i]->virtual_mem, process_info[i]->resident, process_info[i]->shared_mem, process_info[i]->state_c, 
			process_info[i]->per_cpu_time * 1000 / et / 10 >= 100 * process_info[i]->num_threads ? 100 * process_info[i]->num_threads : process_info[i]->per_cpu_time * 1000 / et / 10, 
			process_info[i]->per_cpu_time * 1000 / et / 10 >= 100 * process_info[i]->num_threads ? 0 : process_info[i]->per_cpu_time * 1000 / et % 10,	// CPU percentage
			process_info[i]->resident * 1000 / kb_mem_total / 10, process_info[i]->resident * 1000 / kb_mem_total % 10,	// MEM percentage
			process_info[i]->comm);
	}

}

static void procs_hlp(struct proc_info *this)
{
	struct hst *h;
	struct hst *item;
	u64 per_cpu_time;

	if (this == NULL) {
		static struct timespec64 uptime_sav;
		struct timespec64 uptime_cur;
		struct hst *pos;
		struct hlist_node *tmp;
		unsigned bkt;

		ktime_get_boottime_ts64(&uptime_cur);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,6,0)
		timens_add_boottime(&uptime_cur);
#endif

		et = (uptime_cur.tv_sec - uptime_sav.tv_sec) * NSEC_PER_SEC +
		    (uptime_cur.tv_nsec - uptime_sav.tv_nsec);
		if (et < NSEC_PER_SEC / 100)
			et = NSEC_PER_SEC / 100;
		uptime_sav = uptime_cur;

		total = running = sleeping = stopped = zombie = d_process = 0;

		hash_for_each_safe(hash_table_old, bkt, tmp, pos, hnode) {
			hash_del(&pos->hnode);
			kfree(pos);
		}

		// Move data from hash_table_new to hash_table_old
		hash_for_each_safe(hash_table_new, bkt, tmp, pos, hnode) {
			hash_del(&pos->hnode);
			hash_add(hash_table_old, &pos->hnode, pos->pid);
		}
		return;
	}
	total++;
	if (this->state == TASK_RUNNING) {
		running++;
	} else if (this->state_c == 'D') {
		d_process++;
	} else if (this->state & (TASK_INTERRUPTIBLE | TASK_UNINTERRUPTIBLE)) {
		sleeping++;
	} else if (this->exit_state == EXIT_ZOMBIE) {
		zombie++;
	} else if (this->state & (TASK_STOPPED | TASK_TRACED)) {
		stopped++;
	}

	h = kmalloc(sizeof(*h), GFP_KERNEL);
	if (!h)
		return;
	h->pid = this->pid;
	h->cpu_time = per_cpu_time = this->utime + this->stime;
	strcpy(h->comm, this->comm);
	hash_add(hash_table_new, &h->hnode, h->pid);

	hash_for_each_possible(hash_table_old, item, hnode, h->pid) {
		if (item->pid == h->pid && !strcmp(item->comm, h->comm)) {
			per_cpu_time -= item->cpu_time;
			break;
		}
	}
	this->per_cpu_time = per_cpu_time;
}

static void procs_refresh(void)
{
	struct task_struct *task;
	struct proc_info **new_process_info;

	procs_hlp(NULL);
	rcu_read_lock();
	for_each_process(task) {
		const struct cred *tcred = get_task_cred(task);
		if (n_alloc == total) {
			n_alloc = 10 + ((n_alloc * 5) / 4);
			new_process_info =
			    kmalloc(sizeof(struct proc_info *) * n_alloc,
				    GFP_KERNEL);
			if (!new_process_info)
				continue;
			memcpy(new_process_info, process_info,
			       sizeof(struct proc_info *) * total);
			if (process_info)
				kfree(process_info);
			process_info = new_process_info;
			memset(process_info + total, 0,
			       sizeof(struct proc_info *) * (n_alloc - total));
		}
		if (!process_info[total])
			process_info[total] =
			    kmalloc(sizeof(struct proc_info), GFP_KERNEL);
		if (!process_info[total])
			continue;
		get_task_comm(process_info[total]->comm, task);
		process_info[total]->pid = task->pid;
		process_info[total]->priority = task->prio - MAX_RT_PRIO;
		process_info[total]->nice = task_nice(task);
		process_info[total]->state = task->state;
		process_info[total]->exit_state = task->exit_state;
		// process_info[total]->utime = task->utime;
		// process_info[total]->stime = task->stime;
		thread_group_cputime_adjusted(task, &process_info[total]->utime, &process_info[total]->stime);
		process_info[total]->uid = tcred->uid.val;
		process_info[total]->uid = 0;
		process_info[total]->state_c = task_state_to_char(task);
		process_info[total]->cutime = task->signal->cutime;
		process_info[total]->cstime = task->signal->cstime;
		process_info[total]->num_threads = get_nr_threads(task);
		put_cred(tcred);
		if (task->mm) {
			process_info[total]->virtual_mem = task->mm->total_vm << (PAGE_SHIFT - 10);	// Convert pages to KB
			process_info[total]->shared_mem =
			    (get_mm_counter(task->mm, MM_FILEPAGES) +
			     get_mm_counter(task->mm,
					    MM_SHMEMPAGES)) << (PAGE_SHIFT -
								10);
			process_info[total]->resident =
			    (get_mm_counter(task->mm, MM_FILEPAGES) +
			     get_mm_counter(task->mm,
					    MM_SHMEMPAGES) +
			     get_mm_counter(task->mm,
					    MM_ANONPAGES)) << (PAGE_SHIFT - 10);
		} else {
			process_info[total]->virtual_mem = 0;
			process_info[total]->shared_mem = 0;
			process_info[total]->resident = 0;
		}
		procs_hlp(process_info[total]);
	}
	rcu_read_unlock();
}

static int compare_proc_info(const void *a, const void *b)
{
	const struct proc_info *p1 = *(const struct proc_info **)a;
	const struct proc_info *p2 = *(const struct proc_info **)b;
	if (p1->per_cpu_time + p1->resident < p2->per_cpu_time + p2->resident)
		return 1;
	else if (p1->per_cpu_time + p1->resident >
		 p2->per_cpu_time + p2->resident)
		return -1;
	else
		return 0;
}

static void frame_make(void)
{
	update_cpu_times();
	procs_refresh();
	msleep(100);
	update_cpu_times();
	procs_refresh();
	sort(process_info, total, sizeof(struct proc_info *), compare_proc_info,
	     NULL);
	show();
}

#ifdef PROC_OPS_USE_FILE_OPERATIONS
static ssize_t my_write(struct file *file, const char __user * buffer,
			size_t count, loff_t * pos)
{
#else
static ssize_t my_write(struct proc_dir_entry *proc, const char __user * buffer,
			size_t count, loff_t * pos)
{
#endif
	char *msg = kmalloc(count + 1, GFP_KERNEL);
	if (!msg)
		return -ENOMEM;

	if (copy_from_user(msg, buffer, count)) {
		kfree(msg);
		return -EFAULT;
	}

	msg[count] = '\0';
	kfree(msg);
	frame_make();
	return count;
}

static int proc_show(struct seq_file *m, void *v)
{
	return 0;
}

static int my_open(struct inode *inode, struct file *file)
{
	return single_open(file, proc_show, NULL);
}

#ifdef PROC_OPS_USE_FILE_OPERATIONS
static const struct file_operations proc_fops = {
	.write = my_write,
	.read = seq_read,
	.open = my_open,
	.release = single_release,
};
#else
static const struct proc_ops proc_fops = {
	.proc_write = my_write,
	.proc_read = seq_read,
	.proc_open = my_open,
	.proc_release = single_release,
};
#endif

static int __init proc_stats_init(void)
{
	hash_init(hash_table_old);
	hash_init(hash_table_new);

	proc_create("kernel_top", 0666, NULL, &proc_fops);

	return 0;
}

static void __exit proc_stats_exit(void)
{
	struct hst *info;
	struct hlist_node *tmp;
	unsigned bkt;
	int i;
	hash_for_each_safe(hash_table_old, bkt, tmp, info, hnode) {
		hash_del(&info->hnode);
		kfree(info);
	}
	for (i = 0; i < n_alloc; ++i) {
		if (process_info[i]) {
			kfree(process_info[i]);
		}
	}
	kfree(process_info);
	remove_proc_entry("kernel_top", NULL);
}

module_init(proc_stats_init);
module_exit(proc_stats_exit);
