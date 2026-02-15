TARGET_NAME := libcoll
TARGET_TYPE := static-lib
STATIC_REFS := libkc

TEST_SHARED_REFS := libpanicimpl
LDFLAGS_ON_TEST := -Wl,--exclude-libs=libkc

all: build

include ../mkfiles/conf.mk
include ../mkfiles/rules.mk

build: $(TARGET)
