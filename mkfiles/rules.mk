SOURCE_DIRS        := src platform/$(PLATFORM)/src
C_SOURCES          := $(sort $(shell find $(SOURCE_DIRS) -type f -name '*.c'))
ASM_SOURCES        := $(sort $(shell find $(SOURCE_DIRS) -type f -name '*.asm'))

C_OBJECTS          := $(patsubst %.c, $(BUILD_DIR)/%.c.o, $(C_SOURCES))
ASM_OBJECT         := $(patsubst %.asm, $(BUILD_DIR)/%.asm.o, $(ASM_SOURCES))
KERNEL_BIN         := $(BUILD_DIR)/kernel.bin

ifeq ($(TARGET_TYPE), )
ifneq ($(TARGET_NAME), )
TARGET_TYPE := executable
endif
else
ifeq ($(TARGET_NAME), )
$(error [rules.mk] TARGET_NAME is missing.)
endif
endif

ifeq ($(TARGET_TYPE), executable)
ifeq ($(LD_SCRIPT), )
$(error [rules.mk] LD_SCRIPT is missing.)
endif
TARGET := $(BUILD_DIR)/$(TARGET_NAME).elf
else
$(error [rules.mk] '$(TARGET_TYPE)': unknown target type.)
endif

PHONY_TARGETS += all clean
.PHONY: $(PHONY_TARGETS)

$(BUILD_DIR)/%.c.o: %.c
	@mkdir -p $(dir $@)
	$(TOOLSET_CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.asm.o: %.asm
	@mkdir -p $(dir $@)
	$(TOOLSET_NASM) -f elf64 $< -o $@

$(TARGET): $(C_OBJECTS) $(ASM_OBJECT) $(LD_SCRIPT)
	$(TOOLSET_CC) $(CFLAGS) $(LDFLAGS) -T $(LD_SCRIPT) -o $@ $(ASM_OBJECT) $(C_OBJECTS)

clean:
	rm -rf $(BUILD_DIR)
