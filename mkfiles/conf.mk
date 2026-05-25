CONFIG              ?= debug
PLATFORM            ?= pc-x64

ifeq ($(CONFIG),debug)
else ifeq ($(CONFIG),release)
else # CONFIG
$(error "Invalid build configuration: $(CONFIG). Use 'debug' or 'release'.")
endif # CONFIG

ifeq ($(PLATFORM),pc-x64)
ARCH                ?= x86_64
ifeq ($(ARCH),x86_64)
else # ARCH
$(error "Invalid architecture: $(ARCH). Supported architecture: x86_64.")
endif # ARCH
else # PLATFORM
$(error "Invalid platform: $(PLATFORM). Supported platform: pc-x64.")
endif # PLATFORM

export CONFIG
export PLATFORM
export ARCH

TEST_DIR            := tests

ifneq ($(wildcard $(TEST_DIR)/*),)
HAS_TEST            := 1
endif # TEST_DIR/*

ifeq ($(IS_TEST),1)
IS_TEST_BUILD := 1
ifneq ($(filter $(TARGET_TYPE),executable static-lib),)
TARGET_TYPE := shared-lib
else ifneq ($(TARGET_TYPE),shared-lib)
$(error "Test is unsupported for $(TARGET_TYPE) targets.")
endif # TARGET_TYPE
endif # IS_TEST

PYTHON              := python3

CFLAGS              += -std=c23 -ggdb3 -ffreestanding -masm=intel --embed-dir=res
NASM_FLAGS          += -ires
OBJDUMP_FLAGS       += -dS -M intel

WARNING_FLAGS       := -pedantic -Wall -Wextra -Werror

TEST_WARNING_FLAGS  := -Wno-unused-parameter

DEFINE_FLAGS        := -DOPAL_CONFIG=\"$(CONFIG)\" -DOPAL_PLATFORM=\"$(PLATFORM)\"

# TODO: keep libkc builtin-backed symbols live for LTO-generated libcalls.
LIBKC_BUILTIN_SYMBOLS ?= \
	memcpy memmove memset memcmp memchr \
	strlen strspn strcspn strchr strrchr \
	strcmp strncmp strcpy strcat strncat

ifneq ($(IS_TEST_BUILD),1)

BUILD_PREFIX        := build
BUILD_PREFIX_REF    := build

ifeq ($(UNIT_TEST),1)

ifeq ($(TARGET_TYPE),root)
export UNIT_TEST
else ifeq ($(TARGET_TYPE),executable)
unexport UNIT_TEST
endif # TARGET_TYPE

ifneq ($(filter $(TARGET_TYPE),executable root),)
BUILD_PREFIX        := build/unit-test
DEFINE_FLAGS        += -DOPAL_UNIT_TEST
endif # TARGET_TYPE

endif # UNIT_TEST

TOOLSET_PREFIX      ?= x86_64-elf
TOOLSET_CC          ?= $(TOOLSET_PREFIX)-gcc
TOOLSET_AR          ?= $(TOOLSET_PREFIX)-gcc-ar
TOOLSET_OBJCOPY     ?= $(TOOLSET_PREFIX)-objcopy
TOOLSET_OBJDUMP     ?= $(TOOLSET_PREFIX)-objdump
TOOLSET_NM          ?= $(TOOLSET_PREFIX)-gcc-nm
TOOLSET_GDB         ?= gdb
TOOLSET_NASM        ?= nasm

CFLAGS              += -mno-red-zone -mcmodel=kernel -mno-mmx -mno-sse -mno-sse2 $(DEFINE_FLAGS) $(WARNING_FLAGS)
LDFLAGS             += -nostdlib -Wl,--gc-sections -Wl,--fatal-warning

ifeq ($(TARGET_TYPE),executable)
COMMA := ,
LDFLAGS += $(addprefix -Wl$(COMMA)-u$(COMMA),$(LIBKC_BUILTIN_SYMBOLS))
endif # TARGET_TYPE

ifeq ($(CONFIG),debug)
ifneq ($(NO_ANALYZER),1)
CFLAGS              += -fanalyzer
endif # NO_ANALYZER
endif # CONFIG

else # IS_TEST_BUILD

DEFINE_FLAGS        += -DOPAL_TEST

BUILD_PREFIX        := build/tests
BUILD_PREFIX_REF    := build/tests
TOOLSET_CC          ?= gcc
TOOLSET_AR          ?= gcc-ar
TOOLSET_OBJCOPY     ?= objcopy
TOOLSET_OBJDUMP     ?= objdump
TOOLSET_NM          ?= gcc-nm
TOOLSET_GDB         ?= gdb
TOOLSET_NASM        ?= nasm

CFLAGS              += -fPIC $(DEFINE_FLAGS) $(WARNING_FLAGS) $(CFLAGS_ON_TEST)
LDFLAGS             += -Wl,--fatal-warning $(LDFLAGS_ON_TEST)

TEST_CXX            ?= g++
TEST_CXXFLAGS       += -std=c++23 -ggdb3 -masm=intel $(DEFINE_FLAGS) $(WARNING_FLAGS) $(TEST_WARNING_FLAGS)
TEST_LDFLAGS        += -Wl,--fatal-warning

ifeq ($(CONFIG),debug)
TEST_CXXFLAGS       += -DDEBUG -fsanitize=address,undefined -fno-omit-frame-pointer
else ifeq ($(CONFIG),release)
TEST_CXXFLAGS       += -DNDEBUG -O3 -flto=auto
endif # CONFIG

endif # IS_TEST_BUILD

ifeq ($(TARGET_TYPE),shared-lib)
ifneq ($(IS_TEST_BUILD),1)
CFLAGS              += -fPIC
endif # IS_TEST_BUILD
LDFLAGS             += -shared
endif # TARGET_TYPE

ifeq ($(CONFIG),debug)
CFLAGS              += -DDEBUG
ifneq ($(IS_TEST_BUILD),1)
CFLAGS              += -fno-omit-frame-pointer
ifneq ($(NO_SANITIZE),1)
CFLAGS              += -fsanitize=undefined
endif # NO_SANITIZE
else # IS_TEST_BUILD
CFLAGS              += -fsanitize=address,undefined -fno-omit-frame-pointer
endif # IS_TEST_BUILD
else ifeq ($(CONFIG),release)
CFLAGS              += -DNDEBUG -O3 -flto=auto
endif # CONFIG

BUILD_DIR           := $(BUILD_PREFIX)/$(PLATFORM)/$(CONFIG)
BUILD_DIR_REF       := $(BUILD_PREFIX_REF)/$(PLATFORM)/$(CONFIG)
