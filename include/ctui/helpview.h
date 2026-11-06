/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * helpview.h - key-binding help overlay.
 *
 * A centred, framed dialog listing caller-owned groups of (keys,
 * description) pairs. The groups are flowed into one column on a narrow
 * terminal and into two (the text continues at the top of the second
 * column) when the dialog is wide enough, so it adapts to the terminal
 * size; it scrolls when the lines do not all fit. Like the other dialogs
 * it is a plain state struct: feed keys and clicks, redraw each turn.
 *
 *   keys:  Up / Down (k / j) scroll a line, PgUp / PgDn (b / Space) a page,
 *          Home / End (g / G) go to the ends; Esc, q, ?, Enter and Ctrl-C
 *          close.
 *   mouse: the wheel scrolls, the close button closes.
 */
#ifndef CTUI_HELPVIEW_H
#define CTUI_HELPVIEW_H

#include <stdbool.h>
#include <stddef.h>

#include <ctui/dialog.h>
#include <ctui/labels.h>
#include <ctui/theme.h>

#define CTUI_HELP_TWO_COLUMNS_MIN                                              \
	72 /* inner width from which two columns are used */

typedef struct ctui_help_entry {
	const char *keys; /* "Ctrl-S", "j / Down" */
	const char *description;
} ctui_help_entry;

typedef struct ctui_help_group {
	const char *title; /* heading line, may be NULL */
	const ctui_help_entry *entries;
	size_t count;
} ctui_help_group;

typedef struct ctui_helpview {
	const char *title; /* dialog title; NULL for the "help_title" label */
	const ctui_help_group *groups;
	size_t group_count;
	size_t top; /* first visible line */
	const ctui_labels
	    *labels; /* override of the global labels, may be NULL */
	const ctui_style *style; /* colours and glyphs of this instance, may be
				    NULL; set after init */
} ctui_helpview;

void ctui_helpview_init(ctui_helpview *h, const ctui_help_group *groups,
			size_t group_count);

/* Lines of content: a heading per titled group, an entry per line and a
 * blank line between groups. */
size_t ctui_helpview_lines(const ctui_helpview *h);

/* The box, the rows of content and the columns (1 or 2) on a lines x cols
 * screen; false when it is too small (less than 20 x 5 cells). */
bool ctui_helpview_geometry(const ctui_helpview *h, int lines, int cols,
			    ctui_rect *box, int *rows, int *columns);

/* OPEN, or CANCELLED when the key closes the view. */
ctui_dialog_result ctui_helpview_key(ctui_helpview *h, int key, int lines,
				     int cols);

/* A left click at screen cell (y, x): CANCELLED on the close button. */
ctui_dialog_result ctui_helpview_click(const ctui_helpview *h, int lines,
				       int cols, int y, int x);

/* Wheel: dir > 0 scrolls down, < 0 up, three lines per notch. */
void ctui_helpview_wheel(ctui_helpview *h, int lines, int cols, int dir);

void ctui_helpview_draw(const ctui_helpview *h);

#endif /* CTUI_HELPVIEW_H */
