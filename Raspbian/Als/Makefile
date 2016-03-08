obj-m+=als.o

PWD :=$(shell pwd)
ARCH :=arm

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) ARCH=$(ARCH) modules

