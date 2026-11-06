/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * scrollview.h - a scrollable viewport over a virtual area.
 *
 * The caller gives the size of the virtual area (width x height cells) and
 * a callback that draws it. Each frame the view works out which part of the
 * area is visible and calls the callback with the screen rectangle of the
 * viewport and the virtual cell that sits at its top-left corner (`oy`,
 * `ox`): the cell (r, c) of the area is on screen at (y + r - oy, x + c -
 * ox). The callback draws only inside the rectangle and may skip what falls
 * outside the visible range.
 *
 * Scrollbars appear on the right and bottom edges only when the area is
 * larger than the viewport, and take a column / a row of the space given to
 * ctui_scrollview_draw(). The layout and the hit tests are pure functions.
 *
 * Keys: arrows scroll by one cell, PgUp / PgDn by a page, Shift-Left /
 * Shift-Right by a page sideways, Home / End jump to the top / bottom.
 * Mouse: press, drag and release on the scrollbars, the wheel anywhere.
 * Events carry screen coordinates.
 */
#ifndef CTUI_SCROLLVIEW_H
#define CTUI_SCROLLVIEW_H

#include <stdbool.h>
#include <stddef.h>

#include <ctui/scrollbar.h>
#include <ctui/theme.h>

/* Space of a viewport: the content rectangle and which bars take room. */
typedef struct ctui_scrollview_layout {
	int width, height; /* the content rectangle, cells */
	bool vbar,
	    hbar; /* a vertical bar on the right, a horizontal one below */
} ctui_scrollview_layout;

typedef enum ctui_scrollview_part {
	CTUI_SCROLLVIEW_NONE,
	CTUI_SCROLLVIEW_CONTENT,
	CTUI_SCROLLVIEW_VBAR,
	CTUI_SCROLLVIEW_HBAR,
	CTUI_SCROLLVIEW_CORNER /* where the two bars meet */
} ctui_scrollview_part;

/* Draw the visible part: the viewport is the width x height rectangle at
 * screen (y, x); its top-left cell shows virtual row oy, column ox. */
typedef void (*ctui_scrollview_draw_fn)(void *user, int y, int x, int width,
					int height, size_t oy, size_t ox);

typedef struct ctui_scrollview {
	size_t width, height; /* the virtual area */
	size_t top, left;     /* its cell at the top-left of the viewport */
	ctui_scrollview_draw_fn draw;
	void *user;
	const ctui_style *style; /* colours and glyphs of this instance, may be
				    NULL; set after init */
	ctui_scrollbar vbar, hbar;
	int y, x, area_width,
	    area_height;	       /* the space given to the last draw */
	ctui_scrollview_layout layout; /* and how it was divided */
} ctui_scrollview;

void ctui_scrollview_init(ctui_scrollview *v, size_t width, size_t height,
			  ctui_scrollview_draw_fn draw, void *user);

/* Change the size of the virtual area; the offsets are clamped. */
void ctui_scrollview_resize(ctui_scrollview *v, size_t width, size_t height);

/* Divide an area_width x area_height space for a virtual width x height
 * area: the bars needed and the content rectangle left. */
ctui_scrollview_layout ctui_scrollview_layout_for(size_t width, size_t height,
						  int area_width,
						  int area_height);

/* Largest offsets for the current content rectangle (0 when the area fits). */
size_t ctui_scrollview_max_top(const ctui_scrollview *v);
size_t ctui_scrollview_max_left(const ctui_scrollview *v);

/* Show the area from row top, column left (clamped to the valid range);
 * true when the position changed. */
bool ctui_scrollview_scroll_to(ctui_scrollview *v, size_t top, size_t left);

/* Move by rows / columns (negative = up / left); true when it moved. */
bool ctui_scrollview_scroll_by(ctui_scrollview *v, long rows, long columns);

/* Scroll the least that shows the virtual cell (row, column). */
bool ctui_scrollview_reveal(ctui_scrollview *v, size_t row, size_t column);

/* Apply a key (see above); true when the position changed. */
bool ctui_scrollview_key(ctui_scrollview *v, int key);

/* What lies at the screen cell (y, x), as of the last draw. */
ctui_scrollview_part ctui_scrollview_hit(const ctui_scrollview *v, int y,
					 int x);

/* The virtual cell shown at the screen cell (y, x); false when that cell is
 * not on the content or lies past the area. */
bool ctui_scrollview_cell_at(const ctui_scrollview *v, int y, int x,
			     size_t *row, size_t *column);

/* Mouse: true when the event was taken (the position may have changed). */
bool ctui_scrollview_press(ctui_scrollview *v, int y, int x);
bool ctui_scrollview_drag(ctui_scrollview *v, int y, int x);
bool ctui_scrollview_release(ctui_scrollview *v);
/* The wheel over the viewport: rows and columns, negative = up / left. */
bool ctui_scrollview_wheel(ctui_scrollview *v, int y, int x, int rows,
			   int columns);

/* Draw into the width x height space at (y, x): the content through the
 * callback, then the scrollbars. */
void ctui_scrollview_draw(ctui_scrollview *v, int y, int x, int width,
			  int height);

#endif /* CTUI_SCROLLVIEW_H */
