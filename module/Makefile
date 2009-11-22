ifneq ($(KERNELRELEASE),)
obj-m	:= actuator.o seractuator.o

else
KDIR	:= /lib/modules/$(shell uname -r)/build
PWD	:= $(shell pwd)

default:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) modules

clean:
	-rm -f *.o *.ko *.mod.c .*.cmd .*.o.flags *~

endif


