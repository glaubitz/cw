# uncomment this to build the driver with debugging messages. To increase
# debug level use cw_debug_level=<number> while loading the kernel module
# (with <number> being a decimal number)
#DEBUG=-DCW_DEBUG

FILES:=driver hardware floppy
OBJECTS:=${patsubst %, %.o, ${FILES}}

ifneq (${KERNELRELEASE},)

# UGLY: include directory is hard coded here

EXTRA_CFLAGS:=${DEBUG} -I${CW_DIR}/../include
obj-m:=cw.o
cw-y:=${OBJECTS}
else
PREFIX:=../..
include ${PREFIX}/Makefile.conf

TARGET24:=${BUILD_MODULE_DIR}/cw.o
TARGET26:=${BUILD_MODULE_DIR}/cw.ko

KERNEL_DIR:=/lib/modules/${shell uname -r}/build
VERSION:=${shell uname -r | awk 'BEGIN { FS="."; } { print($$1 $$2); }'}
INCLUDE:=-I${KERNEL_DIR}/include -I${BUILD_INCLUDE_DIR}
CWD:=${shell pwd}
CC:=gcc -Wall -O2 -D__KERNEL__ -DMODULE ${DEBUG} ${INCLUDE}
LD:=ld -r -s

.PHONY: all linux24 linux26 clean

all:
	if [ "${VERSION}" = "26" ]; then ${MAKE} linux26; else ${MAKE} linux24; fi

linux24: config.h firmware.c ${TARGET24}

linux26: config.h firmware.c
	CW_DIR=${CWD} ${MAKE} -C ${KERNEL_DIR} M=${CWD} modules && ${CP} cw.ko ${TARGET26}

config.h:
	${CONFIG_BASH} > config.h

firmware.c: firmware.bin
	${FWDUMP_BASH} < firmware.bin > firmware.c

%.o: %.c
	${CC} -c -o $@ $<

${TARGET24}: ${OBJECTS}
	${LD} -o ${TARGET24} ${OBJECTS}

clean:
	if [ "${VERSION}" = "26" ]; then ${MAKE} -C ${KERNEL_DIR} M=${CWD} clean; fi
	${RM} ${TARGET24} ${TARGET26} ${OBJECTS} ${BUILD_BIN_DIR}/fwdump config.h firmware.c *~ *.bak *.ko Module.symvers Modules.symvers Module.markers modules.order
endif
