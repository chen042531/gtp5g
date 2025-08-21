obj-m += lockup_test.o

KERNEL_SRC := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all:
	$(MAKE) -C $(KERNEL_SRC) M=$(PWD) modules

clean:
	$(MAKE) -C $(KERNEL_SRC) M=$(PWD) clean

install: all
	sudo insmod lockup_test.ko

uninstall:
	sudo rmmod lockup_test

# 测试不同场景
test-spinlock:
	sudo insmod lockup_test.ko use_bh_lock=0 num_threads=8 work_delay_us=50 test_duration=60

test-spinlock-bh:
	sudo insmod lockup_test.ko use_bh_lock=1 num_threads=8 work_delay_us=50 test_duration=60

test-lockfree:
	sudo insmod lockup_test.ko use_bh_lock=2 num_threads=8 work_delay_us=50 test_duration=60

# 高压力测试
test-high-pressure:
	sudo insmod lockup_test.ko use_bh_lock=0 num_threads=16 work_delay_us=100 test_duration=30

stats:
	cat /proc/lockup_test_stats

monitor:
	watch -n 1 cat /proc/lockup_test_stats

.PHONY: all clean install uninstall test-spinlock test-spinlock-bh test-lockfree test-high-pressure stats monitor

