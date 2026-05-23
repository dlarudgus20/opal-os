ifeq ($(TARGET_NAME),)
$(error [rules-rust.mk] TARGET_NAME is missing.)
endif # TARGET_NAME

ifeq ($(RUST_SYSROOT_REF),)
$(error [rules-rust.mk] RUST_SYSROOT_REF is missing.)
endif # RUST_SYSROOT_REF

BASE_SRCDIR         ?= src
MORE_SRCDIRS        ?= $(BASE_SRCDIR)/arch/$(ARCH) $(BASE_SRCDIR)/platform/$(PLATFORM)
INCLUDES            += include include/arch/$(ARCH) include/platform/$(PLATFORM)

RUST_CRATE_NAME     ?= $(subst -,_,$(TARGET_NAME))
RUST_CRATE_ROOT     ?= src/lib.rs

RUST_SYSROOT_DIR    := ../$(RUST_SYSROOT_REF)/$(BUILD_DIR_REF)
RUST_SYSROOT_LIBDIR := $(RUST_SYSROOT_DIR)/lib/rustlib/$(RUST_TARGET_NAME)/lib
RUST_CORE_RLIB      := $(RUST_SYSROOT_LIBDIR)/libcore.rlib
RUST_CORE_RMETA     := $(RUST_SYSROOT_LIBDIR)/libcore.rmeta
RUST_RT_RLIB        := $(RUST_SYSROOT_LIBDIR)/libcompiler_builtins.rlib
RUST_RT_RMETA       := $(RUST_SYSROOT_LIBDIR)/libcompiler_builtins.rmeta

ORDER_REF_FILES     += $(RUST_CORE_RLIB) $(RUST_CORE_RMETA) $(RUST_RT_RLIB) $(RUST_RT_RMETA)
RUSTFLAGS           += --sysroot $(RUST_SYSROOT_DIR)
RUST_SYSROOT_RLIBS  += $(RUST_CORE_RLIB) $(RUST_RT_RLIB)

RUST_MACRO_REF_LIBS := $(foreach ref,$(RUST_MACRO_REFS), \
	../$(ref)/target/debug/lib$(subst -,_,$(ref)).so)
ORDER_REF_FILES     += $(RUST_MACRO_REF_LIBS)
RUSTFLAGS           += $(foreach ref,$(RUST_MACRO_REFS), \
	--extern $(subst -,_,$(ref))=../$(ref)/target/debug/lib$(subst -,_,$(ref)).so)

BASE_SRCDIRS_EXISTING := $(strip $(if $(wildcard $(BASE_SRCDIR)),$(BASE_SRCDIR),))
MORE_SRCDIRS_EXISTING := $(strip $(foreach dir,$(MORE_SRCDIRS),$(if $(wildcard $(dir)),$(dir),)))
BASE_PRUNE_FLAGS    := \( -path '*/arch' -o -path '*/platform' \) -prune -o

BASE_ASM_SOURCES    := $(if $(BASE_SRCDIRS_EXISTING), \
	$(shell find $(BASE_SRCDIRS_EXISTING) $(BASE_PRUNE_FLAGS) -type f -name '*.asm' -print), )
MORE_ASM_SOURCES    := $(if $(MORE_SRCDIRS_EXISTING), \
	$(shell find $(MORE_SRCDIRS_EXISTING) -type f -name '*.asm'), )
ASM_SOURCES         := $(sort $(BASE_ASM_SOURCES) $(MORE_ASM_SOURCES))

ASM_OBJECTS         := $(patsubst %.asm,$(BUILD_DIR)/%.asm.o,$(ASM_SOURCES))
ASM_DEPENDS         := $(patsubst %.o,%.d,$(ASM_OBJECTS))

RUST_OBJECT         := $(BUILD_DIR)/$(TARGET_NAME).rlib.o
RUST_METADATA       := $(BUILD_DIR)/$(TARGET_NAME).rmeta
RUST_DEPEND         := $(BUILD_DIR)/$(TARGET_NAME).rlib.d

RUST_OBJ_REF_FILES  := $(foreach ref,$(RUST_OBJ_REFS),../$(ref)/$(BUILD_DIR)/$(ref).o)
REFS                := $(strip $(RUST_OBJ_REFS) $(RUST_MACRO_REFS) $(RUST_SYSROOT_REF))

NASM_INCLUDE_FLAGS  := $(foreach inc,$(INCLUDES),-I$(inc)/)

PHONY_TARGETS += all build clean fullclean refs
.PHONY: $(PHONY_TARGETS)

all: build

ifeq ($(TARGET_TYPE),static-obj)

TARGET := $(BUILD_DIR)/$(TARGET_NAME).o

$(TARGET): $(RUST_OBJECT) $(ASM_OBJECTS)
	@mkdir -p $(dir $@)
	$(TOOLSET_LD) -r -o $@ $(RUST_OBJECT) $(ASM_OBJECTS)
	$(TOOLSET_NM) $(NM_FLAGS) $@ > $@.nm
	$(TOOLSET_OBJDUMP) $(OBJDUMP_FLAGS) $@ > $@.disasm

else ifeq ($(TARGET_TYPE),executable)

ifeq ($(LD_SCRIPT),)
$(error [rules-rust.mk] LD_SCRIPT is missing.)
endif

TARGET := $(BUILD_DIR)/$(TARGET_NAME).elf
LINK_INPUTS := $(RUST_OBJ_REF_FILES) $(RUST_SYSROOT_RLIBS)

$(TARGET): $(LINK_INPUTS) $(LD_SCRIPT)
	@mkdir -p $(dir $@)
	$(TOOLSET_LD) $(LDFLAGS) -T $(LD_SCRIPT) -Map=$@.map -o $@ $(LINK_INPUTS)
	$(TOOLSET_NM) $(NM_FLAGS) $@ > $@.nm
	$(TOOLSET_OBJDUMP) $(OBJDUMP_FLAGS) $@ > $@.disasm
	$(TOOLSET_NM) -C --numeric-sort $@ \
		| perl -p -e 's/([0-9a-fA-F]*) ([0-9a-fA-F]* .|.) ([^\s]*)(^$$|.*)/\1 \3/g' \
		> $@.sym

else
$(error [rules-rust.mk] '$(TARGET_TYPE)': unknown target type.)
endif

$(RUST_OBJ_REF_FILES) $(ORDER_REF_FILES): | refs

refs:
	for dir in $(REFS); do \
		$(MAKE) -C ../$$dir || exit 1; \
	done

$(BUILD_DIR)/%.asm.o: %.asm
	@mkdir -p $(dir $@)
	$(TOOLSET_NASM) $(NASM_FLAGS) $(NASM_INCLUDE_FLAGS) -f elf64 \
		-MD $(patsubst %.o,%.d,$@) $< -o $@ -l $(patsubst %.o,%.lst,$@)
	$(TOOLSET_OBJDUMP) $(OBJDUMP_FLAGS) $@ > $(patsubst %.o,%.dump,$@)

$(RUST_OBJECT): $(RUST_CRATE_ROOT) $(ORDER_REF_FILES)
	@mkdir -p $(dir $@)
	$(RUSTC) --crate-name $(RUST_CRATE_NAME) --crate-type rlib $(RUSTFLAGS) \
		--out-dir $(BUILD_DIR) \
		--emit=obj=$@,metadata=$(RUST_METADATA),dep-info=$(RUST_DEPEND) \
		$(RUST_CRATE_ROOT)

clean:
	-rm -rf $(BUILD_DIR)

fullclean:
	-rm -rf build

-include $(ASM_DEPENDS) $(RUST_DEPEND)
