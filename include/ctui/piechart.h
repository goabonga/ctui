/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * piechart.h - pie and donut charts with a legend of shares.
 *
 * Slices start at twelve o'clock and run clockwise, each with the share of
 * the total that its value represents (negative and NaN values count as
 * 0). The disc is drawn with dots (see canvas.h), which are square, so it
 * is round whatever the cell shape. A legend to the right lists the
 * slices with their percentages when the area is wide enough.
 *
 * The caller owns the data.
 */
#ifndef CTUI_PIECHART_H
#define CTUI_PIECHART_H

#include <stdbool.h>
#include <stddef.h>

#include <ctui/canvas.h>
#include <ctui/dialog.h>
#include <ctui/theme.h>

typedef struct ctui_slice {
	const char *label;
	double value;
	ctui_icon_color color;
} ctui_slice;

typedef struct ctui_piechart {
	const ctui_slice *slices;
	size_t count;
	double hole; /* 0 = a pie; 0 < hole < 1: a donut with that inner radius
			share */
	bool legend;
	const ctui_style *style; /* may be NULL; set after init */
} ctui_piechart;

/* A pie with a legend. */
void ctui_piechart_init(ctui_piechart *c, const ctui_slice *slices,
			size_t count);

/* Sum of the values that count. */
double ctui_piechart_total(const ctui_piechart *c);

/* Share of slice i in [0, 1] (0 when the total is 0). */
double ctui_piechart_share(const ctui_piechart *c, size_t i);

/* The slice at a position of the circle, as a fraction of the turn from
 * twelve o'clock clockwise ([0, 1)), or -1 when there is no slice. */
int ctui_piechart_slice_at(const ctui_piechart *c, double turn);

typedef struct ctui_piechart_layout {
	ctui_rect pie;	  /* cells of the disc, relative to the chart */
	ctui_rect legend; /* width 0 when there is no legend */
	double radius;	  /* in dots */
} ctui_piechart_layout;

/* False when the area is too small (under 6 x 3). */
bool ctui_piechart_layout_for(const ctui_piechart *c, int width, int height,
			      ctui_piechart_layout *out);

/* The slice under cell (y, x) of the chart (on the disc or in the legend),
 * or -1. */
int ctui_piechart_hit(const ctui_piechart *c, int width, int height, int y,
		      int x);

void ctui_piechart_draw(const ctui_piechart *c, int y, int x, int width,
			int height);

#endif /* CTUI_PIECHART_H */
