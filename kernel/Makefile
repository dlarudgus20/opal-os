PLATFORM    := pc-x64
TARGET_TYPE := executable
TARGET_NAME := kernel

all: build

LD_SCRIPT   := platform/$(PLATFORM)/src/linker.ld

include ../mkfiles/conf.mk
include ../mkfiles/rules.mk

TARGET_STRIPPED := $(BUILD_DIR)/kernel.sys

build: $(TARGET_STRIPPED)

$(TARGET_STRIPPED): $(TARGET)
	$(TOOLSET_OBJCOPY) -j .multiboot2 -j .text -j .rodata -j .data -j .bss -S $< $@
