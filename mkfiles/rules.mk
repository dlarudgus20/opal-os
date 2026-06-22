SOURCE_DIRS         := src platform/$(PLATFORM)/src

ifeq ($(UNIT_TEST),1)
SOURCE_DIRS         += unit-tests platform/$(PLATFORM)/unit-tests
endif # UNIT_TEST

STRIPPED_SRCDIRS    := $(strip $(foreach dir,$(SOURCE_DIRS),$(if $(wildcard $(dir)),$(dir),)))
C_SOURCES           := $(sort $(if $(STRIPPED_SRCDIRS),$(shell find $(STRIPPED_SRCDIRS) -type f -name '*.c'),))
ASM_SOURCES         := $(sort $(if $(STRIPPED_SRCDIRS),$(shell find $(STRIPPED_SRCDIRS) -type f -name '*.asm'),))

C_OBJECTS           := $(patsubst %.c,$(BUILD_DIR)/%.c.o,$(C_SOURCES))
C_DEPENDS           := $(patsubst %.o,%.d,$(C_OBJECTS))
ASM_OBJECTS         := $(patsubst %.asm,$(BUILD_DIR)/%.asm.o,$(ASM_SOURCES))
ASM_DEPENDS         := $(patsubst %.o,%.d,$(ASM_OBJECTS))

OBJECTS             += $(ASM_OBJECTS) $(C_OBJECTS)

ifeq ($(IS_TEST_BUILD),1)
STRIPPED_TESTDIR    := $(strip $(foreach dir,$(TEST_DIR),$(if $(wildcard $(dir)),$(dir),)))
TEST_SOURCES        := $(sort $(if $(STRIPPED_TESTDIR),$(shell find $(STRIPPED_TESTDIR) -type f -name '*.cpp'),))
TEST_OBJECTS        := $(patsubst %.cpp,$(BUILD_DIR)/%.cpp.o,$(TEST_SOURCES))
TEST_DEPENDS        := $(patsubst %.o,%.d,$(TEST_OBJECTS))
TEST_EXECUTABLE     := $(BUILD_DIR)/test
OBJECTS             := $(filter-out $(TEST_EXCLUDE_OBJ),$(OBJECTS))
STATIC_REFS         += $(TEST_STATIC_REFS)
SHARED_REFS         += $(TEST_SHARED_REFS)
INCLUDE_REFS        += $(TEST_INCLUDE_REFS)

TEST_PCH_DIR        := ../test-pch
TEST_PCH_FILE       := test-pch.h
TEST_PCH_SRC        := $(wildcard $(TEST_PCH_DIR)/include/$(TEST_PCH_FILE))
ifneq ($(TEST_PCH_SRC),)
TEST_PCH            := $(TEST_PCH_DIR)/build/$(PLATFORM)/$(TEST_PCH_FILE).gch
TEST_INCLUDE_FLAGS  += -iquote $(TEST_PCH_DIR)/build/$(PLATFORM)/ -iquote $(TEST_PCH_DIR)/include/
else # TEST_PCH_SRC
TEST_PCH            :=
endif # TEST_PCH_SRC
endif # IS_TEST_BUILD

REFS_STATIC_FILES   := $(foreach ref,$(STATIC_REFS),../$(ref)/$(BUILD_DIR_REF)/$(ref).a)
REFS_SHARED_FILES   := $(foreach ref,$(SHARED_REFS),../$(ref)/$(BUILD_DIR_REF)/$(ref).so)
REFS_INCS           := $(foreach ref,$(STATIC_REFS) $(SHARED_REFS) $(INCLUDE_REFS),../$(ref)/include ../$(ref)/platform/$(PLATFORM)/include)

LIBRARIES           += $(REFS_STATIC_FILES) $(REFS_SHARED_FILES)
INCLUDES            += $(REFS_INCS)
INCLUDE_FLAGS       := -Iinclude -Iplatform/$(PLATFORM)/include $(foreach inc,$(INCLUDES),-I$(inc))

ifeq ($(TARGET_NAME),)
$(error [rules.mk] TARGET_NAME is missing.)
endif # TARGET_NAME

# executable
ifeq ($(TARGET_TYPE),executable)

ifeq ($(LD_SCRIPT),)
$(error [rules.mk] LD_SCRIPT is missing.)
endif # LD_SCRIPT
TARGET := $(BUILD_DIR)/$(TARGET_NAME).elf
LD_SCRIPT_FLAG := -T $(LD_SCRIPT)

# static-lib
else ifeq ($(TARGET_TYPE),static-lib)

TARGET := $(BUILD_DIR)/$(TARGET_NAME).a
TEST_TARGET_LIBS := $(TARGET)

ifneq ($(TEST_WITH_REDEF_SYMBOLS),)
TARGET_REDEF := $(BUILD_DIR)/$(TARGET_NAME)_redef.a
TEST_TARGET_LIBS := $(TARGET_REDEF)
endif # TEST_WITH_REDEF_SYMBOLS

# shared-lib
else ifeq ($(TARGET_TYPE),shared-lib)

TARGET := $(BUILD_DIR)/$(TARGET_NAME).so
LD_SCRIPT_FLAG :=
ifeq ($(TEST_DO_NOT_LINK),1)
TEST_TARGET_LIBS := -ldl
else # TEST_DO_NOT_LINK
TEST_TARGET_LIBS := $(TARGET)
endif # TEST_DO_NOT_LINK

else # TARGET_TYPE
$(error [rules.mk] '$(TARGET_TYPE)': unknown target type.)
endif # TARGET_TYPE

