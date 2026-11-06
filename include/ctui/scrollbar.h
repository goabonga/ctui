/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * scrollbar.h - a scrollbar, vertical or horizontal, driven by press, drag,
 * release and the mouse wheel.
 *
 * The scrollbar does not own the content: the caller describes it with
 * ctui_scrollbar_set(total, visible, offset) before drawing, places the bar
 * with ctui_scrollbar_place() and, after each event, reads back `offset`
 * (the first visible row) and applies it to the content.
 *
 * The thumb is proportional to visible / total (one row at least) and sits
 * at offset / (total - visible) of the free track. Nothing is drawn, and
 * no event is taken, while the content fits in the window.
 *
 * A horizontal bar is the same with columns for rows: place it with
 * ctui_scrollbar_place_horizontal(), and `offset` is then the first visible
 * column. Events carry the screen coordinates all the same; only the one
 * along the track counts for a drag.
 *
 * Pressing on the thumb grabs it and keeps the pointer's place on it while
 * dragging; pressing on the track above or below the thumb moves one page.
 * Events carry screen coordinates.
 */
#ifndef CTUI_SCROLLBAR_H
#define CTUI_SCROLLBAR_H

#include <stdbool.h>
#include <stddef.h>

typedef struct ctui_scrollbar {
	size_t total;	  /* rows of content (columns when horizontal) */
	size_t visible;	  /* rows shown at once */
	size_t offset;	  /* first visible row, 0 .. total - visible */
	int y, x, height; /* the track: column x, rows y .. y + height - 1; when
			     horizontal row y, columns x .. x + height - 1 */
	bool horizontal;  /* zero is a vertical bar */
	bool dragging;
	int grab; /* rows between the top of the thumb and the pointer */
	/* Per-bar look; NULL / 0 take the theme's glyphs and colours. */
	const char *track_glyph, *thumb_glyph;
	int track_attr,
	    thumb_attr; /* curses attributes, e.g. from ctui_attr() */
} ctui_scrollbar;

/* Describe the content; offset is clamped. Keeps an ongoing drag. */
void ctui_scrollbar_set(ctui_scrollbar *bar, size_t total, size_t visible,
			size_t offset);

/* Place the track. */
void ctui_scrollbar_place(ctui_scrollbar *bar, int y, int x, int height);

/* Place the track of a horizontal bar: row y, columns x .. x + width - 1. */
void ctui_scrollbar_place_horizontal(ctui_scrollbar *bar, int y, int x,
				     int width);

/* Whether the content is longer than the window and the track has room. */
bool ctui_scrollbar_needed(const ctui_scrollbar *bar);

/* Thumb of a track of `track` rows: first row and length. False when no
 * thumb is needed. */
bool ctui_scrollbar_thumb(size_t total, size_t visible, size_t offset,
			  int track, int *start, int *length);

/* Whether the screen cell is on the track (and the bar is needed). */
bool ctui_scrollbar_over(const ctui_scrollbar *bar, int y, int x);

/* Left button pressed at (y, x). Returns true when the bar took the event;
 * `offset` may have changed. */
bool ctui_scrollbar_press(ctui_scrollbar *bar, int y, int x);

/* Pointer moved with the button down; true when the offset changed. `pos`
 * is the row of the pointer, or its column for a horizontal bar. */
bool ctui_scrollbar_drag(ctui_scrollbar *bar, int pos);

/* Button released; true when a drag ended. */
bool ctui_scrollbar_release(ctui_scrollbar *bar);

/* Wheel: move the offset by `rows` (negative = up, or left); true when it
 * changed. */
bool ctui_scrollbar_wheel(ctui_scrollbar *bar, int rows);

/* The glyph of the track or of the thumb: the bar's own, else the theme's
 * (a horizontal track takes the horizontal line glyph, and a horizontal
 * thumb the lower half block when the theme's thumb is the full block, to be
 * as thick as a vertical one). */
const char *ctui_scrollbar_glyph(const ctui_scrollbar *bar, bool thumb);

/* Draw the bar; dialog selects the colours of a dialog surface. Returns
 * false (drawing nothing) when it is not needed. */
bool ctui_scrollbar_draw(const ctui_scrollbar *bar, bool dialog);

#endif /* CTUI_SCROLLBAR_H */
