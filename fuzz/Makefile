# Enable debugging and suppress pesky warnings
CFLAGS ?= -g -w
CC=afl-clang

all:	vulnerable

clean:
	rm -f vulnerable

vulnerable: vulnerable.c 
	AFL_HARDEN=1 ${CC} ${CFLAGS} vulnerable.c -o vulnerable
