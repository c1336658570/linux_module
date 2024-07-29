obj-m += load_softwdt3.o for_each_process.o kernel_module.o hello.o kset-example.o hds.o \
kobject_test.o all_cpu.o seq-file-test.o load_softwdt2.o livepatch-callbacks-busymod.o kfifo.o \
cdev_driver.o livepatch-shadow-fix1.o livepatch-callbacks-mod.o cpu.o linux_seq.o \
livepatch-shadow-fix2.o kobject-example.o livepatch-callbacks-demo.o cpu_usage2.o cpu_rq.o \
load_softwdt3.o per_cpu_usage.o load_softwdt.o cpu_usage.o livepatch-sample.o livepatch-shadow-mod.o \
mem.o cpu_usage3.o process_cpu_usage.o debugfs_test1.o sysctl_test1.o
# process_info.o ste_top.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
