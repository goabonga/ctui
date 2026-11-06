/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * linechart.h - line chart of one or more series.
 *
 * Series share the x axis by index: point i of every series is at the same
 * column, spread over the plot width. Lines are drawn with dots (see
 * canvas.h), so a chart is smooth even in a small area. The y axis is
 * scaled to the data (or to a range set by the caller) with round tick
 * values labelled on the left; a NaN breaks the line. Optional labels name
 * the first and the last point under the plot, optional titles name the
 * axes, and a legend names the series: on a row above or below the plot, in
 * a column to its right, or not at all. A series is drawn as a line, as
 * points only (a scatter plot), or as both.
 *
 * The caller owns the data. ctui_series is shared with the bar chart.
 */
#ifndef CTUI_LINECHART_H
#define CTUI_LINECHART_H

#include <stdbool.h>
#include <stddef.h>

#include <ctui/canvas.h>
#include <ctui/dialog.h>
#include <ctui/theme.h>

/* How a series is drawn by the line chart; zero is a line. */
typedef enum ctui_series_mode {
	CTUI_SERIES_LINE,   /* points joined by lines */
	CTUI_SERIES_POINTS, /* a marker on each point, nothing between */
	CTUI_SERIES_BOTH    /* lines and markers */
} ctui_series_mode;

typedef struct ctui_series {
	const char *name;
	const double *values;
	size_t count;
	ctui_icon_color color;
	ctui_series_mode mode; /* line chart only, may be left out */
} ctui_series;

/* Where the line chart puts its legend; zero is the top row. */
typedef enum ctui_legend_pos {
	CTUI_LEGEND_TOP,
	CTUI_LEGEND_BOTTOM,
	CTUI_LEGEND_RIGHT,
	CTUI_LEGEND_NONE
} ctui_legend_pos;

typedef struct ctui_linechart {
	const ctui_series *series;
	size_t count;		     /* number of series */
	const char *const *x_labels; /* one per point, may be NULL */
	size_t x_label_count;
	bool legend; /* name the series, where legend_pos says */
	ctui_legend_pos legend_pos;
	const char *x_title, *y_title; /* axis titles, may be NULL */
	bool fixed; /* use [min, max] instead of the data's range */
	double min, max;
	const ctui_style *style; /* colours and glyphs of this instance, may be
				    NULL; set after init */
} ctui_linechart;

/* A chart with a legend on top and an automatic range. */
void ctui_linechart_init(ctui_linechart *c, const ctui_series *series,
			 size_t count);

/* Number of points: the longest series. */
size_t ctui_linechart_points(const ctui_linechart *c);

/* The y axis for a plot of `rows` rows. */
ctui_axis ctui_linechart_axis(const ctui_linechart *c, int rows);

typedef struct ctui_linechart_layout {
	ctui_rect plot;	 /* the dots area, relative to the chart */
	int label_width; /* columns of the y labels */
	int legend_row;	 /* row of a top or bottom legend, -1 when there is none
			  */
	int legend_col;	 /* column of a right legend (one series per row from
			    plot.y), -1 when there is none */
	int legend_width; /* columns of the right legend */
	int labels_row;	  /* row of the x labels, -1 when there are none */
	int x_title_row;  /* row of the x title, -1 when there is none */
	int y_title_row;  /* row of the y title, -1 when there is none */
	ctui_axis axis;
} ctui_linechart_layout;

/* Where things go in a width x height area; false when it is too small
 * (under 8 x 3). */
bool ctui_linechart_layout_for(const ctui_linechart *c, int width, int height,
			       ctui_linechart_layout *out);

/* The point nearest to column x of the chart, or -1 outside the plot. */
int ctui_linechart_index_at(const ctui_linechart *c, int width, int height,
			    int x);

void ctui_linechart_draw(const ctui_linechart *c, int y, int x, int width,
			 int height);

#endif /* CTUI_LINECHART_H */
