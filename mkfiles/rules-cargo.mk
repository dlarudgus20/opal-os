ifeq ($(TARGET_NAME), )
$(error [rules-cargo.mk] TARGET_NAME is missing.)
endif

ifeq ($(CARGO_TARGET), )
$(error [rules-cargo.mk] CARGO_TARGET is missing.)
endif

CARGO_BIN_NAME ?= $(TARGET_NAME)

ifeq ($(CONFIG), release)
CARGO_BUILD_FLAGS += --release
endif

CARGO_PROFILE_DIR := target/$(CARGO_TARGET)/$(CONFIG)

CARGO_ELF := $(CARGO_PROFILE_DIR)/$(CARGO_BIN_NAME)
TARGET_ELF := $(BUILD_DIR)/$(TARGET_NAME).elf

PHONY_TARGETS += .FORCE all build clean fullclean unit-test
.PHONY: $(PHONY_TARGETS)
.FORCE:

$(CARGO_ELF): .FORCE
	TOOLSET_NASM="$(TOOLSET_NASM)" \
	TOOLSET_AR="$(TOOLSET_AR)" \
	cargo build $(CARGO_BUILD_FLAGS)

$(TARGET_ELF): $(CARGO_ELF)
	@mkdir -p $(dir $@)
	cp $< $@
	$(TOOLSET_NM) $(NM_FLAGS) $@ > $@.nm
	$(TOOLSET_OBJDUMP) $(OBJDUMP_FLAGS) $@ > $@.disasm
	$(TOOLSET_NM) -C --numeric-sort $@ \
		| perl -p -e 's/([0-9a-fA-F]*) ([0-9a-fA-F]* .|.) ([^\s]*)(^$$|.*)/\1 \3/g' \
		> $@.sym

clean:
	-rm -rf $(BUILD_DIR) $(CARGO_PROFILE_DIR)

fullclean:
	-rm -rf build target

unit-test: build
	@echo "Rust kernel #[test] unit-test runner is planned for rust porting stage 2."
