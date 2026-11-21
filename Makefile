include mkfiles/config.mk

KERNEL_BIN  := kernel/$(BUILD_DIR)/kernel.sys
GRUB_CONFIG := iso/boot/grub/grub.cfg
ISO_DIR     := $(BUILD_DIR)/iso
ISO_FILE    := $(BUILD_DIR)/opal-os.iso

QEMU_FLAGS  += -m 128 -serial stdio -no-reboot -boot order=dc
QEMU_HDDS   := $(foreach disk,$(wildcard hda.img hdb.img hdd.img),-$(basename $(disk)) $(disk))

ifeq ($(UEFI),1)
UEFI_FIRMWARE ?= /usr/share/ovmf/OVMF.fd
QEMU_FLAGS    += -bios $(UEFI_FIRMWARE)
endif # UEFI
ifeq ($(QEMU_DISPNONE),1)
QEMU_FLAGS  += -display none
endif # QEMU_DISPNONE

SUBDIRS     := kernel libkc libkubsan

.DEFAULT_GOAL := all
.PHONY: all build FORCE iso run disk-images gen clean-gen clean fullclean test clean-test
.NOTPARALLEL:

all: build

build: $(KERNEL_BIN)

$(KERNEL_BIN): FORCE
	$(MAKE) -C kernel build

iso: $(ISO_FILE)

$(ISO_FILE): $(KERNEL_BIN) $(GRUB_CONFIG) Makefile
	rm -rf $(ISO_DIR)
	@mkdir -p $(ISO_DIR)/boot/grub
	cp $(GRUB_CONFIG) $(ISO_DIR)/boot/grub/grub.cfg
	cp $(KERNEL_BIN) $(ISO_DIR)/boot/kernel.sys
	grub-mkrescue -o $@ $(ISO_DIR)

run: iso
	qemu-system-x86_64 $(QEMU_FLAGS) $(QEMU_HDDS) -cdrom $(ISO_FILE) -D qemu.log

disk-images:
	qemu-img create -f qcow2 hda.img 32M
	qemu-img create -f qcow2 hdb.img 16M
	qemu-img create -f qcow2 hdd.img 8M

gen clean-gen:
	for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir $@ || exit 1; \
	done

clean fullclean:
	for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir $@ || exit 1; \
	done
	-rm -rf $(if $(filter clean,$@),$(BUILD_DIR),build)

test:
	$(MAKE) run TEST=1

clean-test:
	$(MAKE) clean TEST=1
