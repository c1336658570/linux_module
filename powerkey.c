#include <linux/module.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/sched/debug.h>
#include <linux/sched/signal.h>
#include <linux/jiffies.h>
#include <linux/timer.h>
#include <linux/mm.h>
#include <linux/swap.h>
#include <linux/vmstat.h>

// #define LONG_PRESS_DURATION 2 * HZ

// static struct timer_list power_key_timer;
// static bool key_pressed = false;

/*
// 定时器回调函数
static void power_key_timer_func(struct timer_list *t)
{
    if (key_pressed) {
        pr_info("ste_powerkey: power key long pressed for 2 seconds!\n");
        // show_state_filter(TASK_UNINTERRUPTIBLE);
    }
}
*/

/*
// 事件处理函数
static void power_key_event(struct input_handle *handle, unsigned int type, unsigned int code, int value)
{
    if (type == EV_KEY && code == KEY_POWER) {
        if (value == 1) {  // 按键按下
            key_pressed = true;
            pr_info("ste_powerkey: power key pressed!\n");
            // show_state_filter(TASK_UNINTERRUPTIBLE);
            mod_timer(&power_key_timer, jiffies + LONG_PRESS_DURATION);
        } else if (value == 0) {  // 按键释放
            key_pressed = false;
            pr_info("ste_powerkey: power key release!\n");
            del_timer(&power_key_timer);
        }
    }
}
*/

static int print_proc_stats(void)
{
	struct task_struct *task;
	int total = 0, running = 0, sleeping = 0, stopped = 0, zombie = 0;

	for_each_process(task) {
		total++;
		if (task->state == TASK_RUNNING) {
			running++;
		} else if (task->state & TASK_UNINTERRUPTIBLE) {
			sleeping++;
		} else if (task->state & TASK_INTERRUPTIBLE) {
			sleeping++;
		} else if (task->exit_state == EXIT_ZOMBIE) {
			zombie++;
		} else if (task->state == TASK_STOPPED
			   || task->state == TASK_TRACED) {
			stopped++;
		}
	}

	pr_info
	    ("ste_powerkey: Tasks: %d total, %d running, %d sleeping, %d stopped, %d zombie\n",
	     total, running, sleeping, stopped, zombie);

	return 0;
}

static int print_meminfo(void)
{
	struct sysinfo si;
	long cached, sreclaimable;

	si_meminfo(&si);
	cached =
	    global_node_page_state(NR_FILE_PAGES) - total_swapcache_pages() -
	    si.bufferram;
	sreclaimable = global_node_page_state(NR_SLAB_RECLAIMABLE);
	pr_info("ste_powerkey: memtotal: %lu MB\n",
		si.totalram * si.mem_unit / 1024 / 1024);
	pr_info("ste_powerkey: memfree: %lu MB\n",
		si.freeram * si.mem_unit / 1024 / 1024);
	pr_info("ste_powerkey: buffer/cache: %lu MB\n",
		(si.bufferram + cached +
		 sreclaimable) * si.mem_unit / 1024 / 1024);

	si_swapinfo(&si);
	pr_info("ste_powerkey: swaptotal: %lu MB\n",
	       si.totalswap * si.mem_unit / 1024 / 1024);
	pr_info("ste_powerkey: swapfree: %lu MB\n",
	       si.freeswap * si.mem_unit / 1024 / 1024);

	return 0;
}

static void power_key_event(struct input_handle *handle, unsigned int type,
			    unsigned int code, int value)
{
	if (type == EV_KEY && code == KEY_POWER) {
		if (value == 1) {	// 按键按下
			pr_info("ste_powerkey: power key pressed!\n");
			// print_proc_stats();
			// print_meminfo();
			// show_state_filter(TASK_UNINTERRUPTIBLE);
		}
	}
}

// 连接函数
static int power_key_connect(struct input_handler *handler,
			     struct input_dev *dev,
			     const struct input_device_id *id)
{
	struct input_handle *handle;
	int error;

	// 分配和初始化输入句柄
	handle = kzalloc(sizeof(struct input_handle), GFP_KERNEL);
	if (!handle)
		return -ENOMEM;

	handle->dev = dev;
	handle->handler = handler;
	handle->name = "power_key_handler";

	// 注册句柄
	error = input_register_handle(handle);
	if (error) {
		kfree(handle);
		return error;
	}
	// 为设备打开事件接口
	error = input_open_device(handle);
	if (error) {
		input_unregister_handle(handle);
		kfree(handle);
		return error;
	}

	return 0;
}

// 断开连接函数
static void power_key_disconnect(struct input_handle *handle)
{
	input_close_device(handle);
	input_unregister_handle(handle);
	kfree(handle);
}

static const struct input_device_id power_key_ids[] = {
	{
	 .flags = INPUT_DEVICE_ID_MATCH_EVBIT | INPUT_DEVICE_ID_MATCH_KEYBIT,
	 .evbit = {[BIT_WORD(EV_KEY)] = BIT_MASK(EV_KEY)},
	 .keybit = {[BIT_WORD(KEY_POWER)] = BIT_MASK(KEY_POWER)},
	 },
	{},
};

/*
// 定义input_device_id
static const struct input_device_id power_key_ids[] = {
    { .driver_info = 1 }, // Matches all devices
    { }, // Terminating zero entry
};
*/

// 定义input_handler
static struct input_handler power_key_handler = {
	.event = power_key_event,
	.connect = power_key_connect,
	.disconnect = power_key_disconnect,
	.name = "power_key_handler",
	.id_table = power_key_ids,
};

static int __init power_key_init(void)
{
	// timer_setup(&power_key_timer, power_key_timer_func, 0);
	return input_register_handler(&power_key_handler);
}

static void __exit power_key_exit(void)
{
	// del_timer(&power_key_timer);
	input_unregister_handler(&power_key_handler);
}

MODULE_LICENSE("GPL");

module_init(power_key_init);
module_exit(power_key_exit);
