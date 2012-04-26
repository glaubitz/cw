CC=${DIET} gcc -s -Wall -O2 -I../include
RM=rm -f

CWIO_TARGET=cwio.o

.PHONY: all clean

all: ${CWIO_TARGET}

${CWIO_TARGET}: cwio.h cwio.c
	${CC} -c -o ${CWIO_TARGET} cwio.c

clean:
	${RM} ${CWIO_TARGET} *~ *.bak
