TARGET_NAME := opsh
TARGET_TYPE := executable
STATIC_REFS := libuc libkc

LDFLAGS := -Wl,-u,_panic_format

TEST_SHARED_REFS := libpanicimpl
LDFLAGS_ON_TEST := -Wl,--exclude-libs=libkc -Wl,--exclude-libs=libuc

ifneq ($(IS_TEST),1)
STATIC_REFS += libkubsan
endif # IS_TEST

LD_SCRIPT := src/linker.ld

all: build

include ../mkfiles/conf.mk
include ../mkfiles/rules.mk

build: $(TARGET)
