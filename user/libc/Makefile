CROSS_COMPILE?=riscv64-unknown-linux-gnu-
GDB=$(CROSS_COMPILE)gdb
CC=$(CROSS_COMPILE)gcc
CXX=$(CROSS_COMPILE)g++
OBJCOPY=$(CROSS_COMPILE)objcopy
CFLAGS=-g -O0 -Wall -pedantic -Wextra -march=rv64gc -mabi=lp64d -ffreestanding -nostdlib -nostartfiles -I. -mcmodel=medany -fPIC
# -I. is a bit of a weird choice. It means to include the current directory,
#       but of course it's alreaedy included if you just used quotes around
#       your includes!
# fPIC: force position independent code. This disallows things like function
#       inlining and makes things simpler, but I don't understand it *fully*.
SOURCES_C=$(wildcard *.c)
SOURCES_S=$(wildcard *.S)
OBJECTS=$(patsubst %.c,%.o,$(SOURCES_C)) $(patsubst %.S,%.o,$(SOURCES_S))
# does make let you reference empty variables?
# previously, I forget the `_C` in my SOURCES_C.
# Yes, it does. It was substituting a pattern from a source of *nothing*
OUT=libc.a

all: $(OUT)

$(OUT): $(OBJECTS) Makefile
	ar rcv $(OUT) $(OBJECTS)
	# Office hours moment: what does ar rcv do? What does a mean? What does r mean?

%.o: %.S Makefile
	$(CC) $(CFLAGS) -c $<

%.o: %.c Makefile
	$(CC) $(CFLAGS) -c $<


.PHONY: clean

clean:
	rm -f *.o *.a
