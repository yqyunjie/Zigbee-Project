# This Makefile was auto-generated by:
#   create-makefile-from-jam.pl
#
# This Makefile defines how to build the 'standalone-bootloader-demo-gateway' application for
# A unix host application connected to an EM260 EZSP uart device.  This also
# works for Windows machines running Cygwin.
#

# Variables

CC = gcc
LD = gcc
SHELL = /bin/sh

INCLUDES= \
  -I. \
  -I./../../base/rel-znet-5.0 \
  -I./hal \
  -I./hal/micro/generic \
  -I./hal/micro/unix/host \
  -I./hal/micro/unix/simulation \
  -I./phy \
  -I./app/ezsp-uart-host \
  -I./app/standalone-bootloader-demo-host \
  -I./app/util/bootload \
  -I./app/util/ezsp \
  -I./app/util/gateway \
  -I./app/util/serial \
  -I./stack

DEFINES= \
  -DBOARD_HEADER=\"hal/micro/unix/host/board/host.h\" \
  -DPLATFORM_HEADER=\"hal/micro/unix/compiler/gcc.h\" \
  -DHAL_MICRO \
  -DUNIX \
  -DUNIX_HOST \
  -DPHY_NULL \
  -DBOARD_HOST \
  -DCONFIGURATION_HEADER=\"app/standalone-bootloader-demo-host/demo-host-configuration.h\" \
  -DEMBER_SERIAL1_MODE=EMBER_SERIAL_FIFO \
  -DEMBER_SERIAL1_TX_QUEUE_SIZE=128 \
  -DEMBER_SERIAL1_RX_QUEUE_SIZE=64 \
  -DEZSP_HOST \
  -DEMBER_SERIAL0_DEBUG \
  -DAPP_SERIAL=1 \
  -DEZSP_APPLICATION_HAS_BOOTLOADER_HANDLER \
  -DGATEWAY_APP \
  -DBACKCHANNEL_SUPPORT \
  -DEZSP_UART \
  -DPLATFORM_HEADER=\"hal/micro/unix/compiler/gcc.h\"

OPTIONS= \
  -Wcast-align \
  -Wformat \
  -Wimplicit \
  -Wimplicit-int \
  -Wimplicit-function-declaration \
  -Winline \
  -Wno-long-long \
  -Wmain \
  -Wnested-externs \
  -Wno-import \
  -Wparentheses \
  -Wpointer-arith \
  -Wredundant-decls \
  -Wreturn-type \
  -Wstrict-prototypes \
  -Wswitch \
  -Wunused-label \
  -Wunused-value \
  -ggdb \
  -g \
  -O0

APPLICATION_FILES= \
  hal/micro/generic/ash-common.c \
  hal/micro/generic/buzzer-stub.c \
  hal/micro/generic/crc.c \
  hal/micro/generic/led-stub.c \
  hal/micro/generic/mem-util.c \
  hal/micro/generic/random.c \
  hal/micro/generic/system-timer.c \
  hal/micro/unix/host/micro.c \
  app/ezsp-uart-host/ash-host-io.c \
  app/ezsp-uart-host/ash-host-queues.c \
  app/ezsp-uart-host/ash-host-ui.c \
  app/ezsp-uart-host/ash-host.c \
  app/standalone-bootloader-demo-host/common.c \
  app/standalone-bootloader-demo-host/demo.c \
  app/util/bootload/bootload-ezsp-utils.c \
  app/util/ezsp/ezsp-callbacks.c \
  app/util/ezsp/ezsp-enum-decode.c \
  app/util/ezsp/ezsp-frame-utilities.c \
  app/util/ezsp/ezsp-utils.c \
  app/util/ezsp/ezsp.c \
  app/util/ezsp/serial-interface-uart.c \
  app/util/gateway/backchannel.c \
  app/util/gateway/gateway.c \
  app/util/serial/cli.c \
  app/util/serial/ember-printf-convert.c \
  app/util/serial/linux-serial.c

