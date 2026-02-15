TARGET_NAME := kernel
TARGET_TYPE := executable
STATIC_REFS := libkc

all: build

# configurations

LDFLAGS_ON_TEST := -Wl,--exclude-libs=libkc

include ../mkfiles/conf.mk
-include platform/$(PLATFORM)/config.mk

ifneq ($(IS_TEST), 1)
STATIC_REFS += libkubsan
endif

# rules

include ../mkfiles/rules.mk

ifneq ($(IS_TEST), 1)
TARGET_STRIPPED := $(BUILD_DIR)/kernel.sys

$(TARGET_STRIPPED): $(TARGET)
	$(TOOLSET_OBJCOPY) -j .startup -j .text -j .rodata -j .data -j .bss -S $< $@

TARGET := $(TARGET_STRIPPED)
endif

build: $(TARGET)
