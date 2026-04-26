TARGET_NAME := opsh
TARGET_TYPE := executable
STATIC_REFS := libkc

TEST_SHARED_REFS := libpanicimpl
LDFLAGS_ON_TEST := -Wl,--exclude-libs=libkc

ifneq ($(IS_TEST), 1)
STATIC_REFS += libkubsan
endif

LD_SCRIPT := src/linker.ld

all: build

include ../mkfiles/conf.mk
include ../mkfiles/rules.mk

build: $(TARGET)
