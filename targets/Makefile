# tools
CC := gcc
RM := rm -f

# flags
CFLAGS := -ggdb -m64 -g -std=c99 -D_GNU_SOURCE -fno-stack-protector -mpreferred-stack-boundary=3 -Wno-format-security
LDFLAGS := -m64 -z execstack

# gmake magic
.PHONY: default install clean

# targets
default: target4

install: target4
	install -o root -t /tmp target4
	chmod 4755 /tmp/target*

%: %.c
	$(CC) $(CFLAGS) $(LDFLAGS) $< -o $@

%.o: %.S
	$(CC) $< -c -o $@ $(CFLAGS)

# Disable executable stack for target5
target4.o: LDFLAGS := -m64
target4.o: target4.c
	$(CC) $(CFLAGS) $(LDFLAGS) $< -c -o $@

target4: CC := musl-gcc
target4: LDFLAGS := -m64
target4: target4.o
	$(CC) -static $(LDFLAGS) $^ -o $@ -Wl,--whole-archive

clean:
	$(RM) target4 target4.o
