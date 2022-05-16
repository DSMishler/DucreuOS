CROSS_COMPILE?=riscv64-unknown-linux-gnu-
GDB=$(CROSS_COMPILE)gdb
CC=$(CROSS_COMPILE)gcc
CXX=$(CROSS_COMPILE)g++
OBJCOPY=$(CROSS_COMPILE)objcopy
CFLAGS=-g -O0 -Wall -pedantic -Wextra -march=rv64gc -mabi=lp64d -ffreestanding -nostdlib -nostartfiles -Ilibc/ -mcmodel=medany
LDFLAGS=-Tlds/libc.lds -Llibc
LIBS=-lc
SOURCES=$(wildcard *.c)
OBJECTS=$(patsubst %.c,%.elf,$(SOURCES))

# Don't bother checking dependencies in libc. Let libc's makefile
# figure out what it needs to do.
default:
	$(MAKE) -C libc
	make all

all: $(OBJECTS)

%.elf: %.c Makefile libc/libc.a
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS)


# What's a phony target? tells the makefile that you shouldn't be looking
# for a file called "clean"
.PHONY: clean

# $(MAKE): interacts with the job tree, letting Makefile know that it should
#  		  expect to find another Makefile in the directory and make there
#  		  as if it were in that directory.
clean:
	$(MAKE) -C libc clean
	rm -f *.elf

objdump:
	riscv64-unknown-linux-gnu-objdump -d console.elf | less