-include $(C_DEPENDS) $(ASM_DEPENDS)
ifeq ($(IS_TEST_BUILD),1)
-include $(TEST_DEPENDS)
endif # IS_TEST_BUILD

PHONY_TARGETS += .FORCE all clean fullclean gen clean-gen build-test test clean-test
.PHONY: $(PHONY_TARGETS)
.FORCE:

$(BUILD_DIR)/%.c.o: %.c
	@mkdir -p $(dir $@)
	$(TOOLSET_CC) $(CFLAGS) $(INCLUDE_FLAGS) -MMD -MP -MF $(patsubst %.o,%.d,$@) -c $< -o $@
	$(TOOLSET_OBJDUMP) $(OBJDUMP_FLAGS) $@ > $(patsubst %.o,%.dump,$@)

$(BUILD_DIR)/%.asm.o: %.asm
	@mkdir -p $(dir $@)
	$(TOOLSET_NASM) $(NASM_FLAGS) -f elf64 -MD $(patsubst %.o,%.d,$@) $< -o $@ -l $(patsubst %.o,%.lst,$@)
	$(TOOLSET_OBJDUMP) $(OBJDUMP_FLAGS) $@ > $(patsubst %.o,%.dump,$@)

ifneq ($(findstring $(TARGET_TYPE),executable shared-lib),)
$(TARGET): $(OBJECTS) $(LD_SCRIPT) $(LIBRARIES)
	$(TOOLSET_CC) $(CFLAGS) $(INCLUDE_FLAGS) $(LDFLAGS) $(LD_SCRIPT_FLAG) -o $@ $(OBJECTS) $(LIBRARIES) \
		-Wl,-Map,$@.map
	$(TOOLSET_NM) $(NM_FLAGS) $@ > $@.nm
	$(TOOLSET_OBJDUMP) $(OBJDUMP_FLAGS) $@ > $@.disasm
	$(TOOLSET_NM) -C --numeric-sort $@ \
		| perl -p -e 's/([0-9a-fA-F]*) ([0-9a-fA-F]* .|.) ([^\s]*)(^$$|.*)/\1 \3/g' \
		> $@.sym
else ifeq ($(TARGET_TYPE),static-lib)
$(TARGET): $(OBJECTS)
	$(TOOLSET_AR) rcs $@ $^
	$(TOOLSET_NM) $(NM_FLAGS) $@ > $@.nm
endif # TARGET_TYPE

ifeq ($(IS_TEST_BUILD),1)
$(BUILD_DIR)/%.cpp.o: %.cpp $(TEST_PCH)
	@mkdir -p $(dir $@)
	$(TEST_CXX) $(TEST_CXXFLAGS) $(INCLUDE_FLAGS) $(TEST_INCLUDE_FLAGS) -MMD -MP -MF $(patsubst %.o,%.d,$@) -c $< -o $@
	$(TOOLSET_OBJDUMP) $(OBJDUMP_FLAGS) $@ > $(patsubst %.o,%.dump,$@)

ifneq ($(TEST_PCH_SRC),)
$(TEST_PCH): $(TEST_PCH_SRC)
	make -C $(TEST_PCH_DIR)
endif # TEST_PCH_SRC

$(TEST_EXECUTABLE): $(TEST_OBJECTS) $(TARGET) $(REFS_SHARED_FILES) $(TEST_WITH_REDEF_SYMBOLS)
ifeq ($(TARGET_TYPE),static-lib)
ifneq ($(TEST_WITH_REDEF_SYMBOLS),)
	$(TOOLSET_OBJCOPY) $(patsubst %,--redefine-syms %,$(TEST_WITH_REDEF_SYMBOLS)) $(TARGET) $(TARGET_REDEF)
endif # TEST_WITH_REDEF_SYMBOLS
endif # TARGET_TYPE
	$(TEST_CXX) $(TEST_CXXFLAGS) $(INCLUDE_FLAGS) $(TEST_INCLUDE_FLAGS) $(TEST_LDFLAGS) -o $@ \
		$(TEST_OBJECTS) $(TEST_TARGET_LIBS) $(REFS_SHARED_FILES) -lgtest -lgtest_main \
		-Wl,-Map,$(BUILD_DIR)/test.map
	$(TOOLSET_NM) $(NM_FLAGS) $@ > $(BUILD_DIR)/test.nm
endif # IS_TEST_BUILD

$(REFS_STATIC_FILES) $(REFS_SHARED_FILES) &: .FORCE
	for dir in $(STATIC_REFS) $(SHARED_REFS); do \
		$(MAKE) build -C ../$$dir IS_TEST_BUILD=$(IS_TEST) IS_TEST= || exit 1; \
	done

clean:
	-rm -rf $(BUILD_DIR)
	-rm -rf res/gen

fullclean:
	-rm -rf build
	-rm -rf res/gen

gen: $(RESOURCES)

clean-gen:
	-rm -rf res/gen

ifeq ($(HAS_TEST),1)
ifeq ($(IS_TEST),1)
build-test: $(TEST_EXECUTABLE)

test: $(TEST_EXECUTABLE)
	./$(TEST_EXECUTABLE)
else # IS_TEST
build-test:
	$(MAKE) build-test IS_TEST=1

test: build-test
	$(MAKE) test IS_TEST=1
endif # IS_TEST
else # HAS_TEST
build-test:

test:
endif # HAS_TEST

ifeq ($(IS_TEST_BUILD),1)
clean-test: clean
else # IS_TEST_BUILD
clean-test:
	$(MAKE) clean IS_TEST=1
endif # IS_TEST_BUILD
