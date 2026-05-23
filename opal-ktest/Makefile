include ../mkfiles/config-rust.mk

TARGET_ROOT := target
TARGET_DIR  := $(TARGET_ROOT)/debug
TARGET_LIB  := $(TARGET_DIR)/libopal_ktest.so

.PHONY: all build clean fullclean

all: build

build: $(TARGET_LIB)

$(TARGET_LIB): Cargo.toml Cargo.lock src/lib.rs
	$(CARGO) build --manifest-path Cargo.toml

clean:

fullclean:
	$(CARGO) clean
