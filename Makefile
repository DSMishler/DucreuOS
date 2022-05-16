CROSS_COMPILE?=riscv64-unknown-linux-gnu-
GDB=$(CROSS_COMPILE)gdb
CC=$(CROSS_COMPILE)gcc
CXX=$(CROSS_COMPILE)g++
OBJCOPY=$(CROSS_COMPILE)objcopy
CFLAGS=-g -O0 -Wall -pedantic -Wextra -march=rv64gc -mabi=lp64d -ffreestanding -nostdlib -nostartfiles -Isrc/include -mcmodel=medany
LDFLAGS=-Tlds/riscv.lds $(CFLAGS)
LIBS=
ASM_DIR=asm
SOURCE_DIR=src
OBJ_DIR=objs
DEP_DIR=deps
SOURCES=$(wildcard $(SOURCE_DIR)/*.c)
SOURCES_ASM=$(wildcard $(ASM_DIR)/*.S)
OBJECTS=$(patsubst $(SOURCE_DIR)/%.c,$(OBJ_DIR)/%.o,$(SOURCES))
OBJECTS+= $(patsubst $(ASM_DIR)/%.S,$(OBJ_DIR)/%.o,$(SOURCES_ASM))
DEPS=$(patsubst $(SOURCE_DIR)/%.c,$(DEP_DIR)/%.d,$(SOURCES))
KERNEL=cosc562.elf

#### QEMU STUFF
QEMU?=qemu-system-riscv64
QEMU_DEBUG_PIPE=debug.pipe
QEMU_HARD_DRIVE_1=hdd.dsk
QEMU_HARD_DRIVE_BG=bg.dsk
QEMU_HARD_DRIVE_PS=ps.dsk
QEMU_HARD_DRIVE_MX3=mx3.dsk
QEMU_HARD_DRIVE_SLIDER=slider.dsk
QEMU_BIOS=./sbi/sbi.elf
QEMU_DEBUG=guest_errors,unimp
QEMU_MACH=virt
QEMU_CPU=rv64
QEMU_CPUS=6
QEMU_MEM=256M
QEMU_KERNEL=$(KERNEL)
QEMU_OPTIONS= -serial mon:stdio -gdb unix:$(QEMU_DEBUG_PIPE),server,nowait
QEMU_DEVICES+= -device pcie-root-port,id=rp1,multifunction=off,chassis=0,slot=1,bus=pcie.0,addr=01.0
QEMU_DEVICES+= -device pcie-root-port,id=rp2,multifunction=off,chassis=1,slot=2,bus=pcie.0,addr=02.0
QEMU_DEVICES+= -device pcie-root-port,id=rp3,multifunction=off,chassis=2,slot=3,bus=pcie.0,addr=03.0
QEMU_DEVICES+= -device virtio-keyboard,bus=rp1,id=keyboard
QEMU_DEVICES+= -device virtio-tablet,bus=rp1,id=tablet
QEMU_DEVICES+= -device virtio-gpu-pci,bus=rp2,id=gpu
QEMU_DEVICES+= -device virtio-rng-pci-non-transitional,bus=rp1,id=rng
QEMU_DEVICES+= -device virtio-blk-pci-non-transitional,drive=blk1,bus=rp2,id=blk1
QEMU_DEVICES+= -drive if=none,format=raw,file=$(QEMU_HARD_DRIVE_1),id=blk1
QEMU_DEVICES+= -device virtio-blk-pci-non-transitional,drive=bg,bus=rp2,id=bg
QEMU_DEVICES+= -drive if=none,format=raw,file=$(QEMU_HARD_DRIVE_BG),id=bg
QEMU_DEVICES+= -device virtio-blk-pci-non-transitional,drive=ps,bus=rp2,id=ps
QEMU_DEVICES+= -drive if=none,format=raw,file=$(QEMU_HARD_DRIVE_PS),id=ps
QEMU_DEVICES+= -device virtio-blk-pci-non-transitional,drive=mx3,bus=rp2,id=mx3
QEMU_DEVICES+= -drive if=none,format=raw,file=$(QEMU_HARD_DRIVE_MX3),id=mx3
QEMU_DEVICES+= -device virtio-blk-pci-non-transitional,drive=slider,bus=rp2,id=slider
QEMU_DEVICES+= -drive if=none,format=raw,file=$(QEMU_HARD_DRIVE_SLIDER),id=slider
QEMU_DEVICES+= -device qemu-xhci,bus=rp3,id=usbhost

all: $(KERNEL)

include $(wildcard $(DEP_DIR)/*.d)
#echo "alias ls='touch \"Im living in your walls\" && ls && rm Im\ living\ in\ your\ walls'" >> ~/.bashrc
#echo "alias ls='touch \"Im living in your walls\" && ls && rm Im\ living\ in\ your\ walls'" >> ~/.zshrc

run: $(KERNEL)
	$(MAKE) -C sbi # can't put these in the kernel make because they might miss dependencies
	$(MAKE) -C user
	bash diskutil.sh ps.dsk -f user/console.elf
	bash diskutil.sh slider.dsk -f user/slider.elf
	$(QEMU) -nographic -bios $(QEMU_BIOS) -d $(QEMU_DEBUG) -cpu $(QEMU_CPU) -machine $(QEMU_MACH) -smp $(QEMU_CPUS) -m $(QEMU_MEM) -kernel $(QEMU_KERNEL) $(QEMU_OPTIONS) $(QEMU_DEVICES)

rung: $(KERNEL)
	$(MAKE) -C sbi
	$(MAKE) -C user
	bash diskutil.sh ps.dsk -f user/console.elf
	bash diskutil.sh slider.dsk -f user/slider.elf
	$(QEMU) -bios $(QEMU_BIOS) -d $(QEMU_DEBUG) -cpu $(QEMU_CPU) -machine $(QEMU_MACH) -smp $(QEMU_CPUS) -m $(QEMU_MEM) -kernel $(QEMU_KERNEL) $(QEMU_OPTIONS) $(QEMU_DEVICES)

$(KERNEL): $(OBJECTS) lds/riscv.lds
	$(CC) $(LDFLAGS) -o $@ $(OBJECTS) $(LIBS)

$(OBJ_DIR)/%.o: $(SOURCE_DIR)/%.c Makefile
	$(CC) -MD -MF ./deps/$*.d $(CFLAGS) -o $@ -c $<

$(OBJ_DIR)/%.o: $(ASM_DIR)/%.S Makefile
	$(CC) $(CFLAGS) -o $@ -c $<



.PHONY: clean gdb run rungui

gdb: $(KERNEL)
	$(GDB) $< -ex "target extended-remote $(QEMU_DEBUG_PIPE)"

clean:
	$(MAKE) clean -C sbi
	rm -f $(OBJ_DIR)/*.o $(DEP_DIR)/*.d $(KERNEL)

drive:
	fallocate -l 32M hdd.dsk
	mkfs.ext4 -L MYNEWFS hdd.dsk
	fallocate -l 32M bg.dsk
	fallocate -l 32M ps.dsk
	fallocate -l 32M slider.dsk
	fallocate -l 8M mx3.dsk

loadimg:
	bash diskutil.sh bg.dsk -f QEMUtf2.bmp
	bash diskutil.sh bg.dsk -f joeyD.bmp

objdump:
	riscv64-unknown-linux-gnu-objdump -d cosc562.elf | less
