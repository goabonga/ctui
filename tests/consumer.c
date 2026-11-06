/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * consumer.c - a program built the way another project would build against
 * the installed library: the single include, flags from pkg-config.
 */
#include <ctui/ctui.h>

int main(void)
{
	ctui_scrollbar bar = {0};
	ctui_scrollbar_set(&bar, 100, 10, 5);
	return ctui_utf8_columns("abc") == 3 && bar.offset == 5 ? 0 : 1;
}
