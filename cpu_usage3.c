#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/smp.h>
#include <linux/cpumask.h>
#include <linux/cpu.h>
#include <linux/delay.h>
#include <linux/kernel_stat.h>

MODULE_LICENSE("GPL");

static int __init cpu_usage_init(void)
{
    int cpu;
    struct kernel_cpustat kstat_start, kstat_end;
    unsigned long start_user = 0, end_user = 0;
    unsigned long start_system = 0, end_system = 0;
    unsigned long start_nice = 0, end_nice = 0;
    unsigned long start_idle = 0, end_idle = 0;
    unsigned long start_iowait = 0, end_iowait = 0;
    unsigned long start_irq = 0, end_irq = 0;
    unsigned long start_softirq = 0, end_softirq = 0;
    unsigned long start_steal = 0, end_steal = 0;
    unsigned long diff_user ,diff_system, diff_nice, diff_idle, diff_iowait, diff_irq, diff_softirq, diff_steal;
    unsigned long total_diff, total_active_usage;

    // Collect initial data
    for_each_online_cpu(cpu) {
        kstat_start = kcpustat_cpu(cpu);
        start_user += kstat_start.cpustat[CPUTIME_USER];
        start_system += kstat_start.cpustat[CPUTIME_SYSTEM];
        start_nice += kstat_start.cpustat[CPUTIME_NICE];
        start_idle += kstat_start.cpustat[CPUTIME_IDLE];
        start_iowait += kstat_start.cpustat[CPUTIME_IOWAIT];
        start_irq += kstat_start.cpustat[CPUTIME_IRQ];
        start_softirq += kstat_start.cpustat[CPUTIME_SOFTIRQ];
        start_steal += kstat_start.cpustat[CPUTIME_STEAL];
    }

    msleep(100);

    // Collect data after 100 ms
    for_each_online_cpu(cpu) {
        kstat_end = kcpustat_cpu(cpu);
        end_user += kstat_end.cpustat[CPUTIME_USER];
        end_system += kstat_end.cpustat[CPUTIME_SYSTEM];
        end_nice += kstat_end.cpustat[CPUTIME_NICE];
        end_idle += kstat_end.cpustat[CPUTIME_IDLE];
        end_iowait += kstat_end.cpustat[CPUTIME_IOWAIT];
        end_irq += kstat_end.cpustat[CPUTIME_IRQ];
        end_softirq += kstat_end.cpustat[CPUTIME_SOFTIRQ];
        end_steal += kstat_end.cpustat[CPUTIME_STEAL];
    }

    diff_user = end_user - start_user;
    diff_system = end_system - start_system;
    diff_nice = end_nice - start_nice;
    diff_idle = end_idle - start_idle;
    diff_iowait = end_iowait - start_iowait;
    diff_irq = end_irq - start_irq;
    diff_softirq = end_softirq - start_softirq;
    diff_steal = end_steal - start_steal;

    total_diff = diff_user + diff_system + diff_nice + diff_idle + diff_iowait + diff_irq + diff_softirq + diff_steal;

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

        total_active_usage = user_pct + system_pct + nice_pct + iowait_pct + irq_pct + softirq_pct + steal_pct;
        idle_pct = 1000 - total_active_usage; // Ensuring total sums to 100%


        pr_info("CPU(s): %lu.%lu us, %lu.%lu sy, %lu.%lu ni, %lu.%lu id, %lu.%lu wa, %lu.%lu hi, %lu.%lu si, %lu.%lu st\n",
               user_pct / 10, user_pct % 10,
               system_pct / 10, system_pct % 10,
               nice_pct / 10, nice_pct % 10,
               idle_pct / 10, idle_pct % 10,
               iowait_pct / 10, iowait_pct % 10,
               irq_pct / 10, irq_pct % 10,
               softirq_pct / 10, softirq_pct % 10,
               steal_pct / 10, steal_pct % 10);
    }

    return 0;
}

static void __exit cpu_usage_exit(void)
{
}

module_init(cpu_usage_init);
module_exit(cpu_usage_exit);
