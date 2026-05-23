TARGET_NAME := opal-kernel
TARGET_TYPE := static-obj

RUST_MACRO_REFS := opal-ktest
RUST_SYSROOT_REF := rust-sysroot

all: build

include ../mkfiles/config-rust.mk
include ../mkfiles/rules-rust.mk

build: $(TARGET)
