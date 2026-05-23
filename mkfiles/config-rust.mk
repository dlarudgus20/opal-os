SELF_DIR := $(dir $(lastword $(MAKEFILE_LIST)))
OPAL_ROOT := $(SELF_DIR)..
undefine SELF_DIR

CONFIG              ?= debug
PLATFORM            ?= pc-x64

ifeq ($(CONFIG),debug)
else ifeq ($(CONFIG),release)
else # CONFIG
$(error "Invalid build configuration: $(CONFIG). Use 'debug' or 'release'.")
endif # CONFIG

ifeq ($(PLATFORM),pc-x64)
ARCH               ?= x86_64
else # PLATFORM
$(error "Invalid platform: $(PLATFORM). Supported platform: pc-x64.")
endif # PLATFORM

ifeq ($(ARCH),x86_64)
RUST_TARGET_NAME   ?= x86_64-unknown-none
else # ARCH
$(error "Invalid architecture: $(ARCH). Supported architecture: x86_64.")
endif # ARCH

export CONFIG
export PLATFORM
export ARCH

PYTHON              := python3

TOOLSET_LD          ?= ld.lld-22
TOOLSET_OBJCOPY     ?= llvm-objcopy-22
TOOLSET_OBJDUMP     ?= llvm-objdump-22
TOOLSET_NM          ?= llvm-nm-22
TOOLSET_GDB         ?= lldb
TOOLSET_NASM        ?= nasm

RUST_TOOLCHAIN      ?= nightly-2026-04-14
RUSTC               ?= rustc +$(RUST_TOOLCHAIN)
CARGO               ?= cargo +$(RUST_TOOLCHAIN)
RUST_EDITION        ?= 2024
RUST_TARGET_JSON    ?= $(OPAL_ROOT)/arch/$(ARCH)/$(RUST_TARGET_NAME).json

NASM_FLAGS          +=
RUSTFLAGS           += -Zunstable-options \
	--target $(RUST_TARGET_JSON) --edition=$(RUST_EDITION) \
	-C link-dead-code=no
LDFLAGS             += -nostdlib --gc-sections --fatal-warnings
OBJDUMP_FLAGS       += -dS -M intel

BUILD_PREFIX        := build
BUILD_PREFIX_REF    := build

ifneq ($(TARGET_TYPE),rust-sysroot)
ifeq ($(UNIT_TEST),1)

ifeq ($(TARGET_TYPE),root)
export UNIT_TEST
else ifeq ($(TARGET_TYPE),executable)
unexport UNIT_TEST
endif # TARGET_TYPE

RUSTFLAGS           += --cfg opal_ktest
BUILD_PREFIX        := build/unit-test

endif # UNIT_TEST
endif # TARGET_TYPE

ifeq ($(CONFIG),debug)
RUSTFLAGS           += -C opt-level=0 -C debuginfo=2 -C force-frame-pointers=yes
else ifeq ($(CONFIG),release)
RUSTFLAGS           += -C opt-level=3 -C codegen-units=1 -C debuginfo=0
endif # CONFIG

BUILD_DIR           := $(BUILD_PREFIX)/$(PLATFORM)/$(CONFIG)
BUILD_DIR_REF       := $(BUILD_PREFIX_REF)/$(PLATFORM)/$(CONFIG)
