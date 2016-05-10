obj-m+=accel.o

PWD :=$(shell pwd)
ARCH :=arm
EXTRA_CFLAGS=-I/usr/local/include
EXTRA_LDFLAGS=-lwiringPi

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) ARCH=$(ARCH) modules

