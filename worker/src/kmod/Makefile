obj-m += CpuStatCollector.o
obj-m += SoftirqCollector.o

KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

install: install-softirq install-cpustat

install-softirq:
	sudo insmod SoftirqCollector.ko
	@echo "Module loaded. Device: /dev/SoftirqCollector"

install-cpustat:
	sudo insmod CpuStatCollector.ko
	@echo "Module loaded. Device: /dev/CpuStatCollector"

uninstall: uninstall-softirq uninstall-cpustat

uninstall-softirq:
	-sudo rmmod SoftirqCollector
	@echo "SoftirqCollector module unloaded."

uninstall-cpustat:
	-sudo rmmod CpuStatCollector
	@echo "CpuStatCollector module unloaded."

info:
	modinfo SoftirqCollector.ko
	modinfo CpuStatCollector.ko

log:
	sudo dmesg | tail -30

.PHONY: all clean install install-softirq install-cpustat uninstall uninstall-softirq uninstall-cpustat info log
