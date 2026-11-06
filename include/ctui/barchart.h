/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * barchart.h - bar chart of one or more series over categories.
 *
 * Value k of every series belongs to category k. Bars of the series are
 * side by side in each category, or piled up when `stacked` is set. The bars
 * are vertical, or horizontal when `horizontal` is set, and use the eighth
 * blocks of the font for a fine length (an ASCII approximation elsewhere).
 * The value axis starts at 0 (or at `min` when `fixed`) and ends at a round
 * value above the largest bar; negative values count as 0. A legend row on
 * top names the series.
 *
 * The caller owns the data.
 */
#ifndef CTUI_BARCHART_H
#define CTUI_BARCHART_H

#include <stdbool.h>
#include <stddef.h>

#include <ctui/linechart.h>

typedef struct ctui_barchart {
	const ctui_series *series;
	size_t count;		       /* number of series */
	const char *const *categories; /* one name per category, may be NULL */
	size_t category_count;	       /* 0 = the longest series */
	bool horizontal, stacked;
	bool legend;
	bool fixed; /* use [min, max] for the value axis */
	double min, max;
	const ctui_style *style; /* may be NULL; set after init */
} ctui_barchart;

/* A vertical, side-by-side chart with a legend and an automatic range. */
void ctui_barchart_init(ctui_barchart *c, const ctui_series *series,
			size_t count);

/* Number of categories. */
size_t ctui_barchart_categories(const ctui_barchart *c);

/* Value of series s in category k (0 when missing, negative or NaN). */
double ctui_barchart_value(const ctui_barchart *c, size_t s, size_t k);

typedef struct ctui_barchart_layout {
	ctui_rect plot;	 /* the bars area, relative to the chart */
	int label_width; /* columns left of the plot (value labels, or category
			    names) */
	int legend_row;	 /* -1 when there is no legend */
	int labels_row; /* row under the plot (category names, or value labels),
			   -1 if none */
	ctui_axis axis;
} ctui_barchart_layout;

/* False when the area is too small (under 8 x 3). */
bool ctui_barchart_layout_for(const ctui_barchart *c, int width, int height,
			      ctui_barchart_layout *out);

/* The bar under cell (y, x) of the chart: its category and series (the
 * segment of a stacked bar). False when the cell is not on a bar. */
bool ctui_barchart_hit(const ctui_barchart *c, int width, int height, int y,
		       int x, size_t *category, size_t *series);

void ctui_barchart_draw(const ctui_barchart *c, int y, int x, int width,
			int height);

#endif /* CTUI_BARCHART_H */
