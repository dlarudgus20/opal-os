TARGET_NAME         := kernel
TARGET_TYPE         := executable
STATIC_REFS         := libkc libkubsan
INCLUDE_REFS        := libopalsys

include ../mkfiles/config.mk
-include platform/$(PLATFORM)/config.mk
include res.mk

CFLAGS              += -DOPAL_KERNEL

KERNEL_BIN          := $(BUILD_DIR)/kernel.sys

include ../mkfiles/rules.mk

build: $(KERNEL_BIN)

$(KERNEL_BIN): $(TARGET)
	$(TOOLSET_OBJCOPY) -j .startup -j .text -j .rodata -j .data -j .bss -S $< $@
