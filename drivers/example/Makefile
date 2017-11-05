%.varlink.c.inc: %.varlink
	echo "static const char *$(subst .,_,$(patsubst %.h,%,$(notdir $<))) =" > $@ && \
	sed -e 's/\(.*\)/"\1\\n"/' < $< >> $@ && \
	echo ';' >> $@

$(obj)/main.o: $(src)/org.kernel.sysinfo.varlink.c.inc $(src)/org.kernel.devices.usb.varlink.c.inc
clean-files := org.kernel.sysinfo.varlink.c.inc org.kernel.devices.usb.varlink.c.inc

example-y := \
	main.o

obj-$(CONFIG_EXAMPLE) += example.o
