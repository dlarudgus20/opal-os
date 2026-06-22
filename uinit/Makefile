TARGET_NAME := uinit
TARGET_TYPE := executable
STATIC_REFS := libuc libopalsys libkc libkubsan

LDFLAGS := -Wl,-u,_panic_format

LD_SCRIPT := src/linker.ld

all: build

include ../mkfiles/conf.mk
include ../mkfiles/rules.mk

build: $(TARGET)
