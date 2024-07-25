/* SPDX-License-Identifier: GPL-2.0-only */

// 重构前
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/slab.h>
#include <linux/kernel_stat.h>
#include <linux/delay.h>

/* 4s timeout */
#define LOAD_WATCHDOG_DEFAULT_TIMEOUT 4000
/* 2s kick watchdog*/
#define LOAD_WATCHDOG_DEFAULT_KICK_TIME 2000

static unsigned long *prev_idle, *prev_total, *prev_interrupt;
static int num_cpus;

typedef struct _load_watchdog_ {
	uint32_t kick_time;										/* feed load watchdog interval */
	struct delayed_work watchdog_delayed_work; /* delayed work to feed the load watchdog */
	struct workqueue_struct *watchdog_wq;			/* workqueue_struct for feeding load watchdog */
	struct timer_list watchdog_timer;			 /* load watchdog timer */
	uint32_t watchdog_timeout_count;				 /* load watchdog timeout count */
	struct work_struct get_task_info_work;	/* work to get the cpus' currently running process information */
	struct workqueue_struct *get_task_info_wq;	/* workqueue to get the cpus' currently running process information */
} load_watchdog;

static load_watchdog g_load_watchdog;

typedef void (*load_watchdog_timer_func)(uintptr_t);

/* statistics of CPU usage time at the current moment, such as user, nice, idle... */
static void statistics_cpu_time(void) {
	unsigned long user, nice, system, idle, iowait, irq, softirq, steal, guest, guest_nice;
	unsigned long total, interrupt_time;
	int cpu;

	for_each_possible_cpu(cpu) {
		user = kcpustat_cpu(cpu).cpustat[CPUTIME_USER];
		nice = kcpustat_cpu(cpu).cpustat[CPUTIME_NICE];
		system = kcpustat_cpu(cpu).cpustat[CPUTIME_SYSTEM];
		idle = kcpustat_cpu(cpu).cpustat[CPUTIME_IDLE];
		iowait = kcpustat_cpu(cpu).cpustat[CPUTIME_IOWAIT];
		irq = kcpustat_cpu(cpu).cpustat[CPUTIME_IRQ];
		softirq = kcpustat_cpu(cpu).cpustat[CPUTIME_SOFTIRQ];
		steal = kcpustat_cpu(cpu).cpustat[CPUTIME_STEAL];
		guest = kcpustat_cpu(cpu).cpustat[CPUTIME_GUEST];
		guest_nice = kcpustat_cpu(cpu).cpustat[CPUTIME_GUEST_NICE];

		total = user + nice + system + idle + iowait + irq + softirq + steal + guest + guest_nice;
		interrupt_time = irq + softirq;
		
		prev_idle[cpu] = idle;
		prev_total[cpu] = total;
		prev_interrupt[cpu] = interrupt_time;
	}
}

/* feeding the load watchdog */
static void load_watchdog_kick(void) {
	printk(KERN_INFO "kick load watchdog");
	statistics_cpu_time();
	mod_timer(&g_load_watchdog.watchdog_timer, jiffies + msecs_to_jiffies(LOAD_WATCHDOG_DEFAULT_TIMEOUT));
}

/* calculating cpu usage */
static void calculate_cpu_usage(void) {
	unsigned long user, nice, system, idle, iowait, irq, softirq, steal, guest, guest_nice;
	unsigned long total, diff_idle, diff_total, interrupt_time, diff_interrupt_time;
	long usage;
	int cpu;
	
	printk(KERN_WARNING "CPU usage from the last kick load watchdog to the current load watchdog timeout");

	for_each_possible_cpu(cpu) {
		user = kcpustat_cpu(cpu).cpustat[CPUTIME_USER];
		nice = kcpustat_cpu(cpu).cpustat[CPUTIME_NICE];
		system = kcpustat_cpu(cpu).cpustat[CPUTIME_SYSTEM];
		idle = kcpustat_cpu(cpu).cpustat[CPUTIME_IDLE];
		iowait = kcpustat_cpu(cpu).cpustat[CPUTIME_IOWAIT];
		irq = kcpustat_cpu(cpu).cpustat[CPUTIME_IRQ];
		softirq = kcpustat_cpu(cpu).cpustat[CPUTIME_SOFTIRQ];
		steal = kcpustat_cpu(cpu).cpustat[CPUTIME_STEAL];
		guest = kcpustat_cpu(cpu).cpustat[CPUTIME_GUEST];
		guest_nice = kcpustat_cpu(cpu).cpustat[CPUTIME_GUEST_NICE];

		total = user + nice + system + idle + iowait + irq + softirq + steal + guest + guest_nice;
		diff_idle = idle - prev_idle[cpu];
		diff_total = total - prev_total[cpu];
		interrupt_time = irq + softirq;
		diff_interrupt_time = interrupt_time - prev_interrupt[cpu];

		if (likely(diff_total != 0)) {
			usage = 100 * (diff_total - diff_idle) / diff_total;
			printk(KERN_WARNING "CPU%d Usage: %ld%%, Interrupt CPU Usage: %ld%%\n", cpu, usage, (diff_interrupt_time * 100) / diff_total);
		}
		else {
			printk(KERN_WARNING "CPU%d No CPU usage update\n", cpu);
		}
	}
}

static DEFINE_SPINLOCK(show_lock);

static void get_current_task_info(void *info) {
	unsigned long flags;
	struct task_struct *task;
	struct mm_struct *mm;
	long rss = 0;

	spin_lock_irqsave(&show_lock, flags);

	task = current;
	mm = task->mm;
	if (mm) {
		rss = get_mm_rss(mm);
	}

	printk(KERN_INFO "CPU: %u, Current Task PID: %d, Name: %s, Nice: %d, CPU Time: %llu, Physical Memory: %lu KB\n",
				 smp_processor_id(), task->pid, task->comm, task_nice(task), (unsigned long long)task->utime + task->stime,
				 rss ? rss * (PAGE_SIZE / 1024) : 0);
	dump_stack();

	spin_unlock_irqrestore(&show_lock, flags);
}

static void get_task_info(struct work_struct *work) {
	/* Print the process information currently executed by the CPU */
	smp_call_function(get_current_task_info, NULL, 1);
	get_current_task_info(NULL);
}

static void load_watchdog_timeout(uintptr_t data) {
	(void)data;
	g_load_watchdog.watchdog_timeout_count++;
	if (g_load_watchdog.watchdog_timeout_count < 10) {
		printk(KERN_WARNING "load watchdog timeout %u time,keep try...\n", g_load_watchdog.watchdog_timeout_count);

		calculate_cpu_usage();
		
		queue_work_on(1, g_load_watchdog.get_task_info_wq, &g_load_watchdog.get_task_info_work);

		load_watchdog_kick();
	} else if (g_load_watchdog.watchdog_timeout_count % 10 == 0) {
		printk(KERN_WARNING "load watchdog timeout %u time,keep try...\n", g_load_watchdog.watchdog_timeout_count);

		calculate_cpu_usage();

		queue_work_on(1, g_load_watchdog.get_task_info_wq, &g_load_watchdog.get_task_info_work);
		
		load_watchdog_kick();
	}

	return;
}

static void load_watchdog_feed_task(struct work_struct *pst_work) {
	load_watchdog_kick();
	queue_delayed_work_on(0, g_load_watchdog.watchdog_wq, &g_load_watchdog.watchdog_delayed_work,
												msecs_to_jiffies(g_load_watchdog.kick_time));

	return;
}

static inline void load_watchdog_timer_init(struct timer_list *pst_timer, uint32_t ul_delay,
																	 load_watchdog_timer_func p_func, uintptr_t arg) {
	timer_setup(pst_timer, (void *)p_func, 0);
	pst_timer->expires = jiffies + msecs_to_jiffies(ul_delay);
}

int32_t load_watchdog_init(void) {
	printk(KERN_INFO "load watchdog init");

	memset((void *)&g_load_watchdog, 0, sizeof(g_load_watchdog));
	load_watchdog_timer_init(&g_load_watchdog.watchdog_timer, LOAD_WATCHDOG_DEFAULT_TIMEOUT, load_watchdog_timeout, 0);
	g_load_watchdog.kick_time = LOAD_WATCHDOG_DEFAULT_KICK_TIME;
	INIT_DELAYED_WORK(&g_load_watchdog.watchdog_delayed_work, load_watchdog_feed_task);

	g_load_watchdog.watchdog_wq = create_workqueue("load_watchdog");
	if (unlikely(g_load_watchdog.watchdog_wq == NULL)) {
		printk(KERN_INFO "load watchdog create workqueue failed!\n");
		return -1;
	}

	INIT_WORK(&g_load_watchdog.get_task_info_work, get_task_info);
	g_load_watchdog.get_task_info_wq = create_workqueue("get_task_info");
	if (unlikely(g_load_watchdog.get_task_info_wq == NULL)) {
		printk(KERN_INFO "get task info create workqueue failed!\n");
		return -1;
	}

	num_cpus = num_possible_cpus();
	prev_idle = kzalloc(sizeof(unsigned long) * num_cpus, GFP_KERNEL);
	prev_total = kzalloc(sizeof(unsigned long) * num_cpus, GFP_KERNEL);
	prev_interrupt = kzalloc(sizeof(unsigned long) * num_cpus, GFP_KERNEL);
	if (!prev_idle || !prev_total || !prev_interrupt) {
		printk(KERN_ERR "Failed to allocate memory\n");
		return 0;
	}

	/* start load watchdog */
	queue_delayed_work_on(0, g_load_watchdog.watchdog_wq, &g_load_watchdog.watchdog_delayed_work, 0);
	return 0;
}

void load_watchdog_exit(void) {
	printk(KERN_INFO "load watchdog exit");

	del_timer_sync(&g_load_watchdog.watchdog_timer);
	cancel_delayed_work_sync(&g_load_watchdog.watchdog_delayed_work);
	cancel_work_sync(&g_load_watchdog.get_task_info_work);
	destroy_workqueue(g_load_watchdog.watchdog_wq);
	destroy_workqueue(g_load_watchdog.get_task_info_wq);
	kfree(prev_idle);
	kfree(prev_total);
	kfree(prev_interrupt);
}

module_init(load_watchdog_init);
module_exit(load_watchdog_exit);
MODULE_LICENSE("GPL");
