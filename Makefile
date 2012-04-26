PREFIX:=.
include ${PREFIX}/Makefile.conf

.PHONY: all clean

all:
	${MAKE} -C src all DIET="${DIET}"

clean:
	${RM} .uninstall
	${MAKE} -C src clean

install:
	${INSTALL_BASH}

upgrade:
	${UPGRADE_BASH}

uninstall:
	${UNINSTALL_BASH}
