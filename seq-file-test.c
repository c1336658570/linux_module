// #include <linux/config.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/percpu.h>
#include <linux/init.h>
#include <linux/sched.h>
// #include <linux/sched/signal.h>

static struct proc_dir_entry *entry;
static loff_t offset = 1;

static void *l_start(struct seq_file *m, loff_t *pos)
{
  loff_t index = *pos;
  loff_t i = 0;
  struct task_struct *task;

  if (index == 0)
  {
    seq_printf(m, "Current all the processes in system:\n"
                  "%-24s%-5s\n",
               "name", "pid");
    printk(KERN_EMERG "++++++++++=========>%5d\n", 0);
    //        offset = 1;
    return &init_task;
  }
  else
  {
    for (i = 0, task = &init_task; i < index; i++)
    {
      task = next_task(task);
    }
    BUG_ON(i != *pos);
    if (task == &init_task)
    {
      return NULL;
    }

    printk(KERN_EMERG "++++++++++>%5d\n", task->pid);
    return task;
  }
}

static void *l_next(struct seq_file *m, void *p, loff_t *pos)
{
  struct task_struct *task = (struct task_struct *)p;

  task = next_task(task);
  if ((*pos != 0) && (task == &init_task))
  {
    //    if ((task == &init_task)) {
    //        printk(KERN_EMERG "=====>%5d\n", task->pid);
    return NULL;
  }

  printk(KERN_EMERG "=====>%5d\n", task->pid);
  offset = ++(*pos);

  return task;
}

static void l_stop(struct seq_file *m, void *p)
{
  printk(KERN_EMERG "------>\n");
}

static int l_show(struct seq_file *m, void *p)
{
  struct task_struct *task = (struct task_struct *)p;

  seq_printf(m, "%-24s%-5d\t%lld\n", task->comm, task->pid, offset);
  //    seq_printf(m, "======>%-24s%-5d\n", task->comm, task->pid);
  return 0;
}

static struct seq_operations exam_seq_op = {
    .start = l_start,
    .next = l_next,
    .stop = l_stop,
    .show = l_show};

static int exam_seq_open(struct inode *inode, struct file *file)
{
  return seq_open(file, &exam_seq_op);
}

// 5.6之前
static struct file_operations exam_seq_fops = {
    .open = exam_seq_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = seq_release,
};

// 5.6之后使用
static const struct proc_ops exam_proc_ops = {
    .proc_open = exam_seq_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = seq_release,
};

static int __init exam_seq_init(void)
{

  //    entry = create_proc_entry("exam_esq_file", 0, NULL);
  // entry = proc_create("exam_esq_file", 0444, NULL, &exam_seq_fops);
  entry = proc_create("exam_esq_file", 0444, NULL, &exam_proc_ops);
  if (!entry)
    printk(KERN_EMERG "proc_create error.\n");
  // entry->proc_fops = &exam_seq_fops;

  printk(KERN_EMERG "exam_seq_init.\n");
  return 0;
}

static void __exit exam_seq_exit(void)
{
  remove_proc_entry("exam_esq_file", NULL);
  printk(KERN_EMERG "exam_seq_exit.\n");
}

module_init(exam_seq_init);
module_exit(exam_seq_exit);
MODULE_LICENSE("GPL");