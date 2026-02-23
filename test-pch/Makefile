TARGET_NAME := test-gch
TARGET_TYPE := static-lib

IS_TEST := 1

all: build

include ../mkfiles/conf.mk

TARGET := build/$(PLATFORM)/test-pch.h.gch
SOURCE := include/test-pch.h

$(TARGET): $(SOURCE)
	@mkdir -p $(dir $@)
	$(TEST_CXX) $(TEST_CXXFLAGS) -x c++-header -c $< -o $@

.PHONY: all clean fullclean build-test test clean-test

build: $(TARGET)

clean:

fullclean:
	rm -rf build

gen:

clean-gen:

build-test:

test:

clean-test:
