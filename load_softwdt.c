// 解析/proc/stat文件

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

/* 12s timeout */
#define LOAD_WATCHDOG_DEFAULT_TIMEOUT 12000
/* 10s kick watchdog*/
#define LOAD_WATCHDOG_DEFAULT_KICK_TIME 10000

#define CPU_INFO_BUFSIZE 4096

static unsigned long *prev_idle, *prev_total, *prev_interrupt;
static int num_cpus;

typedef struct _load_watchdog_ {
	uint32_t kick_time;										/* feed load watchdog interval */
	struct delayed_work watchdog_delayed_work; /* delayed work to feed the load watchdog */
	struct workqueue_struct *watchdog_wq;			/* workqueue_struct for feeding load watchdog */
	struct timer_list watchdog_timer;			 /* load watchdog timer */
	uint32_t watchdog_timeout_count;				 /* load watchdog timeout count */
	struct work_struct cpu_usage_work; /* work used to calculate CPU usage */
} load_watchdog;

static load_watchdog g_load_watchdog;

typedef void (*load_watchdog_timer_func)(uintptr_t);

static void load_watchdog_kick(void) {
	printk(KERN_INFO "kick load watchdog");
	mod_timer(&g_load_watchdog.watchdog_timer, jiffies + msecs_to_jiffies(LOAD_WATCHDOG_DEFAULT_TIMEOUT));
}

static void calculate_cpu_usage(struct work_struct *work) {
	char *buf;
	struct file *f;
	unsigned long user, nice, system, idle, iowait, irq, softirq, steal, guest, guest_nice;
	long usage;
	int cpu;
	loff_t pos = 0;

	buf = kmalloc(CPU_INFO_BUFSIZE, GFP_KERNEL);
	if (!buf) {
		printk(KERN_ERR "Failed to allocate memory\n");
		return;
	}

	f = filp_open("/proc/stat", O_RDONLY, 0);
	if (IS_ERR(f)) {
		printk(KERN_ERR "Cannot open /proc/stat, error %ld\n", PTR_ERR(f));
		return;
	}

	if (kernel_read(f, buf, CPU_INFO_BUFSIZE, &pos) > 0) {
		char *line = buf;
		while (*line != '\n' && *line != '\0')
			line++;
		if (*line == '\n')
			line++;
		for (cpu = 0; cpu < num_cpus; cpu++) {
			unsigned long total, diff_idle, diff_total, interrupt_time, diff_interrupt_time;

			if (sscanf(line, "cpu%*d %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu", &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal, &guest, &guest_nice) == 10) {
				total = user + nice + system + idle + iowait + irq + softirq + steal + guest + guest_nice;
				diff_idle = idle - prev_idle[cpu];
				diff_total = total - prev_total[cpu];
				interrupt_time = irq + softirq;
				diff_interrupt_time = interrupt_time - prev_interrupt[cpu];

				if (likely(prev_total[cpu] != 0)) {
					if (likely(diff_total != 0)) {
						usage = 100 * (diff_total - diff_idle) / diff_total;
						printk(KERN_INFO "CPU%d Usage: %ld%%, Interrupt CPU Usage: %ld%%\n", cpu, usage, (diff_interrupt_time * 100) / diff_total);
					}
					else {
						printk(KERN_INFO "CPU%d No CPU usage update\n", cpu);
					}
				}
				
				prev_idle[cpu] = idle;
				prev_total[cpu] = total;
				prev_interrupt[cpu] = interrupt_time;

				/* move to the next line */
				while (*line != '\n' && *line != '\0')
					line++;
				if (*line == '\n')
					line++;
			}
		}
	}
	else {
		printk(KERN_ERR "Failed to read from /proc/stat\n");
	}

	filp_close(f, NULL);
}

void get_current_task_info(void *info) {
	unsigned int cpu;
	struct task_struct *task;
	struct mm_struct *mm;
	long rss = 0;

	cpu = get_cpu();
	task = current;
	mm = task->mm;
	if (mm) {
		rss = get_mm_rss(mm);
	}

	printk(KERN_INFO "CPU: %u, Current Task PID: %d, Name: %s, Nice: %d, CPU Time: %llu, Physical Memory: %lu KB\n",
				 smp_processor_id(), task->pid, task->comm, task_nice(task), (unsigned long long)task->utime + task->stime,
				 rss ? rss * (PAGE_SIZE / 1024) : 0);

	put_cpu();
}

static void load_watchdog_timeout(uintptr_t data) {
	(void)data;
	g_load_watchdog.watchdog_timeout_count++;
	if (g_load_watchdog.watchdog_timeout_count < 10) {
		printk(KERN_WARNING "load watchdog timeout %u time,keep try...\n", g_load_watchdog.watchdog_timeout_count);

		/* Print the process information currently executed by the CPU */
		smp_call_function(get_current_task_info, NULL, 1);
		get_current_task_info(NULL);

		schedule_work(&g_load_watchdog.cpu_usage_work); /* Calculate CPU usage */

		load_watchdog_kick();
	} else if (g_load_watchdog.watchdog_timeout_count % 10 == 0) {
		printk(KERN_WARNING "load watchdog timeout %u time,keep try...\n", g_load_watchdog.watchdog_timeout_count);

		/* Print the process information currently executed by the CPU */
		smp_call_function(get_current_task_info, NULL, 1);
		get_current_task_info(NULL);

		printk(KERN_WARNING "CPU usage from the last load watchdog timeout to the current load watchdog timeout");
		schedule_work(&g_load_watchdog.cpu_usage_work); /* Calculate CPU usage */

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

	num_cpus = num_possible_cpus();
	prev_idle = kzalloc(sizeof(unsigned long) * num_cpus, GFP_KERNEL);
	prev_total = kzalloc(sizeof(unsigned long) * num_cpus, GFP_KERNEL);
	prev_interrupt = kzalloc(sizeof(unsigned long) * num_cpus, GFP_KERNEL);
	if (!prev_idle || !prev_total || !prev_interrupt) {
		printk(KERN_ERR "Failed to allocate memory\n");
		return 0;
	}
	INIT_WORK(&g_load_watchdog.cpu_usage_work, calculate_cpu_usage);

	/* start load watchdog */
	queue_delayed_work_on(0, g_load_watchdog.watchdog_wq, &g_load_watchdog.watchdog_delayed_work, 0);
	return 0;
}

void load_watchdog_exit(void) {
	printk(KERN_INFO "load watchdog exit");

	del_timer_sync(&g_load_watchdog.watchdog_timer);
	cancel_delayed_work_sync(&g_load_watchdog.watchdog_delayed_work);
	destroy_workqueue(g_load_watchdog.watchdog_wq);
	flush_work(&g_load_watchdog.cpu_usage_work); // make sure all work is done
	kfree(prev_idle);
	kfree(prev_total);
	kfree(prev_interrupt);
}

module_init(load_watchdog_init);
module_exit(load_watchdog_exit);
MODULE_LICENSE("GPL");
