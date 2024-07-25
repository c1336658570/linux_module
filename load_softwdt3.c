// SPDX-License-Identifier: GPL-2.0-only

#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/kernel_stat.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/slab.h>
#include <linux/timer.h>
#include <linux/version.h>
#include <linux/workqueue.h>

/* 4s timeout */
#define STE_SOFTWDT_LOAD_MONITOR_DEFAULT_TIMEOUT 4000
/* 2s kick softwdt*/
#define STE_SOFTWDT_LOAD_MONITOR_DEFAULT_KICK_TIME 2000

static int num_cpus;

enum ste_cpu_metric_operation {
	/* Calculate the CPU utilization percentage */
	STE_CPU_METRIC_CALCULATE_USAGE,
	/* Collect detailed CPU timing statistics(user, system, idle, etc.) */
	STE_CPU_METRIC_COLLECT_TIMINGS
};

struct ste_prev_cpu_usage_stats {
	unsigned long prev_idle;
	unsigned long prev_total;
	unsigned long prev_interrupt;
};

struct ste_softwdt_load_monitor {
	/* Feed softwdt interval */
	uint32_t kick_time;
	/* delayed_work to feed the softwdt */
	struct delayed_work softwdt_delayed_work;
	/* workqueue_struct to feed the softwdt */
	struct workqueue_struct *softwdt_wq;
	/* softwdt timer */
	struct timer_list softwdt_timer;
	/* softwdt timeout count */
	uint32_t softwdt_timeout_count;
	/* work_struct for tasks that may sleep or block */
	struct work_struct sleepable_task_work;
	/*
	 * workqueue_struct for tasks that may sleep or
	 * block, deferred from execution in
	 * timer callbacks
	 */
	struct workqueue_struct *sleepable_task_wq;
};

static struct ste_softwdt_load_monitor softwdt_load_monitor;
static struct ste_prev_cpu_usage_stats *prev_cpu_usage_stats;

/*
 * Calculating cpu usage or statistics of CPU usage time at the current moment,
 * such as user, nice, idle...
 */
static void ste_calculate_cpu_stats(enum ste_cpu_metric_operation op)
{
	unsigned long user, nice, system, idle, iowait, irq, softirq, steal,
	    guest, guest_nice;
	unsigned long total, diff_idle, diff_total, interrupt_time,
	    diff_interrupt_time;
	long usage;
	int cpu;

	if (unlikely(op == STE_CPU_METRIC_CALCULATE_USAGE)) {
		pr_info("ste_softwdt_load_monitor: CPU usage from the last "
			"kick softwdt to the current softwdt timeout");
	}

	mb();
	for_each_online_cpu(cpu) {
		mb();
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

		mb();
		total = user + nice + system + idle + iowait + irq + softirq +
		    steal + guest + guest_nice;
		diff_idle = idle - prev_cpu_usage_stats[cpu].prev_idle;
		diff_total = total - prev_cpu_usage_stats[cpu].prev_total;
		interrupt_time = irq + softirq;
		diff_interrupt_time =
		    interrupt_time - prev_cpu_usage_stats[cpu].prev_interrupt;

		if (unlikely(op == STE_CPU_METRIC_CALCULATE_USAGE)) {
			pr_err("CPU:%d, calculate usage:total:%lu, idle:%lu, diff_total:%lu, diff_idle:%lu, interupt_time:%lu, diff_interupt_time:%lu ", cpu, total, idle, diff_total, diff_idle, interrupt_time, diff_interrupt_time);
			if (likely(diff_total != 0)) {
				usage =
				    100 * (diff_total - diff_idle) / diff_total;
				pr_info
				    ("ste_softwdt_load_monitor: CPU%d Usage: "
				     "%ld%%, Interrupt CPU Usage: %ld%%\n", cpu,
				     usage,
				     (diff_interrupt_time * 100) / diff_total);
			} else {
				pr_info
				    ("ste_softwdt_load_monitor: CPU%d No CPU "
				     "usage update\n", cpu);
			}
		} else if (likely(op == STE_CPU_METRIC_COLLECT_TIMINGS)) {
			mb();
			prev_cpu_usage_stats[cpu].prev_idle = idle;
			prev_cpu_usage_stats[cpu].prev_total = total;
			prev_cpu_usage_stats[cpu].prev_interrupt =
			    interrupt_time;
			pr_err("CPU:%d, collect usage:total:%lu, idle:%lu, interrupt_time:%lu", cpu, total, idle, interrupt_time);
			mb();
		}
	}
}

/* feeding the softwdt */
static void ste_softwdt_load_monitor_kick(void)
{
	pr_info("ste_softwdt_load_monitor: kick");
	ste_calculate_cpu_stats(STE_CPU_METRIC_COLLECT_TIMINGS);
	mod_timer(&softwdt_load_monitor.softwdt_timer,
		  jiffies +
		  msecs_to_jiffies(STE_SOFTWDT_LOAD_MONITOR_DEFAULT_TIMEOUT));
}

static DEFINE_SPINLOCK(show_lock);

static void ste_get_current_task_info(void *info)
{
	unsigned long flags;
	struct task_struct *task;
	struct mm_struct *mm;
	long rss = 0;

	spin_lock_irqsave(&show_lock, flags);

	task = current;
	mm = task->mm;
	if (mm)
		rss = get_mm_rss(mm);

	pr_info
	    ("ste_softwdt_load_monitor: CPU: %u, Current Task PID: %d, Comm: "
	     "%s, Nice: %d, CPU Time: %llu, Physical Memory: %lu KB\n",
	     smp_processor_id(), task->pid, task->comm, task_nice(task),
	     (unsigned long long)task->utime + task->stime,
	     rss ? rss * (PAGE_SIZE / 1024) : 0);
	pr_info("ste_softwdt_load_monitor: dump_stack");
	dump_stack();

	spin_unlock_irqrestore(&show_lock, flags);
}

