/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * histogram.c - histogram of samples (histogram.h).
 */
#include <ctui/histogram.h>

#include <curses.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <ctui/text.h>

void ctui_histogram_init(ctui_histogram *h, const double *samples, size_t count)
{
	*h = (ctui_histogram){
	    .samples = samples, .count = count, .color = CTUI_ICON_BLUE};
}

size_t ctui_histogram_auto_bins(size_t n)
{
	if (n <= 1)
		return 1;
	size_t bits = 0; /* ceil(log2(n)) */
	for (size_t v = n - 1; v; v >>= 1U)
		bits++;
	size_t bins = bits + 1;
	return bins > CTUI_HISTOGRAM_MAX_BINS ? CTUI_HISTOGRAM_MAX_BINS : bins;
}

bool ctui_histogram_range(const double *samples, size_t count, double *lo,
			  double *hi)
{
	bool any = false;
	*lo = 0;
	*hi = 1;
	for (size_t i = 0; samples && i < count; i++) {
		double v = samples[i];
		if (!isfinite(v))
			continue;
		if (!any || v < *lo)
			*lo = v;
		if (!any || v > *hi)
			*hi = v;
		any = true;
	}
	if (!any) {
		*lo = 0;
		*hi = 1;
	}
	return any;
}

size_t ctui_histogram_bin(const double *samples, size_t count, size_t bins,
			  double lo, double hi, bool clamp, size_t *counts)
{
	if (bins == 0 || !counts)
		return 0;
	memset(counts, 0, bins * sizeof *counts);
	size_t counted = 0;
	for (size_t i = 0; samples && i < count; i++) {
		double v = samples[i];
		if (isnan(v))
			continue;
		size_t bin;
		if (v < lo || v > hi) {
			if (!clamp)
				continue;
			bin = v < lo ? 0 : bins - 1;
		} else if (!(hi > lo)) {
			bin = 0;
		} else {
			/* halved so that the span of a huge range cannot
			 * overflow */
			double t =
			    (v / 2 - lo / 2) / (hi / 2 - lo / 2) * (double)bins;
			bin = t >= (double)bins ? bins - 1
			      : t > 0		? (size_t)t
						: 0;
		}
		counts[bin]++;
		counted++;
	}
	return counted;
}

/* Ticks for a length: a few dozen at most, whatever the area. */
static int ticks_for(int n)
{
	return (n > 40 ? 40 : n) + 1;
}

/* First column of bin k of `bins` over `width` columns. */
static int bin_start(size_t k, size_t bins, int width)
{
	return (int)(((unsigned long long)k * (unsigned long long)width + bins -
		      1) /
		     bins);
}

/* The layout and the counts of every bin (freed by the caller). */
static bool compute(const ctui_histogram *h, int width, int height,
		    ctui_histogram_layout *out, size_t **counts_out)
{
	if (width < 8 || height < 3)
		return false;
	ctui_histogram_layout l = {.labels_row = -1};
	int bottom = 0;
	if (height >= 5)
		l.labels_row = height - 1, bottom = 1;
	int rows = height - bottom;

	/* the labels of the count axis are as wide as for all the samples in
	 * one bin */
	ctui_axis probe = ctui_axis_nice(0, (double)(h->count ? h->count : 1),
					 ticks_for(rows / 2));
	for (int i = 0; i < probe.ticks; i++) {
		char label[32];
		ctui_axis_label(&probe, probe.min + i * probe.step, label,
				sizeof label);
		if ((int)ctui_utf8_columns(label) > l.label_width)
			l.label_width = (int)ctui_utf8_columns(label);
	}
	if (l.label_width > width / 3)
		l.label_width = width / 3;
	l.plot = (ctui_rect){.y = 0,
			     .x = l.label_width + 1,
			     .width = width - l.label_width - 1,
			     .height = rows};
	if (l.plot.width < 2)
		return false;

	double dlo, dhi;
	ctui_histogram_range(h->samples, h->count, &dlo, &dhi);
	if (h->ranged && isfinite(h->min) && isfinite(h->max) &&
	    h->max >= h->min) {
		dlo = h->min;
		dhi = h->max;
	} /* else the range of the data, also when the one set is unusable */
	if (!(dhi > dlo)) { /* one value, or none */
		dlo -= 0.5;
		dhi += 0.5;
	}
	ctui_axis xa = ctui_axis_nice(dlo, dhi, ticks_for(l.plot.width / 8));
	bool exact = h->ranged && isfinite(h->min) && isfinite(h->max) &&
		     h->max >= h->min;
	l.lo = exact ? dlo : xa.min;
	l.hi = exact ? dhi : xa.max;

	size_t bins = h->bins ? h->bins : ctui_histogram_auto_bins(h->count);
	if (bins > CTUI_HISTOGRAM_MAX_BINS)
		bins = CTUI_HISTOGRAM_MAX_BINS;
	if (bins > (size_t)l.plot.width)
		bins = (size_t)l.plot.width;
	l.bins = bins;
	size_t *counts = malloc(bins * sizeof *counts);
	if (!counts)
		return false;
	ctui_histogram_bin(h->samples, h->count, bins, l.lo, l.hi, h->clamp,
			   counts);
	for (size_t i = 0; i < bins; i++)
		if (counts[i] > l.tallest)
			l.tallest = counts[i];
	l.counts = ctui_axis_nice(0, (double)(l.tallest ? l.tallest : 1),
				  ticks_for(rows / 2));
	*out = l;
	if (counts_out)
		*counts_out = counts;
	else
		free(counts);
	return true;
}

