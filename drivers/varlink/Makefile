%.varlink.c.inc: %.varlink
	echo "static const char *$(subst .,_,$(patsubst %.h,%,$(notdir $<))) =" > $@ && \
	sed -e 's/\(.*\)/"\1\\n"/' < $< >> $@ && \
	echo ';' >> $@

$(obj)/service.o: $(src)/org.varlink.service.varlink.c.inc
clean-files := org.varlink.service.varlink.c.inc

varlink-y := \
	buffer.o \
	connection.o \
	interface.o \
	json-array.o \
	json-object.o \
	json-value.o \
	message.o \
	scanner.o \
	service.o \
	service-io.o \
	main.o

obj-$(CONFIG_VARLINK) += varlink.o
