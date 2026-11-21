.DEFAULT_GOAL := all
.PHONY: all build gen clean-gen clean fullclean FORCE

SOURCE_DIRS         := src platform/$(PLATFORM)/src
ifeq ($(TEST),1)
SOURCE_DIRS         += tests platform/$(PLATFORM)/tests
endif # TEST

SOURCE_PATHS        := $(wildcard $(SOURCE_DIRS))
C_SOURCES           := $(sort $(shell find $(SOURCE_PATHS) -type f -name '*.c'))
ASM_SOURCES         := $(sort $(shell find $(SOURCE_PATHS) -type f -name '*.asm'))
OBJECTS             := $(patsubst %,$(BUILD_DIR)/%.o,$(ASM_SOURCES) $(C_SOURCES))
BUILD_RULES         := $(MAKEFILE_LIST)
REFS_STATIC_FILES   := $(foreach ref,$(STATIC_REFS),../$(ref)/$(BUILD_DIR_REF)/$(ref).a)
REFS_INCS           := $(foreach ref,$(STATIC_REFS) $(INCLUDE_REFS),../$(ref)/include ../$(ref)/platform/$(PLATFORM)/include)
INCLUDE_FLAGS       := -Iinclude -Iplatform/$(PLATFORM)/include $(addprefix -I,$(INCLUDES) $(REFS_INCS))

all: build

-include $(OBJECTS:.o=.d)

$(BUILD_DIR)/%.c.o: %.c $(BUILD_RULES)
	@mkdir -p $(dir $@)
	$(TOOLSET_CC) $(CFLAGS) $(INCLUDE_FLAGS) -MMD -MP -MF $(@:.o=.d) -c $< -o $@
	$(TOOLSET_OBJDUMP) $(OBJDUMP_FLAGS) $@ > $(@:.o=.dump)

$(BUILD_DIR)/%.asm.o: %.asm $(BUILD_RULES)
	@mkdir -p $(dir $@)
	$(TOOLSET_NASM) $(NASM_FLAGS) -f elf64 -MD $(@:.o=.d) -MP $< -o $@ -l $(@:.o=.lst)
	$(TOOLSET_OBJDUMP) $(OBJDUMP_FLAGS) $@ > $(@:.o=.dump)

ifneq ($(REFS_STATIC_FILES),)
$(REFS_STATIC_FILES): FORCE
	$(MAKE) -C ../$(basename $(notdir $@)) build TEST=0
endif # REFS_STATIC_FILES

ifeq ($(TARGET_TYPE),executable)
ifeq ($(LD_SCRIPT),)
$(error [rules.mk] LD_SCRIPT is missing for $(TARGET_NAME))
endif # LD_SCRIPT
TARGET              := $(BUILD_DIR)/$(TARGET_NAME).elf

$(TARGET): $(OBJECTS) $(REFS_STATIC_FILES) $(LD_SCRIPT) $(BUILD_RULES)
	$(TOOLSET_CC) $(CFLAGS) $(LDFLAGS) -T $(LD_SCRIPT) -o $@ $(OBJECTS) $(REFS_STATIC_FILES) -Wl,-Map,$@.map
	$(TOOLSET_NM) $(NM_FLAGS) $@ > $@.nm
	$(TOOLSET_OBJDUMP) $(OBJDUMP_FLAGS) $@ > $@.disasm
	$(TOOLSET_NM) -C --numeric-sort $@ | awk 'NF == 3 { print $$1, $$3 }' > $@.sym
else ifeq ($(TARGET_TYPE),static-lib)
TARGET              := $(BUILD_DIR)/$(TARGET_NAME).a

$(TARGET): $(OBJECTS) $(BUILD_RULES)
	rm -f $@
	$(TOOLSET_AR) rcs $@ $(OBJECTS)
	$(TOOLSET_NM) $(NM_FLAGS) $@ > $@.nm
else # TARGET_TYPE
$(error [rules.mk] Invalid TARGET_TYPE: $(TARGET_TYPE))
endif # TARGET_TYPE

build: $(TARGET)

gen: $(RESOURCES)

clean-gen:
	rm -rf res/gen

clean: clean-gen
	rm -rf $(BUILD_DIR)

fullclean: clean-gen
	rm -rf build