bool ctui_histogram_layout_for(const ctui_histogram *h, int width, int height,
			       ctui_histogram_layout *out)
{
	return compute(h, width, height, out, NULL);
}

bool ctui_histogram_hit(const ctui_histogram *h, int width, int height, int y,
			int x, size_t *bin, size_t *count, double *from,
			double *to)
{
	ctui_histogram_layout l;
	size_t *counts;
	if (!compute(h, width, height, &l, &counts))
		return false;
	int px = x - l.plot.x, py = y - l.plot.y;
	bool in = px >= 0 && py >= 0 && px < l.plot.width && py < l.plot.height;
	if (in) {
		size_t k = (size_t)((unsigned long long)px * l.bins /
				    (unsigned long long)l.plot.width);
		if (bin)
			*bin = k;
		if (count)
			*count = counts[k];
		if (from)
			*from =
			    l.lo + (l.hi - l.lo) * ((double)k / (double)l.bins);
		if (to)
			*to = k + 1 == l.bins
				  ? l.hi
				  : l.lo + (l.hi - l.lo) * ((double)(k + 1) /
							    (double)l.bins);
	}
	free(counts);
	return in;
}

static void hist_draw(const ctui_histogram *h, int y, int x, int width,
		      int height)
{
	ctui_histogram_layout l;
	size_t *counts;
	if (!compute(h, width, height, &l, &counts))
		return;
	int muted = ctui_attr(CTUI_PAIR_MUTED, false);
	int border = ctui_attr(CTUI_PAIR_BORDER, false);
	const ctui_glyphs *g = ctui_theme_glyphs();
	bool unicode = ctui_theme_unicode();
	int screen_rows, screen_cols;
	getmaxyx(stdscr, screen_rows, screen_cols);

	for (int r = y + l.plot.y < 0 ? -(y + l.plot.y) : 0;
	     r < l.plot.height && y + l.plot.y + r < screen_rows; r++)
		ctui_put(y + l.plot.y + r, x + l.label_width, 1, g->box_v,
			 border);
	double span = l.counts.max - l.counts.min;
	for (int i = 0; i < l.counts.ticks; i++) {
		double v = l.counts.min + i * l.counts.step;
		int r = l.plot.height < 2
			    ? 0
			    : (int)lround((l.counts.max - v) / span *
					  (l.plot.height - 1));
		char label[32];
		ctui_axis_label(&l.counts, v, label, sizeof label);
		int w = (int)ctui_utf8_columns(label);
		if (w <= l.label_width)
			ctui_put(y + l.plot.y + r, x + l.label_width - w, w,
				 label, muted);
	}

	if (l.labels_row >= 0) {
		ctui_axis xa =
		    ctui_axis_nice(l.lo, l.hi, ticks_for(l.plot.width / 8));
		int last_end = -1;
		for (int i = 0; i < xa.ticks; i++) {
			double v = xa.min + i * xa.step;
			if (v < l.lo || v > l.hi)
				continue;
			char label[32];
			ctui_axis_label(&xa, v, label, sizeof label);
			int w = (int)ctui_utf8_columns(label);
			int col = (int)lround((v - l.lo) / (l.hi - l.lo) *
					      (l.plot.width - 1));
			if (col + w > l.plot.width)
				col = l.plot.width - w;
			if (col < 0 || col <= last_end)
				continue;
			ctui_put(y + l.labels_row, x + l.plot.x + col, w, label,
				 muted);
			last_end = col + w;
		}
	}

	static const char *const UP[8] = {
	    "\xe2\x96\x81", "\xe2\x96\x82", "\xe2\x96\x83", "\xe2\x96\x84",
	    "\xe2\x96\x85", "\xe2\x96\x86", "\xe2\x96\x87", "\xe2\x96\x88"};
	int attr = ctui_attr_icon(h->color, false);
	for (size_t k = 0; k < l.bins; k++) {
		int c0 = bin_start(k, l.bins, l.plot.width),
		    c1 = bin_start(k + 1, l.bins, l.plot.width);
		if (c1 - c0 >= 3)
			c1--; /* a gap after wide bars */
		long eighths = counts[k]
				   ? lround((double)counts[k] / l.counts.max *
					    l.plot.height * 8)
				   : 0;
		if (counts[k] && eighths < 1)
			eighths = 1;
		if (eighths > (long)l.plot.height * 8)
			eighths = (long)l.plot.height * 8;
		if (x + l.plot.x + c0 >= screen_cols)
			break;
		for (int col = c0; col < c1 && x + l.plot.x + col < screen_cols;
		     col++) {
			if (x + l.plot.x + col < 0)
				continue;
			long bottom = (long)y + l.plot.y + l.plot.height -
				      1; /* screen row of cell 0 */
			long first = bottom - (screen_rows - 1) > 0
					 ? bottom - (screen_rows - 1)
					 : 0;
			for (long cell = first;
			     cell * 8 < eighths && cell < l.plot.height &&
			     cell <= bottom;
			     cell++) {
				int cover = (int)(eighths - cell * 8 > 8
						      ? 8
						      : eighths - cell * 8);
				const char *glyph =
				    !unicode ? (cover >= 4 ? "#" : ".")
					     : UP[cover - 1];
				ctui_put((int)(bottom - cell),
					 x + l.plot.x + col, 1, glyph, attr);
			}
		}
	}
	free(counts);
}

void ctui_histogram_draw(const ctui_histogram *h, int y, int x, int width,
			 int height)
{
	ctui_style_begin(h->style);
	hist_draw(h, y, x, width, height);
	ctui_style_end();
}
