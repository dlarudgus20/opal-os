TARGET_NAME := opalkrnl
TARGET_TYPE := executable

RUST_OBJ_REFS := opal-kernel
RUST_SYSROOT_REF := rust-sysroot

LD_SCRIPT = arch/$(ARCH)/linker.ld
TARGET_STRIPPED = $(BUILD_DIR)/$(TARGET_NAME).sys

all: build

include ../mkfiles/config-rust.mk
include ../mkfiles/rules-rust.mk

build: $(TARGET_STRIPPED)

$(TARGET_STRIPPED): $(TARGET)
	$(TOOLSET_OBJCOPY) -j .startup -j .text -j .rodata -j .data -j .bss -S $< $@
