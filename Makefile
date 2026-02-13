PLATFORM    := pc-x64

include mkfiles/conf.mk

KERNEL_ELF  := kernel/$(BUILD_DIR)/kernel.elf
KERNEL_BIN  := kernel/$(BUILD_DIR)/kernel.sys

ISO_DIR     := $(BUILD_DIR)/iso
ISO_FILE    := $(BUILD_DIR)/opal-os.iso

SUBDIRS     := kernel libkc

.PHONY: all kernel iso run clean

all: kernel

kernel:
	$(MAKE) -C kernel CONFIG=$(CONFIG) PLATFORM=$(PLATFORM)

iso: kernel
	@mkdir -p $(ISO_DIR)/boot/grub
	cp $(KERNEL_BIN) $(ISO_DIR)/boot/kernel.sys
	cp grub/grub.cfg $(ISO_DIR)/boot/grub/grub.cfg
	grub-mkrescue -o $(ISO_FILE) $(ISO_DIR)

run: iso
	qemu-system-x86_64 -cdrom $(ISO_FILE) -serial stdio

clean:
	for dir in $(SUBDIRS); do \
		$(MAKE) clean -C $$dir CONFIG=$(CONFIG) PLATFORM=$(PLATFORM) || exit 1; \
	done
	rm -rf $(BUILD_DIR)