OUTPUT_DIR= build/standalone-bootloader-demo-gateway
OUTPUT_DIR_CREATED= $(OUTPUT_DIR)/created
# Build a list of object files from the source file list, but all objects
# live in the $(OUTPUT_DIR) above.  The list of object files
# created assumes that the file part of the filepath is unique
# (i.e. the bar.c of foo/bar.c is unique across all sub-directories included).
APPLICATION_OBJECTS= $(shell echo $(APPLICATION_FILES) | xargs -n1 echo | sed -e 's^.*/\(.*\)\.c^$(OUTPUT_DIR)/\1\.o^')

APP_FILE= $(OUTPUT_DIR)/standalone-bootloader-demo-gateway

CPPFLAGS= $(INCLUDES) $(DEFINES) $(OPTIONS)
LINK_FLAGS= \
  -lreadline \
 -lncurses

# Rules

all: $(APP_FILE)

ifneq ($(MAKECMDGOALS),clean)
-include $(APPLICATION_OBJECTS:.o=.d)
endif

$(APP_FILE): $(APPLICATION_OBJECTS)
	$(LD) $^ $(LINK_FLAGS) -o $(APP_FILE)
	@echo -e '\n$@ build success'

clean:
	rm -rf $(OUTPUT_DIR)

$(OUTPUT_DIR_CREATED):
	mkdir -p $(OUTPUT_DIR)
	touch $(OUTPUT_DIR_CREATED)

# To facilitate generating all output files in a single output directory, we
# must create separate .o and .d rules for all the different sub-directories
# used by the source files.
# If additional directories are added that are not already in the
# $(APPLICATION_FILES) above, new rules will have to be created below.

# Object File rules

$(OUTPUT_DIR)/%.o: hal/micro/generic/%.c $(OUTPUT_DIR_CREATED)
	$(CC) $(CPPFLAGS) -c $< -o $(OUTPUT_DIR)/$(@F)

$(OUTPUT_DIR)/%.o: hal/micro/unix/host/%.c $(OUTPUT_DIR_CREATED)
	$(CC) $(CPPFLAGS) -c $< -o $(OUTPUT_DIR)/$(@F)

$(OUTPUT_DIR)/%.o: app/ezsp-uart-host/%.c $(OUTPUT_DIR_CREATED)
	$(CC) $(CPPFLAGS) -c $< -o $(OUTPUT_DIR)/$(@F)

$(OUTPUT_DIR)/%.o: app/standalone-bootloader-demo-host/%.c $(OUTPUT_DIR_CREATED)
	$(CC) $(CPPFLAGS) -c $< -o $(OUTPUT_DIR)/$(@F)

$(OUTPUT_DIR)/%.o: app/util/bootload/%.c $(OUTPUT_DIR_CREATED)
	$(CC) $(CPPFLAGS) -c $< -o $(OUTPUT_DIR)/$(@F)

$(OUTPUT_DIR)/%.o: app/util/ezsp/%.c $(OUTPUT_DIR_CREATED)
	$(CC) $(CPPFLAGS) -c $< -o $(OUTPUT_DIR)/$(@F)

$(OUTPUT_DIR)/%.o: app/util/gateway/%.c $(OUTPUT_DIR_CREATED)
	$(CC) $(CPPFLAGS) -c $< -o $(OUTPUT_DIR)/$(@F)

$(OUTPUT_DIR)/%.o: app/util/serial/%.c $(OUTPUT_DIR_CREATED)
	$(CC) $(CPPFLAGS) -c $< -o $(OUTPUT_DIR)/$(@F)

# Dependency rules

$(OUTPUT_DIR)/%.d: hal/micro/generic/%.c $(OUTPUT_DIR_CREATED)
	@set -e; $(CC) -MM $(CPPFLAGS) $<				\
		| sed 's^\($(*F)\)\.o[ :]*^$(OUTPUT_DIR)/\1.o $(OUTPUT_DIR)/$(@F) : ^g' > $(OUTPUT_DIR)/$(@F);	\
		echo 'created dependency file $(OUTPUT_DIR)/$(@F)';			\
		[ -s $(OUTPUT_DIR)/$(@F) ] || rm -f $(OUTPUT_DIR)/$(@F)

