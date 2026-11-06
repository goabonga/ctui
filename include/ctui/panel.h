/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * panel.h - a scrollable panel made of titled sections of readings.
 *
 * A reading that is absent (NULL value) is dropped, never shown as a
 * placeholder; a section whose readings were all dropped never prints its
 * heading. Opening a section only records its title; the first line added
 * afterwards flushes it as: a blank line (only when the panel already has
 * lines), the title, and a rule.
 *
 * A reading row is "  " + label padded to CTUI_PANEL_LABEL_WIDTH columns +
 * " " + value right-aligned in CTUI_PANEL_VALUE_WIDTH columns, then
 * optionally "  " + a gauge of CTUI_PANEL_GAUGE_WIDTH cells and "  " + a note.
 * Padding counts display columns. The gauge is drawn over reserved blanks
 * with the zoned look, so lines stay plain UTF-8 text.
 *
 * On wide screens complete sections flow into two columns.
 */
#ifndef CTUI_PANEL_H
#define CTUI_PANEL_H

#include <stdbool.h>

#include <ctui/theme.h>
#include <stddef.h>

#define CTUI_PANEL_LABEL_WIDTH 18
#define CTUI_PANEL_VALUE_WIDTH 8
#define CTUI_PANEL_GAUGE_WIDTH 28
#define CTUI_PANEL_RULE_WIDTH 60
#define CTUI_PANEL_TWO_COLUMNS_MIN 80

typedef enum ctui_line_kind {
	CTUI_LINE_TEXT = 0, /* rows and raw lines */
	CTUI_LINE_BLANK,    /* separator before a section */
	CTUI_LINE_HEADING,  /* section title */
	CTUI_LINE_RULE	    /* the rule under a title */
} ctui_line_kind;

typedef struct ctui_panel_line {
	ctui_line_kind kind;
	int gauge_col;	       /* display column of the gauge, -1 when none */
	double gauge_fraction; /* clamped fill ratio; meaningful with a gauge */
	char *text;
} ctui_panel_line;

typedef struct ctui_panel {
	ctui_panel_line *lines;
	size_t count, cap;
	char *pending;		 /* section title waiting for its first line */
	const ctui_style *style; /* colours and glyphs of this instance, may be
				    NULL; set after init */
} ctui_panel;

typedef struct ctui_panel_layout {
	size_t rows; /* height of the tallest column */
	int columns; /* 1 or 2 */
	int width;   /* width of one column */
	int *column; /* per line: 0 or 1 */
	int *row;    /* per line: row in its column, -1 when not drawn */
	size_t count;
} ctui_panel_layout;

void ctui_panel_init(ctui_panel *panel);
void ctui_panel_free(ctui_panel *panel);
bool ctui_panel_is_empty(const ctui_panel *panel);

/* Record a pending section title. */
void ctui_panel_section(ctui_panel *panel, const char *title);

/* Append a raw line, flushing a pending section first. */
void ctui_panel_add_line(ctui_panel *panel, const char *text);

/* Append one reading, or nothing when value is NULL. gauge is the fill
 * ratio in 0..1, or a negative or non-finite number for no gauge; note
 * may be NULL. */
void ctui_panel_row(ctui_panel *panel, const char *label, const char *value,
		    double gauge, const char *note);

/* When nothing was added and empty is non-empty, the panel becomes the
 * single line empty. */
void ctui_panel_finish(ctui_panel *panel, const char *empty);

/* Place complete sections into one or two columns for a given width. */
void ctui_panel_arrange(const ctui_panel *panel, int width, bool allow_columns,
			ctui_panel_layout *layout);
void ctui_panel_layout_free(ctui_panel_layout *layout);

/* Draw the rows [scroll, scroll + rows) of an arranged panel at (y, x). */
void ctui_panel_draw(const ctui_panel *panel, const ctui_panel_layout *layout,
		     int y, int x, int rows, size_t scroll);

#endif /* CTUI_PANEL_H */
