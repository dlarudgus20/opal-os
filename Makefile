TARGET_TYPE := root
PLATFORM    := pc-x64

include mkfiles/conf.mk

KERNEL_ELF  := kernel/$(BUILD_DIR)/kernel.elf
KERNEL_BIN  := kernel/$(BUILD_DIR)/kernel.sys

ISO_DIR     := $(BUILD_DIR)/iso
INITRAMFS   := $(BUILD_DIR)/iso/boot/initramfs
ISO_FILE    := $(BUILD_DIR)/opal-os.iso

QEMU_FLAGS  += -m 128 -serial stdio -no-reboot
QEMU_HDDS   := \
	$(if $(wildcard hda.img), -hda hda.img) \
	$(if $(wildcard hdb.img), -hdb hdb.img) \
	$(if $(wildcard hdd.img), -hdd hdd.img)

ifeq ($(UEFI), 1)
UEFI_FIRMWARE ?= /usr/share/ovmf/OVMF.fd
QEMU_FLAGS += -bios $(UEFI_FIRMWARE)
endif

SUBDIRS     := test-pch kernel libkubsan libkc libpanicimpl libcoll

.PHONY: .FORCE all build iso run mkhdds clean fullclean gen clean-gen build-test test clean-test unit-test clean-unit-test
.FORCE:

all: build

build:
	$(MAKE) -C kernel

iso: $(ISO_FILE)

$(ISO_FILE): build
	@mkdir -p $(ISO_DIR)/boot
	rm -rf $(ISO_DIR)
	cp -rT iso $(ISO_DIR)
	$(MAKE) $(INITRAMFS)
	cp $(KERNEL_BIN) $(ISO_DIR)/boot/kernel.sys
	grub-mkrescue -o $(ISO_FILE) $(ISO_DIR)

$(INITRAMFS): .FORCE
	@mkdir -p $(dir $@)
	cd initramfs && (find . | cpio -o -H newc > ../$(INITRAMFS))

run: iso
	qemu-system-x86_64 $(QEMU_FLAGS) $(QEMU_HDDS) -cdrom $(ISO_FILE) -D qemu.log

mkhdds:
	qemu-img create -f qcow2 hda.img 32M
	qemu-img create -f qcow2 hdb.img 16M
	qemu-img create -f qcow2 hdd.img 8M

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