$(OUTPUT_DIR)/%.d: hal/micro/unix/host/%.c $(OUTPUT_DIR_CREATED)
	@set -e; $(CC) -MM $(CPPFLAGS) $<				\
		| sed 's^\($(*F)\)\.o[ :]*^$(OUTPUT_DIR)/\1.o $(OUTPUT_DIR)/$(@F) : ^g' > $(OUTPUT_DIR)/$(@F);	\
		echo 'created dependency file $(OUTPUT_DIR)/$(@F)';			\
		[ -s $(OUTPUT_DIR)/$(@F) ] || rm -f $(OUTPUT_DIR)/$(@F)

$(OUTPUT_DIR)/%.d: app/ezsp-uart-host/%.c $(OUTPUT_DIR_CREATED)
	@set -e; $(CC) -MM $(CPPFLAGS) $<				\
		| sed 's^\($(*F)\)\.o[ :]*^$(OUTPUT_DIR)/\1.o $(OUTPUT_DIR)/$(@F) : ^g' > $(OUTPUT_DIR)/$(@F);	\
		echo 'created dependency file $(OUTPUT_DIR)/$(@F)';			\
		[ -s $(OUTPUT_DIR)/$(@F) ] || rm -f $(OUTPUT_DIR)/$(@F)

$(OUTPUT_DIR)/%.d: app/standalone-bootloader-demo-host/%.c $(OUTPUT_DIR_CREATED)
	@set -e; $(CC) -MM $(CPPFLAGS) $<				\
		| sed 's^\($(*F)\)\.o[ :]*^$(OUTPUT_DIR)/\1.o $(OUTPUT_DIR)/$(@F) : ^g' > $(OUTPUT_DIR)/$(@F);	\
		echo 'created dependency file $(OUTPUT_DIR)/$(@F)';			\
		[ -s $(OUTPUT_DIR)/$(@F) ] || rm -f $(OUTPUT_DIR)/$(@F)

$(OUTPUT_DIR)/%.d: app/util/bootload/%.c $(OUTPUT_DIR_CREATED)
	@set -e; $(CC) -MM $(CPPFLAGS) $<				\
		| sed 's^\($(*F)\)\.o[ :]*^$(OUTPUT_DIR)/\1.o $(OUTPUT_DIR)/$(@F) : ^g' > $(OUTPUT_DIR)/$(@F);	\
		echo 'created dependency file $(OUTPUT_DIR)/$(@F)';			\
		[ -s $(OUTPUT_DIR)/$(@F) ] || rm -f $(OUTPUT_DIR)/$(@F)

$(OUTPUT_DIR)/%.d: app/util/ezsp/%.c $(OUTPUT_DIR_CREATED)
	@set -e; $(CC) -MM $(CPPFLAGS) $<				\
		| sed 's^\($(*F)\)\.o[ :]*^$(OUTPUT_DIR)/\1.o $(OUTPUT_DIR)/$(@F) : ^g' > $(OUTPUT_DIR)/$(@F);	\
		echo 'created dependency file $(OUTPUT_DIR)/$(@F)';			\
		[ -s $(OUTPUT_DIR)/$(@F) ] || rm -f $(OUTPUT_DIR)/$(@F)

$(OUTPUT_DIR)/%.d: app/util/gateway/%.c $(OUTPUT_DIR_CREATED)
	@set -e; $(CC) -MM $(CPPFLAGS) $<				\
		| sed 's^\($(*F)\)\.o[ :]*^$(OUTPUT_DIR)/\1.o $(OUTPUT_DIR)/$(@F) : ^g' > $(OUTPUT_DIR)/$(@F);	\
		echo 'created dependency file $(OUTPUT_DIR)/$(@F)';			\
		[ -s $(OUTPUT_DIR)/$(@F) ] || rm -f $(OUTPUT_DIR)/$(@F)

$(OUTPUT_DIR)/%.d: app/util/serial/%.c $(OUTPUT_DIR_CREATED)
	@set -e; $(CC) -MM $(CPPFLAGS) $<				\
		| sed 's^\($(*F)\)\.o[ :]*^$(OUTPUT_DIR)/\1.o $(OUTPUT_DIR)/$(@F) : ^g' > $(OUTPUT_DIR)/$(@F);	\
		echo 'created dependency file $(OUTPUT_DIR)/$(@F)';			\
		[ -s $(OUTPUT_DIR)/$(@F) ] || rm -f $(OUTPUT_DIR)/$(@F)

