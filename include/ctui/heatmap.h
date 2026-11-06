/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * heatmap.h - grid of values drawn with a colour ramp.
 *
 * `values` holds rows x cols doubles, row-major and owned by the caller.
 * Each value is mapped linearly from [min, max] (the range of the finite
 * values, or the fixed one) to one of CTUI_HEATMAP_STEPS steps; NaN cells are
 * left empty and infinities take the extreme steps.
 *
 * The ramp is sequential and comes from the palette of the theme: info,
 * accent, cyan, green, yellow, red, from the lowest to the highest step.
 * With colours and UTF-8 a cell is a run of spaces on the colour of its
 * step. Without colours or without UTF-8 it is a density character of
 * ".:=*#@" instead, so the map is readable in plain ASCII.
 *
 * Optional row labels sit left of the grid, column labels above it (cut to
 * the width of a cell), and a legend row at the bottom shows the ramp
 * between the minimum and the maximum. A cursor cell, moved with the arrow
 * keys, Home and End, is drawn in the playhead colours.
 *
 * Cells are at least one character and are widened evenly (up to 8 x 4)
 * when the area allows it.
 */
#ifndef CTUI_HEATMAP_H
#define CTUI_HEATMAP_H

#include <stdbool.h>
#include <stddef.h>

#include <ctui/dialog.h>
#include <ctui/theme.h>

#define CTUI_HEATMAP_STEPS 6

typedef struct ctui_heatmap {
	const double *values; /* rows * cols, row-major */
	size_t rows, cols;
	const char *const *row_labels; /* one per row, may be NULL */
	const char *const *col_labels; /* one per column, may be NULL */
	bool legend;		       /* min / ramp / max row */
	bool fixed; /* use [min, max] instead of the data's range */
	double min, max;
	size_t cursor_row, cursor_col;
	bool show_cursor;
	const ctui_style *style; /* may be NULL; set after init */
} ctui_heatmap;

/* Legend on, cursor shown at (0, 0), range from the data. */
void ctui_heatmap_init(ctui_heatmap *h, const double *values, size_t rows,
		       size_t cols);

/* The range used: the fixed one, or the minimum and maximum of the finite
 * values. False (and [0, 1]) when there is no finite value. */
bool ctui_heatmap_range(const ctui_heatmap *h, double *lo, double *hi);

/* Step 0..steps-1 of a value in [lo, hi]; -1 for NaN. Values outside are
 * clamped, an empty range gives step 0. */
int ctui_heatmap_step(double value, double lo, double hi, int steps);

typedef struct ctui_heatmap_layout {
	ctui_rect grid; /* the cells, relative to the heatmap */
	int cell_w, cell_h;
	int label_width; /* columns of the row labels and the space after, 0 if
			    none */
	int header_row;	 /* row of the column labels, -1 if none */
	int legend_row;	 /* -1 if none */
} ctui_heatmap_layout;

/* False when the cells do not fit, there is no data, or a size is huge. */
bool ctui_heatmap_layout_for(const ctui_heatmap *h, int width, int height,
			     ctui_heatmap_layout *out);

/* The cell under (y, x) of the heatmap; its value may be NaN. */
bool ctui_heatmap_hit(const ctui_heatmap *h, int width, int height, int y,
		      int x, size_t *row, size_t *col, double *value);

/* Move the cursor with KEY_UP / DOWN / LEFT / RIGHT, KEY_HOME (first cell)
 * or KEY_END (last cell), clamped to the grid. True when the key is one of
 * these. */
bool ctui_heatmap_key(ctui_heatmap *h, int key);

void ctui_heatmap_draw(const ctui_heatmap *h, int y, int x, int width,
		       int height);

#endif /* CTUI_HEATMAP_H */
