TARGET_NAME := libcoll
TARGET_TYPE := static-lib
STATIC_REFS := libkc

TEST_SHARED_REFS := libpanicimpl
TEST_AS_SHARED := 1
TEST_LDFLAGS :=

all: build

include ../mkfiles/conf.mk
include ../mkfiles/rules.mk

build: $(TARGET)
