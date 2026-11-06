# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Chris <goabonga@pm.me>

CC      ?= cc
CFLAGS  ?= -O2
CFLAGS  += -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude
PREFIX  ?= /usr/local
LIBDIR  ?= $(PREFIX)/lib
SANITIZE ?= address,undefined
BUILD   := build

SRCS    := $(sort $(wildcard src/*.c))
OBJS    := $(SRCS:src/%.c=$(BUILD)/%.o)
HEADERS := $(sort $(wildcard include/ctui/*.h))
LIB     := $(BUILD)/libctui.a
SOURCES := $(SRCS) $(HEADERS) $(wildcard tests/*.c)

# Unit tests are linked with the library built with the sanitizers.
# -fno-pie -no-pie: on linux-hardened a PIE lands inside the ASan shadow
# range.
# Disable the sanitizers with `make test SANITIZE=` where the platform
# rejects them.
SAN_FLAGS := -g -fno-pie $(if $(SANITIZE),-fsanitize=$(SANITIZE) -fno-sanitize-recover=all)
SAN_OBJS  := $(SRCS:src/%.c=$(BUILD)/san/%.o)
SAN_LIB   := $(BUILD)/libctui-san.a
TEST_SRCS := $(sort $(wildcard tests/test_*.c))
TEST_BINS := $(TEST_SRCS:tests/%.c=$(BUILD)/tests/%)

.PHONY: all test format format-check lint clean install

all: $(LIB)

$(LIB): $(OBJS)
	rm -f $@
	$(AR) rcs $@ $^

$(BUILD)/%.o: src/%.c $(HEADERS) | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD):
	mkdir -p $@ $(BUILD)/san $(BUILD)/tests

test: $(TEST_BINS)
	@set -e; for t in $(TEST_BINS); do echo "== $$t"; $$t; done

$(BUILD)/san/%.o: src/%.c $(HEADERS) | $(BUILD)
	$(CC) $(CFLAGS) $(SAN_FLAGS) -c -o $@ $<

$(SAN_LIB): $(SAN_OBJS)
	rm -f $@
	$(AR) rcs $@ $^

$(BUILD)/tests/%: tests/%.c $(wildcard tests/*.h) $(SAN_LIB) $(HEADERS) | $(BUILD)
	$(CC) $(CFLAGS) $(SAN_FLAGS) $(LDFLAGS) -no-pie -o $@ $< $(SAN_LIB)

format:
	clang-format -i $(SOURCES)

format-check:
	clang-format --dry-run -Werror $(SOURCES)

lint:
	clang-tidy --quiet $(filter %.c,$(SOURCES)) -- -std=c11 -Iinclude

install: all
	install -Dm644 $(LIB) $(DESTDIR)$(LIBDIR)/libctui.a
	install -d $(DESTDIR)$(PREFIX)/include/ctui
	install -m644 $(HEADERS) $(DESTDIR)$(PREFIX)/include/ctui/

clean:
	rm -rf $(BUILD)
