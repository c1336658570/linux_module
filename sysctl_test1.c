#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sysctl.h>

static int my_sysctl_value = 0;
static struct ctl_table my_table[] = {
    {
        .procname   = "my_sysctl_value",
        .data       = &my_sysctl_value,
        .maxlen     = sizeof(int),
        .mode       = 0666,
        .proc_handler = proc_dointvec,
    },
    {}
};

static struct ctl_table_header *my_sysctl_header;

static int __init mymodule_init(void)
{
    my_sysctl_header = register_sysctl_table(my_table);
    if (!my_sysctl_header)
        return -ENOMEM;
    return 0;
}

static void __exit mymodule_exit(void)
{
    unregister_sysctl_table(my_sysctl_header);
}

module_init(mymodule_init);
module_exit(mymodule_exit);
MODULE_LICENSE("GPL");
