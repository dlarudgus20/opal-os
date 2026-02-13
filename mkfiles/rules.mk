SOURCE_DIRS        := src platform/$(PLATFORM)/src
STRIPPED_SRCDIRS   := $(strip $(foreach dir, $(SOURCE_DIRS), $(if $(wildcard $(dir)), $(dir), )))
C_SOURCES          := $(sort $(if $(STRIPPED_SRCDIRS), $(shell find $(STRIPPED_SRCDIRS) -type f -name '*.c'), ))
ASM_SOURCES        := $(sort $(if $(STRIPPED_SRCDIRS), $(shell find $(STRIPPED_SRCDIRS) -type f -name '*.asm'), ))

C_OBJECTS          := $(patsubst %.c, $(BUILD_DIR)/%.c.o, $(C_SOURCES))
C_DEPENDS          := $(patsubst %.o, %.d, $(C_OBJECTS))
ASM_OBJECT         := $(patsubst %.asm, $(BUILD_DIR)/%.asm.o, $(ASM_SOURCES))
ASM_DEPENDS        := $(patsubst %.o, %.d, $(ASM_OBJECT))

REFS_LIBS          := $(foreach ref, $(PROJECT_REFS), ../$(ref)/$(BUILD_DIR)/$(ref).a)
REFS_INCS          := $(foreach ref, $(PROJECT_REFS), ../$(ref)/include ../$(ref)/platform/$(PLATFORM)/include)

LIBRARIES          += $(REFS_LIBS)
INCLUDES           += $(REFS_INCS)
INCLUDE_FLAGS      := -Iinclude -Iplatform/$(PLATFORM)/include $(foreach inc, $(INCLUDES), -I$(inc))

ifeq ($(TARGET_NAME), )
$(error [rules.mk] TARGET_NAME is missing.)
endif

ifeq ($(TARGET_TYPE), executable)
ifeq ($(LD_SCRIPT), )
$(error [rules.mk] LD_SCRIPT is missing.)
endif
TARGET := $(BUILD_DIR)/$(TARGET_NAME).elf
else ifeq ($(TARGET_TYPE), static-lib)
TARGET := $(BUILD_DIR)/$(TARGET_NAME).a
else
$(error [rules.mk] '$(TARGET_TYPE)': unknown target type.)
endif

-include $(C_DEPENDS) $(ASM_DEPENDS)

PHONY_TARGETS += all clean
.PHONY: $(PHONY_TARGETS) .FORCE
.FORCE:

$(BUILD_DIR)/%.c.o: %.c
	@mkdir -p $(dir $@)
	$(TOOLSET_CC) $(CFLAGS) $(INCLUDE_FLAGS) -MMD -MP -MF $(patsubst %.o, %.d, $@) -c $< -o $@

$(BUILD_DIR)/%.asm.o: %.asm
	@mkdir -p $(dir $@)
	$(TOOLSET_NASM) -f elf64 -MD $(patsubst %.o, %.d, $@) $< -o $@

ifeq ($(TARGET_TYPE), executable)
$(TARGET): $(C_OBJECTS) $(ASM_OBJECT) $(LD_SCRIPT) $(LIBRARIES)
	$(TOOLSET_CC) $(CFLAGS) $(INCLUDE_FLAGS) $(LDFLAGS) -T $(LD_SCRIPT) -o $@ $(ASM_OBJECT) $(C_OBJECTS) $(LIBRARIES)
else ifeq ($(TARGET_TYPE), static-lib)
$(TARGET): $(C_OBJECTS) $(ASM_OBJECT)
	$(TOOLSET_AR) rcs $@ $^
endif

$(REFS_LIBS): .FORCE
	for dir in $(PROJECT_REFS); do \
		$(MAKE) build -C ../$$dir CONFIG=$(CONFIG) PLATFORM=$(PLATFORM) || exit 1; \
	done

clean:
	rm -rf $(BUILD_DIR)
