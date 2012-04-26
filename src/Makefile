PREFIX:=..
include ${PREFIX}/Makefile.conf

.PHONY: all clean

all:
	${MAKE} -C driver all
	${MAKE} -C cwtool all DIET="${DIET}"

clean:
	${MAKE} -C driver clean
	${MAKE} -C cwtool clean
