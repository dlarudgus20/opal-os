TARGET_TYPE := rust-sysroot

all: build

include ../mkfiles/config-rust.mk

CORE_LIB_DIR    := $(BUILD_DIR)/lib/rustlib/$(RUST_TARGET_NAME)/lib
CORE_RLIB       := $(CORE_LIB_DIR)/libcore.rlib
CORE_RMETA      := $(CORE_LIB_DIR)/libcore.rmeta
RT_RLIB         := $(CORE_LIB_DIR)/libcompiler_builtins.rlib
RT_RMETA        := $(CORE_LIB_DIR)/libcompiler_builtins.rmeta
CORE_STAMP      := $(BUILD_DIR)/.rust-sysroot-built
CARGO_TARGET_DIR := $(BUILD_DIR)/cargo
CARGO_PROFILE_DIR := debug
CARGO_BUILD_FLAGS :=

ifeq ($(CONFIG), release)
CARGO_PROFILE_DIR := release
CARGO_BUILD_FLAGS := --release
endif

.PHONY: all build clean fullclean

build: $(CORE_RLIB) $(CORE_RMETA) $(RT_RLIB) $(RT_RMETA)

$(CORE_RLIB) $(CORE_RMETA) $(RT_RLIB) $(RT_RMETA): $(CORE_STAMP)

$(CORE_STAMP): Makefile Cargo.toml Cargo.lock src/lib.rs ../mkfiles/conf.mk $(RUST_TARGET_JSON)
	@mkdir -p $(CORE_LIB_DIR)
	$(CARGO) build \
		-Z build-std=core,compiler_builtins \
		-Z build-std-features=compiler-builtins-mem \
		-Z json-target-spec \
		--target $(RUST_TARGET_JSON) \
		--target-dir $(CARGO_TARGET_DIR) \
		$(CARGO_BUILD_FLAGS)
	core_rlib="$$(find "$(CARGO_TARGET_DIR)/$(RUST_TARGET_NAME)/$(CARGO_PROFILE_DIR)/deps" -maxdepth 1 -type f -name 'libcore-*.rlib' | sort | tail -n 1)"; \
	core_rmeta="$$(find "$(CARGO_TARGET_DIR)/$(RUST_TARGET_NAME)/$(CARGO_PROFILE_DIR)/deps" -maxdepth 1 -type f -name 'libcore-*.rmeta' | sort | tail -n 1)"; \
	rt_rlib="$$(find "$(CARGO_TARGET_DIR)/$(RUST_TARGET_NAME)/$(CARGO_PROFILE_DIR)/deps" -maxdepth 1 -type f -name 'libcompiler_builtins-*.rlib' | sort | tail -n 1)"; \
	rt_rmeta="$$(find "$(CARGO_TARGET_DIR)/$(RUST_TARGET_NAME)/$(CARGO_PROFILE_DIR)/deps" -maxdepth 1 -type f -name 'libcompiler_builtins-*.rmeta' | sort | tail -n 1)"; \
	test -n "$$core_rlib" || { echo "missing built libcore rlib"; exit 1; }; \
	test -n "$$core_rmeta" || { echo "missing built libcore rmeta"; exit 1; }; \
	test -n "$$rt_rlib" || { echo "missing built compiler_builtins rlib"; exit 1; }; \
	test -n "$$rt_rmeta" || { echo "missing built compiler_builtins rmeta"; exit 1; }; \
	cp "$$core_rlib" $(CORE_LIB_DIR)/$$(basename "$$core_rlib"); \
	cp "$$core_rmeta" $(CORE_LIB_DIR)/$$(basename "$$core_rmeta"); \
	cp "$$rt_rlib" $(CORE_LIB_DIR)/$$(basename "$$rt_rlib"); \
	cp "$$rt_rmeta" $(CORE_LIB_DIR)/$$(basename "$$rt_rmeta"); \
	cp "$$core_rlib" $(CORE_RLIB); \
	cp "$$core_rmeta" $(CORE_RMETA); \
	cp "$$rt_rlib" $(RT_RLIB); \
	cp "$$rt_rmeta" $(RT_RMETA)
	@touch $@

clean:

fullclean:
	-rm -rf build
