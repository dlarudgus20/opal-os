SOURCE_DIRS        := src platform/$(PLATFORM)/src
STRIPPED_SRCDIRS   := $(strip $(foreach dir, $(SOURCE_DIRS), $(if $(wildcard $(dir)), $(dir), )))
C_SOURCES          := $(sort $(if $(STRIPPED_SRCDIRS), $(shell find $(STRIPPED_SRCDIRS) -type f -name '*.c'), ))
ASM_SOURCES        := $(sort $(if $(STRIPPED_SRCDIRS), $(shell find $(STRIPPED_SRCDIRS) -type f -name '*.asm'), ))

C_OBJECTS          := $(patsubst %.c, $(BUILD_DIR)/%.c.o, $(C_SOURCES))
C_DEPENDS          := $(patsubst %.o, %.d, $(C_OBJECTS))
ASM_OBJECT         := $(patsubst %.asm, $(BUILD_DIR)/%.asm.o, $(ASM_SOURCES))
ASM_DEPENDS        := $(patsubst %.o, %.d, $(ASM_OBJECT))

ifeq ($(IS_TEST_BUILD), 1)
TEST_SOURCES       := $(wildcard $(TEST_DIR)/*.cpp)
TEST_OBJECTS       := $(patsubst %.cpp, $(BUILD_DIR)/%.cpp.o, $(TEST_SOURCES))
TEST_DEPENDS       := $(patsubst %.o, %.d, $(TEST_OBJECTS))
TEST_EXECUTABLE    := $(BUILD_DIR)/test
STATIC_REFS        += $(TEST_STATIC_REFS)
SHARED_REFS        += $(TEST_SHARED_REFS)
endif

REFS_STATIC_FILES  := $(foreach ref, $(STATIC_REFS), ../$(ref)/$(BUILD_DIR)/$(ref).a)
REFS_SHARED_FILES  := $(foreach ref, $(SHARED_REFS), ../$(ref)/$(BUILD_DIR)/$(ref).so)
REFS_INCS          := $(foreach ref, $(STATIC_REFS) $(SHARED_REFS), ../$(ref)/include ../$(ref)/platform/$(PLATFORM)/include)

LIBRARIES          += $(REFS_STATIC_FILES) $(REFS_SHARED_FILES)
INCLUDES           += $(REFS_INCS)
INCLUDE_FLAGS      := -Iinclude -Iplatform/$(PLATFORM)/include $(foreach inc, $(INCLUDES), -I$(inc))

ifeq ($(TARGET_NAME), )
$(error [rules.mk] TARGET_NAME is missing.)
endif

# executable
ifeq ($(TARGET_TYPE), executable)

ifeq ($(LD_SCRIPT), )
$(error [rules.mk] LD_SCRIPT is missing.)
endif
TARGET := $(BUILD_DIR)/$(TARGET_NAME).elf
LD_SCRIPT_FLAG := -T $(LD_SCRIPT)

# static-lib
else ifeq ($(TARGET_TYPE), static-lib)

TARGET := $(BUILD_DIR)/$(TARGET_NAME).a
TEST_TARGET_LINK := $(TARGET)

ifneq ($(TEST_WITH_REDEF_SYMBOLS), )
TARGET_REDEF := $(BUILD_DIR)/$(TARGET_NAME)_redef.a
TEST_TARGET_LINK := $(TARGET_REDEF)
endif

# shared-lib
else ifeq ($(TARGET_TYPE), shared-lib)

TARGET := $(BUILD_DIR)/$(TARGET_NAME).so
LD_SCRIPT_FLAG :=
ifeq ($(TEST_AS_SHARED) $(TEST_DO_NOT_LINK), 1 1)
TEST_TARGET_LINK := -ldl
else
TEST_TARGET_LINK := $(TARGET)
endif

else
$(error [rules.mk] '$(TARGET_TYPE)': unknown target type.)
endif

-include $(C_DEPENDS) $(ASM_DEPENDS)
ifeq ($(IS_TEST_BUILD), 1)
-include $(TEST_DEPENDS)
endif

PHONY_TARGETS += all clean build-test test clean-test
.PHONY: $(PHONY_TARGETS) .FORCE
.FORCE:

$(BUILD_DIR)/%.c.o: %.c
	@mkdir -p $(dir $@)
	$(TOOLSET_CC) $(CFLAGS) $(INCLUDE_FLAGS) -MMD -MP -MF $(patsubst %.o, %.d, $@) -c $< -o $@
	$(TOOLSET_OBJDUMP) $(OBJDUMP_FLAGS) -D $@ > $(patsubst %.o, %.dump, $@)

$(BUILD_DIR)/%.asm.o: %.asm
	@mkdir -p $(dir $@)
	$(TOOLSET_NASM) -f elf64 -MD $(patsubst %.o, %.d, $@) $< -o $@ -l $(patsubst %.o, %.lst, $@)
	$(TOOLSET_OBJDUMP) $(OBJDUMP_FLAGS) -D $@ > $(patsubst %.o, %.dump, $@)

ifneq ($(findstring $(TARGET_TYPE), executable shared-lib), )
$(TARGET): $(C_OBJECTS) $(ASM_OBJECT) $(LD_SCRIPT) $(LIBRARIES)
	$(TOOLSET_CC) $(CFLAGS) $(INCLUDE_FLAGS) $(LDFLAGS) $(LD_SCRIPT_FLAG) -o $@ $(ASM_OBJECT) $(C_OBJECTS) $(LIBRARIES) \
		-Wl,-Map,$@.map
	$(TOOLSET_NM) $(NM_FLAGS) $@ > $@.nm
	$(TOOLSET_OBJDUMP) $(OBJDUMP_FLAGS) -D $@ > $@.disasm
	$(TOOLSET_NM) -C --numeric-sort $@ \
		| perl -p -e 's/([0-9a-fA-F]*) ([0-9a-fA-F]* .|.) ([^\s]*)(^$$|.*)/\1 \3/g' \
		> $@.sym
else ifeq ($(TARGET_TYPE), static-lib)
$(TARGET): $(C_OBJECTS) $(ASM_OBJECT)
	$(TOOLSET_AR) rcs $@ $^
	$(TOOLSET_NM) $(NM_FLAGS) $@ > $@.nm
endif

ifeq ($(IS_TEST_BUILD), 1)
$(BUILD_DIR)/%.cpp.o: %.cpp
	@mkdir -p $(dir $@)
	$(TEST_CXX) $(TEST_CXXFLAGS) $(INCLUDE_FLAGS) -MMD -MP -MF $(patsubst %.o, %.d, $@) -c $< -o $@
	$(TOOLSET_OBJDUMP) $(OBJDUMP_FLAGS) -D $@ > $(patsubst %.o, %.dump, $@)

$(TEST_EXECUTABLE): $(TEST_OBJECTS) $(TARGET) $(LIBRARIES) $(TEST_WITH_REDEF_SYMBOLS)
ifeq ($(TARGET_TYPE), static-lib)
ifneq ($(TEST_WITH_REDEF_SYMBOLS), )
	$(TOOLSET_OBJCOPY) $(patsubst %, --redefine-syms %, $(TEST_WITH_REDEF_SYMBOLS)) $(TARGET) $(TARGET_REDEF)
endif
endif
	$(TEST_CXX) $(TEST_CXXFLAGS) $(INCLUDE_FLAGS) $(TEST_LDFLAGS) -o $@ $(TEST_OBJECTS) $(TEST_TARGET_LINK) $(LIBRARIES) -lgtest -lgtest_main \
		-Wl,-Map,$(BUILD_DIR)/test.map
	$(TOOLSET_NM) $(NM_FLAGS) $@ > $(BUILD_DIR)/test.nm
	$(TOOLSET_OBJDUMP) $(OBJDUMP_FLAGS) -D $@ > $(BUILD_DIR)/test.disasm
endif

$(REFS_STATIC_FILES) $(REFS_SHARED_FILES): .FORCE
	for dir in $(STATIC_REFS) $(SHARED_REFS); do \
		$(MAKE) build -C ../$$dir IS_TEST_BUILD=$(IS_TEST) IS_TEST= || exit 1; \
	done

clean:
	rm -rf $(BUILD_DIR)

ifeq ($(HAS_TEST), 1)
ifeq ($(IS_TEST), 1)
build-test: $(TEST_EXECUTABLE)

test: $(TEST_EXECUTABLE)
	./$(TEST_EXECUTABLE)

clean-test: clean
else
build-test:
	$(MAKE) build-test IS_TEST=1

test: build-test
	$(MAKE) test IS_TEST=1

clean-test:
	$(MAKE) clean IS_TEST=1
endif
else
build-test:

test:

clean-test:
endif
