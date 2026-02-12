BUILD_DIR := build
ISO_DIR   := $(BUILD_DIR)/iso
ISO_FILE  := $(BUILD_DIR)/opal-os.iso

.PHONY: all kernel iso run clean

all: kernel

kernel:
	$(MAKE) -C kernel all

iso: kernel
	@mkdir -p $(ISO_DIR)/boot/grub
	cp kernel/build/kernel.bin $(ISO_DIR)/boot/kernel.bin
	cp grub/grub.cfg $(ISO_DIR)/boot/grub/grub.cfg
	grub-mkrescue -o $(ISO_FILE) $(ISO_DIR)

run: iso
	qemu-system-x86_64 -cdrom $(ISO_FILE) -serial stdio

clean:
	$(MAKE) -C kernel clean
	rm -rf $(BUILD_DIR)
