TARGET_NAME := opalkrnl
CARGO_TARGET := x86_64-unknown-none

TARGET_STRIPPED = $(BUILD_DIR)/$(TARGET_NAME).sys

all: build

include ../mkfiles/conf.mk
include ../mkfiles/rules-cargo.mk

build: $(TARGET_STRIPPED)

$(TARGET_STRIPPED): $(TARGET_ELF)
	$(TOOLSET_OBJCOPY) -j .startup -j .text -j .rodata -j .data -j .bss -S $< $@
