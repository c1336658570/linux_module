obj-m := kernel_module.o hds.o hello.o kfifo.o \
livepatch-sample.o livepatch-callbacks-busymod.o \
livepatch-callbacks-demo.o livepatch-callbacks-mod.o \
livepatch-shadow-fix1.o livepatch-shadow-fix2.o \
livepatch-shadow-mod.o kobject_test.o kobject-example.o \
kset-example.o seq-file-test.o

KERNELBUILD := /lib/modules/$(shell uname -r)/build

default:
# -C $(KDIR) 指明跳转到内核源码目录下读取那里的Makefile；M=$(PWD) 表明然后返回到当�? �?录继�? 读入、执�? 当前的Makefile�?
	make -C $(KERNELBUILD) M=$(shell pwd) modules


# 这是另一�?�?以编译程序的makefile
#obj-m:=hds.o						#根据make的自动推导原则，make会自动将源程序hds.c编译成目标程序hds.o�?
                                    #所有在配置文件�?标�?�为-m的模块将�?编译成可动态加载进内核的模块。即后缀�?.ko的文件�?
CURRENT_PATH:=$(shell pwd)  		#参数化，将模块源码路径保存在CURRENT_PATH�?
LINUX_KERNEL:=$(shell uname -r) 	#参数化，将当前内核版�?保存在LINUX_KERNEL�?
LINUX_KERNEL_PATH:=/usr/src/linux-headers-$(LINUX_KERNEL) 	
                                    #参数化，将内核源代码的绝对路径保存在LINUX_KERNEL_PATH�?
#all:
#	make -C $(LINUX_KERNEL_PATH) M=$(CURRENT_PATH) modules 	#编译模块
clean:
	make -C $(LINUX_KERNEL_PATH) M=$(CURRENT_PATH) clean  	#清理
