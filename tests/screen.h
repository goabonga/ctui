/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * screen.h - a curses screen on /dev/null, to read back what a component
 * drew. Include after check.h.
 *
 *   screen_open(24, 80);         colours on, UTF-8 glyphs
 *   screen_clear(); ...draw...
 *   screen_has_text("x")         the text is somewhere on the screen
 *   screen_has_color(attr)       a cell uses the colour pair of attr
 */
#ifndef CTUI_TESTS_SCREEN_H
#define CTUI_TESTS_SCREEN_H

#include <curses.h>
#include <locale.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ctui/theme.h>

/* Kept open for the life of the process: curses owns them. */
static FILE *screen_out_;
static FILE *screen_in_;

static __attribute__((unused)) void screen_open(int lines, int cols)
{
	(void)lines;
	(void)cols;
	setlocale(LC_ALL, "C.UTF-8");
	setenv("TERM", "xterm-256color", 1);
	unsetenv("NO_COLOR");
	unsetenv("CTUI_ASCII");
	screen_out_ = fopen("/dev/null", "w");
	screen_in_ = fopen("/dev/null", "r");
	if (!screen_out_ || !screen_in_ ||
	    !newterm("xterm-256color", screen_out_, screen_in_)) {
		fprintf(stderr, "cannot open a test screen\n");
		exit(1);
	}
	ctui_theme_init();
}

static __attribute__((unused)) void screen_clear(void)
{
	erase();
}

static __attribute__((unused)) bool screen_has_text(const char *needle)
{
	char row[1024];
	for (int y = 0; y < LINES; y++) {
		if (mvinnstr(y, 0, row, (int)sizeof row - 1) != ERR &&
		    strstr(row, needle))
			return true;
	}
	return false;
}

static __attribute__((unused)) bool screen_has_color(int attr)
{
	short want = (short)PAIR_NUMBER((chtype)attr);
	for (int y = 0; y < LINES; y++)
		for (int x = 0; x < COLS; x++) {
			cchar_t cell;
			wchar_t wch[CCHARW_MAX];
			attr_t attrs;
			short pair;
			if (mvin_wch(y, x, &cell) == OK &&
			    getcchar(&cell, wch, &attrs, &pair, NULL) == OK &&
			    pair == want)
				return true;
		}
	return false;
}

#endif /* CTUI_TESTS_SCREEN_H */
