OVCAM_KVER := $(shell uname -r)
OVCAM_MAJMIN := $(shell echo $(OVCAM_KVER) | cut -d . -f 1-2)

ifeq ($(OVCAM_MAJMIN),2.6)

ifneq ($(KERNELRELEASE),)
#
# Make rules for use from within 2.6 kbuild system
#
obj-m	+= ov51x-jpeg.o

ov51x-jpeg-objs := ov51x-jpeg-core.o ov511-decomp.o ov518-decomp.o ov519-decomp.o

else  # We were called from command line

KDIR	:= /lib/modules/$(shell uname -r)/build
PWD	:= $(shell pwd)
all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

install: all
	$(MAKE) -C $(KDIR) M=$(PWD) modules_install
	depmod -a

clean:
	rm -rf .*.cmd  *.mod.c *.ko *.o .tmp_versions Module.symvers *~ core *.i *.cmd .ov51x-jpeg-core.o.d
endif

endif
