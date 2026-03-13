TARGET_TYPE := root
PLATFORM    := pc-x64

include mkfiles/conf.mk

KERNEL_ELF  := kernel/$(BUILD_DIR)/kernel.elf
KERNEL_BIN  := kernel/$(BUILD_DIR)/kernel.sys

ISO_DIR     := $(BUILD_DIR)/iso
ISO_FILE    := $(BUILD_DIR)/opal-os.iso

QEMU_FLAGS  += -m 128 -serial stdio -no-reboot

ifeq ($(UEFI), 1)
UEFI_FIRMWARE ?= /usr/share/ovmf/OVMF.fd
QEMU_FLAGS += -bios $(UEFI_FIRMWARE)
endif

SUBDIRS     := test-pch kernel libkubsan libkc libpanicimpl libcoll

.PHONY: all build iso run clean fullclean gen clean-gen build-test test clean-test unit-test clean-unit-test

all: build

build:
	$(MAKE) -C kernel

iso: build
	@mkdir -p $(ISO_DIR)/boot
	cp -rT iso $(ISO_DIR)
	cp $(KERNEL_BIN) $(ISO_DIR)/boot/kernel.sys
	grub-mkrescue -o $(ISO_FILE) $(ISO_DIR)

run: iso
	qemu-system-x86_64 $(QEMU_FLAGS) -cdrom $(ISO_FILE) -D qemu.log

clean:
	for dir in $(SUBDIRS); do \
		$(MAKE) clean -C $$dir || exit 1; \
	done
	-rm -rf $(BUILD_DIR)

fullclean:
	for dir in $(SUBDIRS); do \
		$(MAKE) fullclean -C $$dir || exit 1; \
	done
	-rm -rf build

gen:
	for dir in $(SUBDIRS); do \
		$(MAKE) gen -C $$dir || exit 1; \
	done

clean-gen:
	for dir in $(SUBDIRS); do \
		$(MAKE) clean-gen -C $$dir || exit 1; \
	done

build-test:
	for dir in $(SUBDIRS); do \
		$(MAKE) build-test -C $$dir || exit 1; \
	done

test:
	for dir in $(SUBDIRS); do \
		$(MAKE) test -C $$dir || exit 1; \
	done

clean-test:
	for dir in $(SUBDIRS); do \
		$(MAKE) clean-test -C $$dir || exit 1; \
	done

unit-test:
	$(MAKE) run UNIT_TEST=1

clean-unit-test:
	$(MAKE) clean -C kernel UNIT_TEST=1
