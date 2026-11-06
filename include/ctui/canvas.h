/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * canvas.h - a dot canvas for plotting, and nice axis scales.
 *
 * Every terminal cell holds 2 x 4 dots (Braille patterns), so a canvas of
 * cols x rows cells is 2*cols x 4*rows dots, origin at the top left. Each
 * cell has one colour, that of the last dot set in it. Locales without
 * UTF-8 get a '*' in each cell that holds a dot. Cells without a dot are
 * left untouched when drawing, so a canvas can go over other drawing.
 *
 * ctui_axis_nice() rounds a data range out to round tick values, the way
 * the chart components scale their axes.
 */
#ifndef CTUI_CANVAS_H
#define CTUI_CANVAS_H

#include <stdbool.h>
#include <stddef.h>

#include <ctui/theme.h>

typedef struct ctui_canvas {
	int cols, rows;	      /* size in cells */
	unsigned char *dots;  /* Braille bits of each cell */
	unsigned char *color; /* ctui_icon_color + 1 of each cell, 0 = none */
} ctui_canvas;

/* False when a size is below 1 or memory runs out. */
bool ctui_canvas_init(ctui_canvas *c, int cols, int rows);
void ctui_canvas_free(ctui_canvas *c);
void ctui_canvas_clear(ctui_canvas *c);

/* Size in dots. */
int ctui_canvas_width(const ctui_canvas *c);
int ctui_canvas_height(const ctui_canvas *c);

/* Set one dot; outside the canvas nothing happens. */
void ctui_canvas_set(ctui_canvas *c, int x, int y, ctui_icon_color color);
bool ctui_canvas_get(const ctui_canvas *c, int x, int y);

/* A line of dots from (x0, y0) to (x1, y1), ends included. */
void ctui_canvas_line(ctui_canvas *c, int x0, int y0, int x1, int y1,
		      ctui_icon_color color);

/* The visible part of a line between points that may lie anywhere,
 * fractional or far outside the canvas (Cohen-Sutherland clipping against
 * the dots, then the line of the rounded ends). The cost follows what is
 * visible, not the length of the segment. A non-finite coordinate draws
 * nothing. */
void ctui_canvas_clipped_line(ctui_canvas *c, double x0, double y0, double x1,
			      double y1, ctui_icon_color color);

/* The rectangle with corners (x0, y0) and (x1, y1), in any order: its
 * outline, or all its dots when filled. */
void ctui_canvas_rect(ctui_canvas *c, int x0, int y0, int x1, int y1,
		      ctui_icon_color color, bool filled);

/* The circle of centre (cx, cy) and radius r dots (midpoint algorithm): its
 * outline, or the whole disc when filled. A negative radius draws nothing,
 * a radius of 0 one dot. */
void ctui_canvas_circle(ctui_canvas *c, int cx, int cy, int r,
			ctui_icon_color color, bool filled);

typedef struct ctui_dot {
	int x, y;
} ctui_dot;

/* Lines joining the n points in order; one point is a dot, none nothing. */
void ctui_canvas_polyline(ctui_canvas *c, const ctui_dot *points, size_t n,
			  ctui_icon_color color);

/* The Braille character of a set of dot bits, as UTF-8 (NUL-terminated,
 * 4 bytes at most). */
void ctui_canvas_glyph(unsigned char dots, char out[4]);

/* Draw the canvas with its top left cell at (y, x). */
void ctui_canvas_draw(const ctui_canvas *c, int y, int x);

typedef struct ctui_axis {
	double min, max; /* the scale, rounded out from the data */
	double step;	 /* between ticks */
	int ticks;	 /* number of ticks, min and max included */
} ctui_axis;

/* A scale containing [lo, hi] with at most max_ticks round ticks (at least
 * 2). An empty range is widened. */
ctui_axis ctui_axis_nice(double lo, double hi, int max_ticks);

/* Text of a tick value, with as many decimals as the step needs. */
void ctui_axis_label(const ctui_axis *axis, double value, char *out,
		     size_t cap);

#endif /* CTUI_CANVAS_H */
