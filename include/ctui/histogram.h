/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * histogram.h - histogram of samples.
 *
 * The samples are put in equal-width bins over a range, and drawn as
 * vertical bars that use the eighth blocks of the font for a fine height
 * (an ASCII approximation elsewhere). The range is that of the finite
 * samples rounded out to round values by ctui_axis_nice(), or the range
 * [min, max] set by the caller. NaN samples are ignored. A sample outside
 * the range set by the caller is ignored too, or counted in the first or
 * last bin when `clamp` is set (infinities only count when clamping).
 *
 * The number of bins is `bins`, or when it is 0 the rule of Sturges:
 * ceil(log2(n)) + 1 for n samples (1 for none), at most the width of the
 * plot in columns. The count axis starts at 0 and ends at a round value
 * above the tallest bin; the value axis is labelled under the bars.
 *
 * The caller owns the samples.
 */
#ifndef CTUI_HISTOGRAM_H
#define CTUI_HISTOGRAM_H

#include <stdbool.h>
#include <stddef.h>

#include <ctui/canvas.h>
#include <ctui/dialog.h>
#include <ctui/theme.h>

#define CTUI_HISTOGRAM_MAX_BINS                                                \
	4096 /* what the automatic rule never exceeds                          \
	      */

typedef struct ctui_histogram {
	const double *samples;
	size_t count;
	size_t bins; /* 0 = automatic */
	bool ranged; /* bin [min, max] instead of the data's range */
	double min, max;
	bool clamp; /* samples outside [min, max] go to the end bins */
	ctui_icon_color color;
	const ctui_style *style; /* may be NULL; set after init */
} ctui_histogram;

/* Automatic bins, data range, blue bars. */
void ctui_histogram_init(ctui_histogram *h, const double *samples,
			 size_t count);

/* The rule of Sturges for n samples, in [1, CTUI_HISTOGRAM_MAX_BINS]. */
size_t ctui_histogram_auto_bins(size_t n);

/* Minimum and maximum of the finite samples. False (and [0, 1]) when there
 * is none. */
bool ctui_histogram_range(const double *samples, size_t count, double *lo,
			  double *hi);

/* Count the samples in `bins` equal bins of [lo, hi]: counts[i] gets the
 * number in bin i (all set, `counts` has `bins` entries). A sample equal to
 * hi falls in the last bin; an empty range (hi <= lo) puts the samples equal
 * to lo in the first bin. Returns the number of samples counted, 0 when
 * bins is 0 or counts is NULL. */
size_t ctui_histogram_bin(const double *samples, size_t count, size_t bins,
			  double lo, double hi, bool clamp, size_t *counts);

typedef struct ctui_histogram_layout {
	ctui_rect plot;	 /* the bars area, relative to the chart */
	int label_width; /* columns left of the plot */
	int labels_row; /* row of the value labels under the plot, -1 if none */
	size_t bins;	/* bins drawn */
	double lo, hi;	/* the range covered by the bins */
	size_t tallest; /* count of the fullest bin */
	ctui_axis counts; /* the count axis */
} ctui_histogram_layout;

/* False when the area is too small (under 8 x 3). */
bool ctui_histogram_layout_for(const ctui_histogram *h, int width, int height,
			       ctui_histogram_layout *out);

/* The bin in the column of cell (y, x), whatever the height of its bar: its
 * index, count and range [from, to). Any output may be NULL. False outside
 * the plot. */
bool ctui_histogram_hit(const ctui_histogram *h, int width, int height, int y,
			int x, size_t *bin, size_t *count, double *from,
			double *to);

void ctui_histogram_draw(const ctui_histogram *h, int y, int x, int width,
			 int height);

#endif /* CTUI_HISTOGRAM_H */
