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
SOVERSION := 0
# The version is the CTUI_VERSION of the umbrella header, which multicz bumps.
VERSION := $(shell sed -n 's/^\#define CTUI_VERSION "\(.*\)"/\1/p' include/ctui/ctui.h)
SANITIZE ?= address,undefined
BUILD   := build

SRCS    := $(sort $(wildcard src/*.c))
OBJS    := $(SRCS:src/%.c=$(BUILD)/%.o)
HEADERS := $(sort $(wildcard include/ctui/*.h))
LIB     := $(BUILD)/libctui.a
PIC_OBJS := $(SRCS:src/%.c=$(BUILD)/pic/%.o)
SHLIB   := $(BUILD)/libctui.so.$(VERSION)
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

.DEFAULT_GOAL := all
.PHONY: help all test check-headers check-install check-shared format format-check lint clean install uninstall shared install-shared

help: ## List the make targets
	@awk -F':.*## ' '/^[a-z][a-z-]*:.*## /{printf "  %-16s %s\n", $$1, $$2}' $(MAKEFILE_LIST)

all: $(LIB) ## Build the static library

$(LIB): $(OBJS)
	rm -f $@
	$(AR) rcs $@ $^

$(BUILD)/%.o: src/%.c $(HEADERS) | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD):
	mkdir -p $@ $(BUILD)/san $(BUILD)/tests $(BUILD)/hdr $(BUILD)/pic $(BUILD)/shared

test: check-headers check-install check-shared $(TEST_BINS) ## Build and run the unit tests and the install checks
	@set -e; for t in $(TEST_BINS); do echo "== $$t"; $$t; done

$(BUILD)/san/%.o: src/%.c $(HEADERS) | $(BUILD)
	$(CC) $(CFLAGS) $(SAN_FLAGS) -c -o $@ $<

$(SAN_LIB): $(SAN_OBJS)
	rm -f $@
	$(AR) rcs $@ $^

$(BUILD)/tests/%: tests/%.c $(wildcard tests/*.h) $(SAN_LIB) $(HEADERS) | $(BUILD)
	$(CC) $(CFLAGS) $(SAN_FLAGS) $(LDFLAGS) -no-pie -o $@ $< $(SAN_LIB) $(NCURSES_LIBS) -lm

# Every public header must compile alone, and twice (include guards).
check-headers: | $(BUILD) ## Compile every public header on its own
	@set -e; for h in $(HEADERS); do \
	  n=$$(basename $$h .h); \
	  printf '#include <ctui/%s.h>\n#include <ctui/%s.h>\nint ctui_hdr_check_%s;\n' $$n $$n $$n > $(BUILD)/hdr/$$n.c; \
	  $(CC) $(CFLAGS) -fsyntax-only $(BUILD)/hdr/$$n.c; \
	done; echo "headers ok"

# Install under build/stage and build tests/consumer.c with pkg-config only,
# the way another project would use the library.
STAGE := $(abspath $(BUILD))/stage
check-install: $(LIB) ## Build a consumer against the installed library
	@$(MAKE) --no-print-directory uninstall PREFIX=$(STAGE) DESTDIR= >/dev/null
	@$(MAKE) --no-print-directory install PREFIX=$(STAGE) DESTDIR= >/dev/null
	@export PKG_CONFIG_PATH=$(STAGE)/lib/pkgconfig; \
	  $(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror $(LDFLAGS) \
	  $$($(PKG_CONFIG) --cflags ctui) -o $(BUILD)/consumer tests/consumer.c \
	  $$($(PKG_CONFIG) --libs ctui) && $(BUILD)/consumer && echo "install ok"

# Shared library: separate -fPIC objects, only the ctui_ API is exported.
shared: $(SHLIB) ## Build the shared library

$(BUILD)/pic/%.o: src/%.c $(HEADERS) | $(BUILD)
	$(CC) $(CFLAGS) -fPIC -c -o $@ $<

$(SHLIB): $(PIC_OBJS) libctui.map
	$(CC) $(CFLAGS) $(LDFLAGS) -shared -Wl,-soname,libctui.so.$(SOVERSION) \
	  -Wl,--version-script=libctui.map -o $@ $(PIC_OBJS) $(NCURSES_LIBS) -lm

install-shared: install $(SHLIB) ## Install the static and the shared libraries
	$(INSTALL) -m 755 $(SHLIB) $(DESTDIR)$(LIBDIR)/libctui.so.$(VERSION)
	ln -sf libctui.so.$(VERSION) $(DESTDIR)$(LIBDIR)/libctui.so.$(SOVERSION)
	ln -sf libctui.so.$(SOVERSION) $(DESTDIR)$(LIBDIR)/libctui.so

# Link tests/consumer.c against the shared library; check the SONAME and that
# nothing but ctui_ symbols is exported.
check-shared: $(SHLIB) | $(BUILD) ## Check the SONAME and the exported symbols
	@ln -sf ../libctui.so.$(VERSION) $(BUILD)/shared/libctui.so
	@ln -sf ../libctui.so.$(VERSION) $(BUILD)/shared/libctui.so.$(SOVERSION)
	@$(CC) -std=c11 -Wall -Wextra -Wpedantic -Werror $(LDFLAGS) -Iinclude $(NCURSES_CFLAGS) \
	  -o $(BUILD)/consumer-shared tests/consumer.c -L$(BUILD)/shared -lctui \
	  $(NCURSES_LIBS) -lm -Wl,-rpath,$(abspath $(BUILD))/shared
	@$(BUILD)/consumer-shared >/dev/null
	@readelf -d $(SHLIB) | grep -q 'SONAME.*\[libctui.so.$(SOVERSION)\]'
	@! nm -D --defined-only $(SHLIB) | grep -v ' ctui_' | grep -q .
	@echo "shared ok"

format: ## Format the sources with clang-format
	clang-format -i $(SOURCES)

format-check: ## Fail when a source is not formatted
	clang-format --dry-run -Werror $(SOURCES)

lint: ## Run clang-tidy on the sources
	clang-tidy --quiet $(filter %.c,$(SOURCES)) -- -std=c11 -D_POSIX_C_SOURCE=200809L -Iinclude \
		$(NCURSES_CFLAGS)

# DESTDIR stages the files for packaging; the .pc keeps the real PREFIX.
install: $(LIB) ## Install the static library, headers and ctui.pc
	$(INSTALL) -d $(DESTDIR)$(LIBDIR)/pkgconfig $(DESTDIR)$(PREFIX)/include/ctui
	$(INSTALL) -m 644 $(LIB) $(DESTDIR)$(LIBDIR)/libctui.a
	$(INSTALL) -m 644 $(HEADERS) $(DESTDIR)$(PREFIX)/include/ctui/
	sed -e 's|@PREFIX@|$(PREFIX)|' -e 's|@LIBDIR@|$(LIBDIR)|' \
	  -e 's|@VERSION@|$(VERSION)|' ctui.pc.in > $(DESTDIR)$(LIBDIR)/pkgconfig/ctui.pc

uninstall: ## Remove the installed files
	rm -f $(DESTDIR)$(LIBDIR)/libctui.so $(DESTDIR)$(LIBDIR)/libctui.so.$(SOVERSION) \
	  $(DESTDIR)$(LIBDIR)/libctui.so.$(VERSION)
	rm -f $(DESTDIR)$(LIBDIR)/libctui.a $(DESTDIR)$(LIBDIR)/pkgconfig/ctui.pc
	rm -rf $(DESTDIR)$(PREFIX)/include/ctui

clean: ## Remove the build directory
	rm -rf $(BUILD)
