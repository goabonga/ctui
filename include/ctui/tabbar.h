/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * tabbar.h - the powerline tab bar.
 *
 * One row on the bar background: a bold brand, then the tabs, then
 * right-aligned key hints when they fit.
 *   current tab:  hard divider in, " icon label " bold on the idle tone,
 *                 hard divider out;
 *   other tabs:   " icon label " in the inactive colour, a soft divider
 *                 between two of them;
 *   disabled tab: same shape in the soft colour.
 * A closable tab ends with an "x" the hit test reports separately.
 *
 * The layout is a pure function of the tab bar and the width, so mouse
 * hit testing and drawing always agree. Tabs that do not fit are dropped
 * from the right.
 */
#ifndef CTUI_TABBAR_H
#define CTUI_TABBAR_H

#include <stdbool.h>
#include <stddef.h>

typedef struct ctui_tab {
	const char *icon;	/* shown in UTF-8 locales; may be NULL */
	const char *ascii_icon; /* shown otherwise; may be NULL */
	const char *label;
	bool enabled;
	bool closable;
} ctui_tab;

typedef struct ctui_tabbar {
	const char *brand; /* may be NULL */
	const ctui_tab *tabs;
	size_t count;
	size_t current;
	const char *hints; /* right-aligned, may be NULL */
} ctui_tabbar;

typedef struct ctui_tab_span {
	int x, width;
	int close_x; /* column of the close mark, -1 when not closable */
} ctui_tab_span;

/* Fill spans[0..count) with the placement of every tab that fits in
 * columns; returns how many fit. */
size_t ctui_tabbar_layout(const ctui_tabbar *bar, int columns, bool unicode,
			  ctui_tab_span *spans);

/* Draw the bar on row y across the whole screen width. */
void ctui_tabbar_draw(int y, const ctui_tabbar *bar);

/* Index of the tab under column x, or -1. *close is set when x is on the
 * close mark of a closable tab (close may be NULL). */
int ctui_tabbar_hit(const ctui_tabbar *bar, int columns, bool unicode, int x,
		    bool *close);

#endif /* CTUI_TABBAR_H */
