# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Chris <goabonga@pm.me>

CC      ?= cc
CFLAGS  ?= -O2
CFLAGS  += -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude
PREFIX  ?= /usr/local
LIBDIR  ?= $(PREFIX)/lib
BUILD   := build

SRCS    := $(sort $(wildcard src/*.c))
OBJS    := $(SRCS:src/%.c=$(BUILD)/%.o)
HEADERS := $(sort $(wildcard include/ctui/*.h))
LIB     := $(BUILD)/libctui.a

.PHONY: all clean install

all: $(LIB)

$(LIB): $(OBJS)
	rm -f $@
	$(AR) rcs $@ $^

$(BUILD)/%.o: src/%.c $(HEADERS) | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD):
	mkdir -p $@

install: all
	install -Dm644 $(LIB) $(DESTDIR)$(LIBDIR)/libctui.a
	install -d $(DESTDIR)$(PREFIX)/include/ctui
	install -m644 $(HEADERS) $(DESTDIR)$(PREFIX)/include/ctui/

clean:
	rm -rf $(BUILD)