static void ste_handle_sleepable_task(struct work_struct *work)
{
	/*
	 * Print the process information and stack information currently
	 * executed by all CPUs
	 */
	on_each_cpu(ste_get_current_task_info, NULL, 1);
}

static void ste_softwdt_load_monitor_timeout(uintptr_t data)
{
	(void)data;
	softwdt_load_monitor.softwdt_timeout_count++;
	pr_info("ste_softwdt_load_monitor: softwdt timeout "
		"%u time,keep try...\n",
		softwdt_load_monitor.softwdt_timeout_count);
	if (softwdt_load_monitor.softwdt_timeout_count <= 3) {
		ste_calculate_cpu_stats(STE_CPU_METRIC_CALCULATE_USAGE);

		queue_work_on(1, softwdt_load_monitor.sleepable_task_wq,
			      &softwdt_load_monitor.sleepable_task_work);
	} else if (softwdt_load_monitor.softwdt_timeout_count % 10 == 1
		   || softwdt_load_monitor.softwdt_timeout_count % 10 == 3
		   || softwdt_load_monitor.softwdt_timeout_count % 10 == 2) {
		ste_calculate_cpu_stats(STE_CPU_METRIC_CALCULATE_USAGE);

		queue_work_on(1, softwdt_load_monitor.sleepable_task_wq,
			      &softwdt_load_monitor.sleepable_task_work);
	}
	ste_softwdt_load_monitor_kick();
}

static void ste_softwdt_load_monitor_feed_task(struct work_struct *work)
{
	ste_softwdt_load_monitor_kick();
	queue_delayed_work_on(0, softwdt_load_monitor.softwdt_wq,
			      &softwdt_load_monitor.softwdt_delayed_work,
			      msecs_to_jiffies(softwdt_load_monitor.kick_time));
}

static inline void ste_softwdt_load_monitor_timer_init(struct timer_list *timer,
						       uint32_t delay,
						       void (*func)(uintptr_t),
						       uintptr_t arg)
{
	timer_setup(timer, (void *)func, 0);
	timer->expires = jiffies + msecs_to_jiffies(delay);
}

int32_t ste_softwdt_load_monitor_init(void)
{
	pr_info("ste_softwdt_load_monitor: ste_softwdt_load_monitor_init");

	memset((void *)&softwdt_load_monitor, 0, sizeof(softwdt_load_monitor));
	ste_softwdt_load_monitor_timer_init(&softwdt_load_monitor.softwdt_timer,
					    STE_SOFTWDT_LOAD_MONITOR_DEFAULT_TIMEOUT,
					    ste_softwdt_load_monitor_timeout,
					    0);
	softwdt_load_monitor.kick_time =
	    STE_SOFTWDT_LOAD_MONITOR_DEFAULT_KICK_TIME;
	INIT_DELAYED_WORK(&softwdt_load_monitor.softwdt_delayed_work,
			  ste_softwdt_load_monitor_feed_task);

	softwdt_load_monitor.softwdt_wq =
	    create_workqueue("ste_softwdt_load_monitor");
	if (unlikely(softwdt_load_monitor.softwdt_wq == NULL)) {
		pr_info("ste_softwdt_load_monitor:  create "
			"softwdt_wq failed!\n");
		return -1;
	}

	INIT_WORK(&softwdt_load_monitor.sleepable_task_work,
		  ste_handle_sleepable_task);
	softwdt_load_monitor.sleepable_task_wq =
	    create_workqueue("ste_softwdt_load_monitor_sleepable_task_wq");
	if (unlikely(softwdt_load_monitor.sleepable_task_wq == NULL)) {
		pr_info("ste_softwdt_load_monitor: create "
			"sleepable_task_wq failed!\n");
		return -1;
	}

	num_cpus = num_possible_cpus();
	prev_cpu_usage_stats =
	    kcalloc(num_cpus,
		    sizeof(struct ste_prev_cpu_usage_stats), GFP_KERNEL);
	if (!prev_cpu_usage_stats) {
		pr_info("ste_softwdt_load_monitor: Failed to "
			"allocate memory for prev_cpu_usage_stats\n");
		return 0;
	}

	/* start softwdt */
	queue_delayed_work_on(0, softwdt_load_monitor.softwdt_wq,
			      &softwdt_load_monitor.softwdt_delayed_work, 0);
	return 0;
}

void ste_softwdt_load_monitor_exit(void)
{
	pr_info("ste_softwdt_load_monitor: ste_softwdt_load_monitor_exit");

	del_timer_sync(&softwdt_load_monitor.softwdt_timer);
	cancel_delayed_work_sync(&softwdt_load_monitor.softwdt_delayed_work);
	cancel_work_sync(&softwdt_load_monitor.sleepable_task_work);
	destroy_workqueue(softwdt_load_monitor.softwdt_wq);
	destroy_workqueue(softwdt_load_monitor.sleepable_task_wq);
	kfree(prev_cpu_usage_stats);
}

module_init(ste_softwdt_load_monitor_init);
module_exit(ste_softwdt_load_monitor_exit);
MODULE_LICENSE("GPL");
