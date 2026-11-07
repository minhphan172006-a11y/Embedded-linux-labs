KDIR := $(HOME)/embedded_lab1/kernel/linux-6.6.21
ARCH := arm
CROSS_COMPILE := arm-linux-gnueabihf-

obj-m += asgn1_driver.o

all:
	$(MAKE) -C $(KDIR) M=$(PWD) ARCH=$(ARCH) \
	CROSS_COMPILE=$(CROSS_COMPILE) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) ARCH=$(ARCH) \
	CROSS_COMPILE=$(CROSS_COMPILE) clean

.PHONY: all clean
