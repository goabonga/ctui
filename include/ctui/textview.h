/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * textview.h - a read-only text view with a line-number gutter.
 *
 * The view shows a NUL-terminated UTF-8 text (owned by the caller, which
 * must outlive the view) split at '\n'. It keeps a cursor, an optional
 * selection and the scroll position. Cursor offsets are byte positions at
 * character boundaries, not screen columns. Tabs use 4-column stops;
 * control characters and invalid bytes are drawn as '?', so file contents
 * never become terminal escape sequences.
 *
 * Keys: arrows, Home/End, PgUp/PgDn move the cursor; the shifted arrows
 * (KEY_SLEFT, KEY_SRIGHT, KEY_SR, KEY_SF) extend the selection; Ctrl-A
 * selects everything; Esc clears the selection.
 *
 * Horizontal scrolling: the view follows the cursor sideways, unless the
 * window was scrolled away from a line too short to reach it. With
 * `hscroll` set, the bottom row of the drawn area also holds a horizontal
 * scrollbar under the text (the gutter stays fixed); ctui_textview_hscroll()
 * and the ctui_textview_h*() pointer functions then move the window along the
 * lines, pulling the cursor into it as ctui_textview_scroll_to() does for
 * rows. Mouse wheel events with Shift are up to the caller, who maps them
 * to ctui_textview_hscroll().
 */
#ifndef CTUI_TEXTVIEW_H
#define CTUI_TEXTVIEW_H

#include <stdbool.h>

#include <ctui/scrollbar.h>
#include <ctui/theme.h>
#include <stddef.h>

#define CTUI_TAB_WIDTH 4

typedef struct ctui_textview {
	const char *text;
	size_t *starts;	   /* byte offset of each line */
	size_t line_count; /* at least 1 */
	size_t line, byte; /* cursor */
	size_t goal;	   /* column kept across vertical moves */
	bool selecting;
	size_t anchor_line, anchor_byte;
	size_t top, left; /* first visible line and column */
	size_t width;	  /* columns of the widest line */
	bool detached; /* the window was scrolled sideways away from the cursor
			*/
	bool hscroll; /* zero (default): no horizontal scrollbar; set after init
		       */
	ctui_scrollbar hbar;	 /* the horizontal scrollbar, as last drawn */
	const ctui_style *style; /* colours and glyphs of this instance, may be
				    NULL; set after init */
} ctui_textview;

bool ctui_textview_init(ctui_textview *v, const char *text);
void ctui_textview_free(ctui_textview *v);

/* Line i without its newline; *len receives its length in bytes. */
const char *ctui_textview_line(const ctui_textview *v, size_t i, size_t *len);

/* Columns taken by the code point cp when it starts at column col: tabs
 * reach the next stop, controls and invalid bytes take one (drawn as '?'). */
size_t ctui_cell_width(unsigned long cp, size_t col);

/* Display column of a byte offset within a line, and back: the byte of the
 * character that covers column (the line length past its end). */
size_t ctui_line_column(const char *line, size_t len, size_t byte);
size_t ctui_line_byte(const char *line, size_t len, size_t column);

/* Columns taken by the gutter: line-number digits plus a margin. */
int ctui_textview_gutter(const ctui_textview *v);

/* Whether the character at (line, byte) is inside the selection. */
bool ctui_textview_selected(const ctui_textview *v, size_t line, size_t byte);

/* Apply a key; rows is the page size. Returns true when anything changed. */
bool ctui_textview_key(ctui_textview *v, int key, size_t rows);

/* Scroll so that the cursor is inside a rows x cols text area. */
void ctui_textview_reveal(ctui_textview *v, size_t rows, size_t cols);

/* Show the window starting at line top (clamped so the window stays full)
 * and pull the cursor into it. */
void ctui_textview_scroll_to(ctui_textview *v, size_t rows, size_t top);

/* Show the columns starting at left (clamped so the window stays full) in
 * a window of cols text columns and pull the cursor into it. */
void ctui_textview_scroll_left_to(ctui_textview *v, size_t cols, size_t left);

/* Move the window sideways by delta columns (negative = left); true when
 * it moved. cols is the text width, as in ctui_textview_scroll_left_to(). */
bool ctui_textview_hscroll(ctui_textview *v, size_t cols, int delta);

/* Pointer events for the horizontal scrollbar, in screen coordinates and
 * valid after a draw with `hscroll` set. Each returns true when the bar
 * took the event or the window moved. */
bool ctui_textview_hpress(ctui_textview *v, int y, int x);
bool ctui_textview_hdrag(ctui_textview *v, int x);
bool ctui_textview_hrelease(ctui_textview *v);
bool ctui_textview_hwheel(ctui_textview *v, int columns);

/* Draw the gutter and the text in a width x height area. The cursor cell
 * is shown reversed when focused. With `hscroll` the last row is the
 * horizontal scrollbar and the text takes height - 1 rows. */
void ctui_textview_draw(ctui_textview *v, int y, int x, int width, int height,
			bool focused);

#endif /* CTUI_TEXTVIEW_H */
