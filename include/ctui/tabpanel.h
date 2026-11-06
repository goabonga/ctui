/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * tabpanel.h - a strip of tabs that switches the panel under it.
 *
 * Unlike the powerline tab bar (tabbar.h), which spans the screen, a tab
 * panel strip sits inside a page or a dialog: one row of labels and a rule
 * under them, the selected tab marked on the rule. The strip knows only
 * which tab is selected; the caller draws the panel of that tab in the
 * rows below and decides who has the keyboard (the strip while its own
 * focus, then the panel).
 *
 * Left/Right (h/l), Home and End change the selection.
 */
#ifndef CTUI_TABPANEL_H
#define CTUI_TABPANEL_H

#include <stdbool.h>

#include <ctui/theme.h>
#include <stddef.h>

#define CTUI_TABPANEL_ROWS 2 /* labels and rule */

typedef struct ctui_tabpanel {
	const char *const *labels;
	size_t count;
	size_t selected;
	bool dialog;		 /* drawn on the dialog background */
	const ctui_style *style; /* colours and glyphs of this instance, may be
				    NULL; set after init */
} ctui_tabpanel;

void ctui_tabpanel_init(ctui_tabpanel *p, const char *const *labels,
			size_t count);

/* Select tab i; false when there is none. */
bool ctui_tabpanel_select(ctui_tabpanel *p, size_t i);

/* Apply a key; true when the selection changed. */
bool ctui_tabpanel_key(ctui_tabpanel *p, int key);

/* Columns taken by tab i: its label with two columns of padding each side. */
int ctui_tabpanel_tab_width(const ctui_tabpanel *p, size_t i);

/* Tab under column x of a strip width columns wide, or -1. */
int ctui_tabpanel_hit(const ctui_tabpanel *p, int width, int x);

/* Draw the strip at (y, x); focused marks the selected tab as the cursor
 * position. Returns the rows used (CTUI_TABPANEL_ROWS). */
int ctui_tabpanel_draw(const ctui_tabpanel *p, int y, int x, int width,
		       bool focused);

#endif /* CTUI_TABPANEL_H */
