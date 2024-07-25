#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched/signal.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/mm.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("A simple Linux module to list process information.");

static int __init list_process_init(void) {
    struct task_struct *task;
    char *task_state;

    printk(KERN_INFO "Loading Module\n");

    // 遍历进程列表
    for_each_process(task) {
        // 获取进程状态
        switch (task->state) {
            case TASK_RUNNING:
                task_state = "R (running)";
                break;
            case TASK_INTERRUPTIBLE:
                task_state = "S (sleeping)";
                break;
            case TASK_UNINTERRUPTIBLE:
                task_state = "D (disk sleep)";
                break;
            case TASK_STOPPED:
                task_state = "T (stopped)";
                break;
            case TASK_TRACED:
                task_state = "t (traced)";
                break;
            default:
                task_state = "U (unknown)";
        }

        if (task->mm) {  // Ensure the task has a memory descriptor.
            get_task_comm(info.comm, task);
            info.pid = task->pid;
            info.priority = task->prio;
            info.nice = task_nice(task);
            info.virtual_mem = task->mm->total_vm << (PAGE_SHIFT - 10);  // Convert pages to KB
            info.physical_mem = get_mm_rss(task->mm) << (PAGE_SHIFT - 10);  // Convert pages to KB
            info.shared_mem = get_mm_counter(task->mm, MM_SHMEMPAGES) << (PAGE_SHIFT - 10);  // Convert pages to KB
            info.state = task_state_to_char(task);
            info.time_plus = task->utime + task->stime; // User time + System time
            
            printk(KERN_INFO "PID: %d, USER: N/A, PR: %ld, NI: %ld, VIRT: %lu KB, RES: %lu KB, SHR: %lu KB, S: %c, TIME+: %llu, CMD: %s\n",
                info.pid, info.priority, info.nice, info.virtual_mem, info.physical_mem, info.shared_mem,
                info.state, info.time_plus, info.comm);
        }

        // 打印进程信息
        printk(KERN_INFO "PID: %d | USER: %d | PR: %d | NI: %d | VIRT: %lu | RES: %lu | SHR: %lu | S: %s | MEM: %lu | TIME+: %llu | COMMAND: %s\n",
               task->pid, 
               from_kuid(&init_user_ns, task->cred->uid), 
               task->prio, 
               task->normal_prio, 
               task->mm ? task->mm->total_vm * 4 : 0, // VIRT
               task->mm ? get_mm_rss(task->mm) * 4 : 0, // RES
               task->mm ? task->mm->shared_vm * 4 : 0, // SHR
               task_state,
               task->mm ? get_mm_rss(task->mm) * 4 * 100 / totalram_pages() : 0, // MEM percentage
               (unsigned long long) task->utime, // 这只是用户态时间，不是完整的TIME+
               task->comm);
    }

    return 0;
}

static void __exit list_process_exit(void) {
    printk(KERN_INFO "Removing Module\n");
}

module_init(list_process_init);
module_exit(list_process_exit);
