CONFIG             ?= debug

ifeq ($(CONFIG), debug)
else ifeq ($(CONFIG), release)
else
$(error "Invalid build configuration: $(CONFIG). Use 'debug' or 'release'.")
endif

ifeq ($(PLATFORM), pc-x64)
else
$(error "Invalid platform: $(PLATFORM). Supported platform: pc-x64.")
endif

TOOLSET_PREFIX     ?= x86_64-elf
TOOLSET_CC         ?= $(TOOLSET_PREFIX)-gcc
TOOLSET_OBJCOPY    ?= $(TOOLSET_PREFIX)-objcopy
TOOLSET_NASM       ?= nasm

CFLAGS             += -std=c23 -ggdb3 -ffreestanding -mno-red-zone -masm=intel \
	-mcmodel=kernel -mno-mmx -mno-sse -mno-sse2 \
	-Iinclude -Iplatform/$(PLATFORM)/include \
	-pedantic -Wall -Wextra -Werror \
	-Wno-switch -Wno-unused-parameter -Wno-error=unused-variable -Wno-error=unused-function

LDFLAGS            += -nostdlib -Wl,--gc-sections -Wl,--fatal-warning

ifeq ($(CONFIG), debug)
CFLAGS             += -DDEBUG
else ifeq ($(CONFIG), release)
CFLAGS             += -DNDEBUG -O3 -flto
endif

BUILD_DIR          := build/$(PLATFORM)/$(CONFIG)
