PLATFORM       := pc-x64
BUILD_DIR      := build

TOOLSET_PREFIX ?= x86_64-elf
TOOLSET_CC     ?= $(TOOLSET_PREFIX)-gcc
TOOLSET_LD     ?= $(TOOLSET_PREFIX)-ld
TOOLSET_NASM   ?= nasm

CFLAGS         := -std=c11 -Wall -Wextra -ffreestanding -fno-stack-protector -fno-pic -m64 -mno-red-zone -mcmodel=kernel -Iinclude -Iplatform/$(PLATFORM)/include
LDFLAGS        := -m elf_x86_64 -T platform/$(PLATFORM)/src/linker.ld -nostdlib

C_SOURCES      := \
	src/kmain.c \
	src/drivers/uart.c \
	platform/$(PLATFORM)/src/drivers/uart.c

ASM_SOURCES    := \
	platform/$(PLATFORM)/src/boot.asm

C_OBJECTS      := $(patsubst %.c,$(BUILD_DIR)/%.c.o,$(C_SOURCES))
ASM_OBJECT     := $(patsubst %.asm,$(BUILD_DIR)/%.asm.o,$(ASM_SOURCES))
KERNEL_BIN     := $(BUILD_DIR)/kernel.bin

.PHONY: all clean

all: $(KERNEL_BIN)

$(BUILD_DIR)/%.c.o: %.c
	@mkdir -p $(dir $@)
	$(TOOLSET_CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.asm.o: %.asm
	@mkdir -p $(dir $@)
	$(TOOLSET_NASM) -f elf64 $< -o $@

$(KERNEL_BIN): $(C_OBJECTS) $(ASM_OBJECT)
	$(TOOLSET_LD) $(LDFLAGS) -o $@ $(ASM_OBJECT) $(C_OBJECTS)

clean:
	rm -rf $(BUILD_DIR)
