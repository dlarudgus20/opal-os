TARGET_TYPE := root

ifeq ($(RUST), 1)
include mkfiles/config-rust.mk
KERNEL := opalkrnl
else # RUST
include mkfiles/conf.mk
KERNEL := kernel
endif # RUST

KERNEL_ELF  := $(KERNEL)/$(BUILD_DIR)/$(KERNEL).elf
KERNEL_BIN  := $(KERNEL)/$(BUILD_DIR)/$(KERNEL).sys

ISO_DIR     := $(BUILD_DIR)/iso
ISO_FILE    := $(BUILD_DIR)/opal-os.iso

INITRAMFS_DIR := $(BUILD_DIR)/initramfs
INITRAMFS   := $(BUILD_DIR)/iso/boot/initramfs

QEMU_FLAGS  += -m 128 -serial stdio -no-reboot -boot order=dc
QEMU_HDDS   := \
	$(if $(wildcard hda.img), -hda hda.img) \
	$(if $(wildcard hdb.img), -hdb hdb.img) \
	$(if $(wildcard hdd.img), -hdd hdd.img)

ifeq ($(UEFI), 1)
UEFI_FIRMWARE ?= /usr/share/ovmf/OVMF.fd
QEMU_FLAGS  += -bios $(UEFI_FIRMWARE)
endif # UEFI

ifeq ($(QEMU_DISPNONE), 1)
QEMU_FLAGS  += -display none
endif # QEMU_DISPNONE

ifeq ($(QEMU_DEBUG_EXIT), 1)
QEMU_FLAGS  += -device isa-debug-exit,iobase=0xf4,iosize=0x04
endif # QEMU_DEBUG_EXIT

ifeq ($(RUST), 1)
SUBDIRS     :=
CLEAN_SUBDIRS := opal-ktest opal-kernel opalkrnl rust-sysroot
else # RUST
SUBDIRS     := test-pch kernel libkubsan libkc libpanicimpl libcoll
CLEAN_SUBDIRS := $(SUBDIRS)
endif # RUST

all: build

.PHONY: .FORCE all build run clean clean-root fullclean \
	gen clean-gen build-test test clean-test unit-test clean-unit-test \
	iso disk-images rust-project.json
.FORCE:
.NOTPARALLEL:

build:
	$(MAKE) -C $(KERNEL)

iso: $(ISO_FILE)

$(ISO_FILE): build
	@mkdir -p $(ISO_DIR)/boot
	rm -rf $(ISO_DIR)
	cp -rT iso $(ISO_DIR)
	$(MAKE) $(INITRAMFS)
	cp $(KERNEL_BIN) $(ISO_DIR)/boot/kernel.sys
	grub-mkrescue -o $(ISO_FILE) $(ISO_DIR)

$(INITRAMFS): .FORCE
	rm -rf $(INITRAMFS_DIR)
	cp -rT initramfs $(INITRAMFS_DIR)
ifneq ($(RUST), 1)
	$(MAKE) -C opsh
	cp opsh/$(BUILD_DIR)/opsh.elf $(INITRAMFS_DIR)/opsh.elf
endif
	@mkdir -p $(dir $@)
	(cd $(INITRAMFS_DIR); find .) | cpio -o -H newc -D $(INITRAMFS_DIR) > $(INITRAMFS)

run: iso
	qemu-system-x86_64 $(QEMU_FLAGS) $(QEMU_HDDS) -cdrom $(ISO_FILE) -D qemu.log; \
	status=$$?; \
	if [ "$(QEMU_DEBUG_EXIT)" = "1" ]; then \
		if [ $$status -eq 33 ]; then exit 0; fi; \
		exit 1; \
	fi; \
	exit $$status

disk-images:
	qemu-img create -f qcow2 hda.img 32M
	qemu-img create -f qcow2 hdb.img 16M
	qemu-img create -f qcow2 hdd.img 8M

ifeq ($(RUST), 1)

rust-project.json:
	$(MAKE) build -C opal-ktest
	RUSTC="$(RUSTC)" RUST_TARGET_NAME="$(RUST_TARGET_NAME)" RUST_EDITION="$(RUST_EDITION)" \
		tools/gen-rust-project.sh

else # RUST

rust-project.json:
	$(error $@ requires RUST=1)

endif # RUST

clean:
	for dir in $(CLEAN_SUBDIRS); do \
		$(MAKE) clean -C $$dir || exit 1; \
	done
	$(MAKE) clean-root

clean-root:
	-rm -rf $(BUILD_DIR)

fullclean:
	for dir in $(CLEAN_SUBDIRS); do \
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
ifeq ($(RUST), 1)
	$(MAKE) run UNIT_TEST=1 QEMU_DISPNONE=1 QEMU_DEBUG_EXIT=1
else
	$(MAKE) run UNIT_TEST=1
endif

clean-unit-test:
ifeq ($(RUST), 1)
	$(MAKE) clean -C opalkrnl UNIT_TEST=1
	$(MAKE) clean -C opal-kernel UNIT_TEST=1
else
	$(MAKE) clean -C kernel UNIT_TEST=1
endif
	$(MAKE) clean-root UNIT_TEST=1
