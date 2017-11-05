obj-$(CONFIG_VARLINK) += drivers/varlink/
obj-$(CONFIG_EXAMPLE) += drivers/example/

#
# Project Makefile
# Everything below is part of the out-of-tree module and builds the related
# tools if the kernel makefile cannot be used.
#

KERNELVER		?= $(shell uname -r)
KERNELDIR 		?= /lib/modules/$(KERNELVER)/build
SHELL			:= /bin/bash
PWD			:= $(shell pwd)
EXTRA_CFLAGS		+= -I$(PWD)/include
HOST_EXTRACFLAGS	+= -I$(PWD)/usr/include

#
# Default Target
# By default, build the out-of-tree module and everything that belongs into the
# same build.
#
all: module
.PHONY: all

#
# Module Target
# The 'module' target maps to the default out-of-tree target of the current
# tree. This builds the obj-{y,m} contents and also any hostprogs. We need a
# fixup for cflags and configuration options. Everything else is taken directly
# from the kernel makefiles.
#
module:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) \
		HOST_EXTRACFLAGS="$(HOST_EXTRACFLAGS)" \
		EXTRA_CFLAGS="$(EXTRA_CFLAGS)"  \
		CONFIG_VARLINK=m CONFIG_EXAMPLE=m
.PHONY: module

#
# Documentation Target
# The out-of-tree support in the upstream makefile lacks integration with
# documentation targets. Therefore, we need a fixup makefile to make sure our
# documentation makefile works properly.
#
%docs:
	$(MAKE) -f Makefile.docs $@

# Check
# This runs sparse as part of the build process to try to detect any common
# errors in the kernel code.
#
check:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) \
		C=2 CF="-D__CHECK_ENDIAN" \
		HOST_EXTRACFLAGS="$(HOST_EXTRACFLAGS)" \
		EXTRA_CFLAGS="$(EXTRA_CFLAGS)"  \
		CONFIG_VARLINK=m CONFIG_EXAMPLE=m
.PHONY: check

clean:
	rm -f *.o *~ core .depend .*.cmd *.ko *.mod.c
	rm -f drivers/{varlink,example}/{*.ko,*.o,.*.cmd,*.order,*.mod.c}
	rm -f Module.markers Module.symvers modules.order
	rm -rf .tmp_versions Modules.symvers $(hostprogs-y)
.PHONY: clean

install: module
	mkdir -p /lib/modules/$(KERNELVER)/kernel/drivers/varlink
	cp -f drivers/varlink/varlink.ko /lib/modules/$(KERNELVER)/kernel/drivers/varlink
	mkdir -p /lib/modules/$(KERNELVER)/kernel/drivers/example
	cp -f drivers/example/example.ko /lib/modules/$(KERNELVER)/kernel/drivers/example
	depmod $(KERNELVER)
.PHONY: install

uninstall:
	rm -f /lib/modules/$(KERNELVER)/kernel/drivers/varlink/varlink.ko
	rm -f /lib/modules/$(KERNELVER)/kernel/drivers/example/example.ko
.PHONY: uninstall

tt: module
	-sudo sh -c 'dmesg -c > /dev/null'
	-sudo sh -c 'rmmod example'
	-sudo sh -c 'rmmod varlink'
	sudo sh -c 'insmod drivers/varlink/varlink.ko'
	sudo sh -c 'insmod drivers/example/example.ko'
.PHONY: tt

format:
	@for f in drivers/varlink/*.[ch] drivers/example/*.[ch] include/linux/*.h; do \
		echo "  FORMAT $$f"; \
		astyle --quiet --options=.astylerc $$f; \
	done
.PHONY: check-format
