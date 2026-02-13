TARGET_NAME := kernel
TARGET_TYPE := executable
PROJECT_REFS := libkc

all: build

include ../mkfiles/conf.mk

LD_SCRIPT := platform/$(PLATFORM)/src/linker.ld
include ../mkfiles/rules.mk

TARGET_STRIPPED := $(BUILD_DIR)/kernel.sys

build: $(TARGET_STRIPPED)

$(TARGET_STRIPPED): $(TARGET)
	$(TOOLSET_OBJCOPY) -j .startup -j .text -j .rodata -j .data -j .bss -S $< $@
