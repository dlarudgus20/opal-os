CONFIG              ?= debug
PLATFORM            ?= pc-x64
ARCH                ?= x86_64

ifeq ($(CONFIG),debug)
else ifneq ($(CONFIG),release)
$(error Invalid build configuration: $(CONFIG). Use debug or release)
endif
ifneq ($(PLATFORM),pc-x64)
$(error Invalid platform: $(PLATFORM). Use pc-x64)
endif
ifneq ($(ARCH),x86_64)
$(error Invalid architecture: $(ARCH). Use x86_64)
endif

export CONFIG PLATFORM ARCH TEST

BUILD_PREFIX        := build
ifeq ($(TEST),1)
BUILD_PREFIX        := build/test
endif # TEST
BUILD_DIR           := $(BUILD_PREFIX)/$(PLATFORM)/$(CONFIG)
BUILD_DIR_REF       := build/$(PLATFORM)/$(CONFIG)

TOOLSET_PREFIX      ?= x86_64-elf
TOOLSET_CC          ?= $(TOOLSET_PREFIX)-gcc
TOOLSET_AR          ?= $(TOOLSET_PREFIX)-gcc-ar
TOOLSET_OBJCOPY     ?= $(TOOLSET_PREFIX)-objcopy
TOOLSET_OBJDUMP     ?= $(TOOLSET_PREFIX)-objdump
TOOLSET_NM          ?= $(TOOLSET_PREFIX)-gcc-nm
TOOLSET_NASM        ?= nasm

PYTHON              := python3

CFLAGS              += -std=c23 -ggdb3 -ffreestanding -masm=intel --embed-dir=res
CFLAGS              += -mno-red-zone -mcmodel=kernel
CFLAGS              += -mno-mmx -mno-sse -mno-sse2 -pedantic -Wall -Wextra -Werror
CFLAGS              += -DOPAL_CONFIG=\"$(CONFIG)\" -DOPAL_PLATFORM=\"$(PLATFORM)\"
LDFLAGS             += -nostdlib -Wl,--gc-sections -Wl,--fatal-warnings
NASM_FLAGS          += -ires
OBJDUMP_FLAGS       += -dS -M intel

ifeq ($(TARGET_TYPE),executable)
ifeq ($(TEST),1)
CFLAGS              += -DOPAL_TEST
endif # TEST

# Keep libkc symbols available for compiler-generated calls during LTO.
LIBKC_BUILTIN_SYMBOLS ?= \
	memcpy memmove memset memcmp memchr \
	strlen strspn strcspn strchr strrchr \
	strcmp strncmp strcpy strcat strncat
COMMA               := ,
LDFLAGS             += $(addprefix -Wl$(COMMA)-u$(COMMA),$(LIBKC_BUILTIN_SYMBOLS))
endif # TARGET_TYPE

ifeq ($(CONFIG),debug)
CFLAGS              += -DDEBUG -fno-omit-frame-pointer
ifneq ($(NO_ANALYZER),1)
CFLAGS              += -fanalyzer
endif # NO_ANALYZER
ifneq ($(NO_SANITIZE),1)
CFLAGS              += -fsanitize=undefined
endif # NO_SANITIZE
else # CONFIG
CFLAGS              += -DNDEBUG -O3 -flto=auto
endif # CONFIG
