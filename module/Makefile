modname := actuator
obj-m := actuator.o seractuator.o

KVERSION := $(shell uname -r)
KDIR := /lib/modules/$(KVERSION)/build
PWD := "$$(pwd)"
MODDESTDIR := /lib/modules/$(KVERSION)/kernel/drivers/char/

default:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) O=$(PWD) -C $(KDIR) M=$(PWD) clean

install:
	install -p -m 644 actuator.ko seractuator.ko $(MODDESTDIR)
	/sbin/depmod -a ${KVERSION}

uninstall:
	rm -f $(MODDESTDIR)/actuator.ko $(MODDESTDIR)/seractuator.ko
	/sbin/depmod -a ${KVER}



