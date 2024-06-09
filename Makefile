# MyOS RISC-V on VisionFive2

DEBUG = build/debug
LDFLAGS = -nostdlib
INCLUDE = -Iinclude
CPFLAGS = -W -nostdlib

GCC = gcc
OBJCOPY = objcopy

boot-obj = boot/start.S boot/boot.c boot/uart.c boot/printf.c


all: build boot.elf OSImage.bin


build:
	mkdir -p $(DEBUG)

OSImage.bin: boot.elf
	$(OBJCOPY) -O binary $(DEBUG)/boot.elf OSImage.bin

boot.elf:
	$(GCC) $(CPFLAGS) $(INCLUDE) -T boot/boot.ld $(boot-obj) -o $(DEBUG)/boot.elf

clean:
	rm -rf build
	rm OSImage.bin