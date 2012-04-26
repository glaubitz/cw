CWIO_DIR=..
CC=${DIET} gcc -s -Wall -O2 -I${CWIO_DIR}
MAKE=make
RM=rm -f

OBJECTS=cwio.o read.o write.o
TARGETS=read write

.PHONY: all clean

all: ${TARGETS}

cwio.o:
	${MAKE} -C ${CWIO_DIR} CWIO_TARGET=$$PWD/cwio.o

%.o: %.c
	${CC} -c -o $@ $<

read: read.o cwio.o
	${CC} -o read read.o cwio.o

write: write.o cwio.o
	${CC} -o write write.o cwio.o

clean:
	${RM} ${TARGETS} ${OBJECTS} *~ *.bak
