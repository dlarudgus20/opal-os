CONFIG             ?= debug
PLATFORM           ?= pc-x64

ifeq ($(CONFIG), debug)
else ifeq ($(CONFIG), release)
else
$(error "Invalid build configuration: $(CONFIG). Use 'debug' or 'release'.")
endif

ifeq ($(PLATFORM), pc-x64)
else
$(error "Invalid platform: $(PLATFORM). Supported platform: pc-x64.")
endif

TEST_DIR           := tests

ifneq ($(findstring $(TARGET_TYPE), static-lib shared-lib), )
ifneq ($(wildcard $(TEST_DIR)/*), )
HAS_TEST           := 1
endif
endif

MAKE_ARG           := CONFIG=$(CONFIG) PLATFORM=$(PLATFORM)

ifeq ($(HAS_TEST), 1)
TEST_MAKE_ARG      := IS_TEST=1
ifeq ($(TEST_AS_SHARED), 1)
TEST_MAKE_ARG      += TARGET_TYPE=shared-lib TEST_AS_SHARED=1
endif
endif

WARNING_FLAGS      := -pedantic -Wall -Wextra -Werror \
	-Wno-switch -Wno-unused-parameter -Wno-error=unused-variable -Wno-error=unused-function

ifneq ($(IS_TEST), 1)

BUILD_PREFIX       := build
TOOLSET_PREFIX     ?= x86_64-elf
TOOLSET_CC         ?= $(TOOLSET_PREFIX)-gcc
TOOLSET_AR         ?= $(TOOLSET_PREFIX)-gcc-ar
TOOLSET_OBJCOPY    ?= $(TOOLSET_PREFIX)-objcopy
TOOLSET_OBJDUMP    ?= $(TOOLSET_PREFIX)-objdump
TOOLSET_NM         ?= $(TOOLSET_PREFIX)-gcc-nm
TOOLSET_GDB        ?= gdb
TOOLSET_NASM       ?= nasm

CFLAGS             += -std=c23 -ggdb3 -ffreestanding -mno-red-zone -masm=intel \
	-mcmodel=kernel -mno-mmx -mno-sse -mno-sse2 $(WARNING_FLAGS)

LDFLAGS            += -nostdlib -Wl,--gc-sections -Wl,--fatal-warning

else

BUILD_PREFIX       := build/tests
TOOLSET_CC         ?= gcc
TOOLSET_AR         ?= gcc-ar
TOOLSET_OBJCOPY    ?= objcopy
TOOLSET_OBJDUMP    ?= objdump
TOOLSET_NM         ?= gcc-nm
TOOLSET_GDB        ?= gdb
TOOLSET_NASM       ?= nasm

CFLAGS             += -std=c23 -ggdb3 -ffreestanding -masm=intel $(WARNING_FLAGS)
LDFLAGS            += -Wl,--fatal-warning

TEST_CXX           ?= g++
TEST_CXXFLAGS      += -std=c++23 -ggdb3 -masm=intel $(WARNING_FLAGS)
TEST_LDFLAGS       += -Wl,--fatal-warning

ifeq ($(CONFIG), debug)
TEST_CXXFLAGS      += -DDEBUG
else ifeq ($(CONFIG), release)
TEST_CXXFLAGS      += -DNDEBUG -O3 -flto
endif

endif

ifeq ($(TARGET_TYPE), shared-lib)
CFLAGS             += -fPIC
LDFLAGS            += -shared
endif

ifeq ($(CONFIG), debug)
CFLAGS             += -DDEBUG
else ifeq ($(CONFIG), release)
CFLAGS             += -DNDEBUG -O3 -flto
endif

BUILD_DIR          := $(BUILD_PREFIX)/$(PLATFORM)/$(CONFIG)
