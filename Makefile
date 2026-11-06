# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Chris <goabonga@pm.me>

CC      ?= cc
CFLAGS  ?= -O2
PKG_CONFIG ?= pkg-config
NCURSES_CFLAGS := $(shell $(PKG_CONFIG) --cflags ncursesw 2>/dev/null || echo -D_DEFAULT_SOURCE -D_XOPEN_SOURCE=600)
NCURSES_LIBS   := $(shell $(PKG_CONFIG) --libs ncursesw 2>/dev/null || echo -lncursesw)
CFLAGS  += -std=c11 -Wall -Wextra -Wpedantic -Werror -D_POSIX_C_SOURCE=200809L \
	-Iinclude $(NCURSES_CFLAGS)
PREFIX  ?= /usr/local
LIBDIR  ?= $(PREFIX)/lib
INSTALL ?= install
# The version is the CTUI_VERSION of the umbrella header, which multicz bumps.
VERSION := $(shell sed -n 's/^\#define CTUI_VERSION "\(.*\)"/\1/p' include/ctui/ctui.h)
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

.PHONY: all test check-headers format format-check lint clean install uninstall

all: $(LIB)

$(LIB): $(OBJS)
	rm -f $@
	$(AR) rcs $@ $^

$(BUILD)/%.o: src/%.c $(HEADERS) | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD):
	mkdir -p $@ $(BUILD)/san $(BUILD)/tests $(BUILD)/hdr

test: check-headers $(TEST_BINS)
	@set -e; for t in $(TEST_BINS); do echo "== $$t"; $$t; done

$(BUILD)/san/%.o: src/%.c $(HEADERS) | $(BUILD)
	$(CC) $(CFLAGS) $(SAN_FLAGS) -c -o $@ $<

$(SAN_LIB): $(SAN_OBJS)
	rm -f $@
	$(AR) rcs $@ $^

$(BUILD)/tests/%: tests/%.c $(wildcard tests/*.h) $(SAN_LIB) $(HEADERS) | $(BUILD)
	$(CC) $(CFLAGS) $(SAN_FLAGS) $(LDFLAGS) -no-pie -o $@ $< $(SAN_LIB) $(NCURSES_LIBS) -lm

# Every public header must compile alone, and twice (include guards).
check-headers: | $(BUILD)
	@set -e; for h in $(HEADERS); do \
	  n=$$(basename $$h .h); \
	  printf '#include <ctui/%s.h>\n#include <ctui/%s.h>\nint ctui_hdr_check_%s;\n' $$n $$n $$n > $(BUILD)/hdr/$$n.c; \
	  $(CC) $(CFLAGS) -fsyntax-only $(BUILD)/hdr/$$n.c; \
	done; echo "headers ok"

format:
	clang-format -i $(SOURCES)

format-check:
	clang-format --dry-run -Werror $(SOURCES)

lint:
	clang-tidy --quiet $(filter %.c,$(SOURCES)) -- -std=c11 -D_POSIX_C_SOURCE=200809L -Iinclude \
		$(NCURSES_CFLAGS)

# DESTDIR stages the files for packaging; the .pc keeps the real PREFIX.
install: $(LIB)
	$(INSTALL) -d $(DESTDIR)$(LIBDIR)/pkgconfig $(DESTDIR)$(PREFIX)/include/ctui
	$(INSTALL) -m 644 $(LIB) $(DESTDIR)$(LIBDIR)/libctui.a
	$(INSTALL) -m 644 $(HEADERS) $(DESTDIR)$(PREFIX)/include/ctui/
	sed -e 's|@PREFIX@|$(PREFIX)|' -e 's|@LIBDIR@|$(LIBDIR)|' \
	  -e 's|@VERSION@|$(VERSION)|' ctui.pc.in > $(DESTDIR)$(LIBDIR)/pkgconfig/ctui.pc

uninstall:
	rm -f $(DESTDIR)$(LIBDIR)/libctui.a $(DESTDIR)$(LIBDIR)/pkgconfig/ctui.pc
	rm -rf $(DESTDIR)$(PREFIX)/include/ctui

clean:
	rm -rf $(BUILD)
