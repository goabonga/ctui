/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * barchart.c - bar chart of one or more series (barchart.h).
 */
#include <ctui/barchart.h>

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <ctui/text.h>

void ctui_barchart_init(ctui_barchart *c, const ctui_series *series,
			size_t count)
{
	*c = (ctui_barchart){.series = series, .count = count, .legend = true};
}

size_t ctui_barchart_categories(const ctui_barchart *c)
{
	if (c->category_count)
		return c->category_count;
	size_t n = 0;
	for (size_t i = 0; i < c->count; i++)
		if (c->series[i].count > n)
			n = c->series[i].count;
	return n;
}

double ctui_barchart_value(const ctui_barchart *c, size_t s, size_t k)
{
	if (s >= c->count || k >= c->series[s].count)
		return 0;
	double v = c->series[s].values[k];
	return isnan(v) || v < 0 ? 0 : v;
}

/* Length of the tallest bar, in value units. */
static double largest(const ctui_barchart *c)
{
	double top = 0;
	size_t n = ctui_barchart_categories(c);
	for (size_t k = 0; k < n; k++) {
		double sum = 0;
		for (size_t s = 0; s < c->count; s++) {
			double v = ctui_barchart_value(c, s, k);
			if (c->stacked)
				sum += v;
			else if (v > sum)
				sum = v;
		}
		if (sum > top)
			top = sum;
	}
	return top;
}

bool ctui_barchart_layout_for(const ctui_barchart *c, int width, int height,
			      ctui_barchart_layout *out)
{
	if (width < 8 || height < 3)
		return false;
	ctui_barchart_layout l = {.legend_row = -1, .labels_row = -1};
	int top = 0, bottom = 0;
	if (c->legend && c->count > 0 && height >= 5)
		l.legend_row = top++;
	bool named = c->categories != NULL;
	if (c->horizontal ? height >= 5 : named && height >= 5)
		l.labels_row = height - 1, bottom = 1;
	int rows = height - top - bottom;

	double lo = c->fixed ? c->min : 0, hi = c->fixed ? c->max : largest(c);
	if (!(hi > lo))
		hi = lo + 1;
	if (c->horizontal) {
		int w = 0;
		for (size_t k = 0; named && k < ctui_barchart_categories(c);
		     k++)
			if (c->categories[k] &&
			    (int)ctui_utf8_columns(c->categories[k]) > w)
				w = (int)ctui_utf8_columns(c->categories[k]);
		if (w > width / 3)
			w = width / 3;
		l.label_width = w;
		int cols = width - w - 1;
		l.axis = ctui_axis_nice(lo, hi, cols / 8 + 1);
		l.plot = (ctui_rect){
		    .y = top, .x = w + 1, .width = cols, .height = rows};
	} else {
		l.axis = ctui_axis_nice(lo, hi, rows / 2 + 1);
		for (int i = 0; i < l.axis.ticks; i++) {
			char label[32];
			ctui_axis_label(&l.axis, l.axis.min + i * l.axis.step,
					label, sizeof label);
			int w = (int)ctui_utf8_columns(label);
			if (w > l.label_width)
				l.label_width = w;
		}
		if (l.label_width > width / 3)
			l.label_width = width / 3;
		l.plot = (ctui_rect){.y = top,
				     .x = l.label_width + 1,
				     .width = width - l.label_width - 1,
				     .height = rows};
	}
	if (l.plot.width < 2 || l.plot.height < 1)
		return false;
	*out = l;
	return true;
}

typedef struct span {
	int start, len; /* along the category axis, relative to the plot */
} span;

/* Where the bar of series s in category k sits across the plot. */
static bool bar_span(const ctui_barchart *c, const ctui_barchart_layout *l,
		     size_t k, size_t s, span *out)
{
	size_t n = ctui_barchart_categories(c);
	int along = c->horizontal ? l->plot.height : l->plot.width;
	if (n == 0 || (size_t)along < n)
		return false;
	int slot = along / (int)n;
	int base = (along - slot * (int)n) / 2 + (int)k * slot;
	int gap = slot >= 3 ? 1 : 0;
	int usable = slot - gap;
	size_t groups = c->stacked ? 1 : c->count;
	if (groups == 0)
		return false;
	int bw = usable / (int)groups;
	if (bw < 1)
		bw = 1;
	if (!c->stacked && (int)s >= usable)
		return false;
	int used = c->stacked ? usable : bw * (int)groups;
	if (used > usable)
		used = usable;
	int lead = (usable - used) / 2;
	*out = (span){.start = base + lead + (c->stacked ? 0 : (int)s * bw),
		      .len = c->stacked ? usable : bw};
	return true;
}

/* Eighths of a cell of the value axis for a value, from the baseline. */
static long eighths(const ctui_barchart_layout *l, double v, int cells)
{
	double t = (v - l->axis.min) / (l->axis.max - l->axis.min);
	if (t < 0)
		t = 0;
	if (t > 1)
		t = 1;
	return lround(t * cells * 8);
}

/* The segment of bar (k, s) as [lo, hi) eighths along the value axis. */
static void segment(const ctui_barchart *c, const ctui_barchart_layout *l,
		    size_t k, size_t s, int cells, long *lo, long *hi)
{
	double below = 0;
	if (c->stacked)
		for (size_t i = 0; i < s; i++)
			below += ctui_barchart_value(c, i, k);
	*lo = c->stacked ? eighths(l, below, cells) : 0;
	*hi = eighths(l, below + ctui_barchart_value(c, s, k), cells);
}

bool ctui_barchart_hit(const ctui_barchart *c, int width, int height, int y,
		       int x, size_t *category, size_t *series)
{
	ctui_barchart_layout l;
	if (!ctui_barchart_layout_for(c, width, height, &l))
		return false;
	int px = x - l.plot.x, py = y - l.plot.y;
	if (px < 0 || py < 0 || px >= l.plot.width || py >= l.plot.height)
		return false;
	int across = c->horizontal ? py : px;
	int cells = c->horizontal ? l.plot.width : l.plot.height;
	int along =
	    c->horizontal ? px : l.plot.height - 1 - py; /* from the baseline */
	size_t n = ctui_barchart_categories(c);
	for (size_t k = 0; k < n; k++)
		for (size_t s = 0; s < c->count; s++) {
			span sp;
			if (!bar_span(c, &l, k, s, &sp) || across < sp.start ||
			    across >= sp.start + sp.len)
				continue;
			long lo, hi;
			segment(c, &l, k, s, cells, &lo, &hi);
			if (hi <= lo || (long)along * 8 >= hi ||
			    (long)(along + 1) * 8 <= lo)
				continue;
			*category = k;
			*series = s;
			return true;
		}
	return false;
}

static void chart_draw(const ctui_barchart *c, int y, int x, int width,
		       int height)
{
	ctui_barchart_layout l;
	if (!ctui_barchart_layout_for(c, width, height, &l))
		return;
	bool unicode = ctui_theme_unicode();
	const ctui_glyphs *g = ctui_theme_glyphs();
	int muted = ctui_attr(CTUI_PAIR_MUTED, false);
	int border = ctui_attr(CTUI_PAIR_BORDER, false);
	size_t n = ctui_barchart_categories(c);

	if (l.legend_row >= 0) {
		int col = 0;
		for (size_t i = 0; i < c->count && col < width; i++) {
			col += ctui_put(
			    y + l.legend_row, x + col, width - col, g->knob,
			    ctui_attr_icon(c->series[i].color, false));
			col += ctui_put(y + l.legend_row, x + col, width - col,
					" ", muted);
			col +=
			    ctui_put(y + l.legend_row, x + col, width - col,
				     c->series[i].name ? c->series[i].name : "",
				     ctui_attr(CTUI_PAIR_BODY, false));
			col += 2;
		}
	}

	double span_v = l.axis.max - l.axis.min;
	if (c->horizontal) {
		for (int r = 0; r < l.plot.height; r++)
			ctui_put(y + l.plot.y + r, x + l.label_width, 1,
				 g->box_v, border);
		for (int i = 0; i < l.axis.ticks && l.labels_row >= 0; i++) {
			double v = l.axis.min + i * l.axis.step;
			char label[32];
			ctui_axis_label(&l.axis, v, label, sizeof label);
			int w = (int)ctui_utf8_columns(label);
			int col = (int)lround((v - l.axis.min) / span_v *
					      (l.plot.width - 1));
			if (col + w > l.plot.width)
				col = l.plot.width - w;
			if (col < 0)
				continue;
			ctui_put(y + l.labels_row, x + l.plot.x + col, w, label,
				 muted);
		}
	} else {
		for (int r = 0; r < l.plot.height; r++)
			ctui_put(y + l.plot.y + r, x + l.label_width, 1,
				 g->box_v, border);
		for (int i = 0; i < l.axis.ticks; i++) {
			double v = l.axis.min + i * l.axis.step;
			int r = l.plot.height < 2
				    ? 0
				    : (int)lround((l.axis.max - v) / span_v *
						  (l.plot.height - 1));
			char label[32];
			ctui_axis_label(&l.axis, v, label, sizeof label);
			int w = (int)ctui_utf8_columns(label);
			if (w <= l.label_width)
				ctui_put(y + l.plot.y + r,
					 x + l.label_width - w, w, label,
					 muted);
		}
	}

	static const char *const UP[8] = {
	    "\xe2\x96\x81", "\xe2\x96\x82", "\xe2\x96\x83", "\xe2\x96\x84",
	    "\xe2\x96\x85", "\xe2\x96\x86", "\xe2\x96\x87", "\xe2\x96\x88"};
	static const char *const RIGHT[8] = {
	    "\xe2\x96\x8f", "\xe2\x96\x8e", "\xe2\x96\x8d", "\xe2\x96\x8c",
	    "\xe2\x96\x8b", "\xe2\x96\x8a", "\xe2\x96\x89", "\xe2\x96\x88"};
	int cells = c->horizontal ? l.plot.width : l.plot.height;
	for (size_t k = 0; k < n; k++) {
		/* names of the categories */
		span first, last;
		bool placed =
		    c->count > 0 && bar_span(c, &l, k, 0, &first) &&
		    bar_span(c, &l, k, c->stacked ? 0 : c->count - 1, &last);
		if (c->categories && c->categories[k] && placed) {
			int from = first.start, to = last.start + last.len;
			if (c->horizontal) {
				int row = from + (to - from - 1) / 2;
				int w =
				    (int)ctui_utf8_columns(c->categories[k]);
				if (w > l.label_width)
					w = l.label_width;
				ctui_put(y + l.plot.y + row,
					 x + l.label_width - w, w,
					 c->categories[k], muted);
			} else if (l.labels_row >= 0) {
				int w =
				    (int)ctui_utf8_columns(c->categories[k]);
				int room = to - from + 1;
				if (w > room)
					w = room;
				int col = from + (to - from - w) / 2;
				ctui_put(y + l.labels_row, x + l.plot.x + col,
					 w, c->categories[k], muted);
			}
		}
		for (size_t s = 0; s < c->count; s++) {
			span sp;
			long lo, hi;
			if (!bar_span(c, &l, k, s, &sp))
				continue;
			segment(c, &l, k, s, cells, &lo, &hi);
			if (hi <= lo)
				continue;
			int attr = ctui_attr_icon(c->series[s].color, false);
			for (int cell = (int)(lo / 8);
			     (long)cell * 8 < hi && cell < cells; cell++) {
				long from =
				    (long)cell * 8 > lo ? (long)cell * 8 : lo;
				long to = (long)(cell + 1) * 8 < hi
					      ? (long)(cell + 1) * 8
					      : hi;
				int cover = (int)(to - from); /* eighths of this
								 cell, 1..8 */
				for (int across = sp.start;
				     across < sp.start + sp.len; across++) {
					int row =
					    c->horizontal
						? across
						: l.plot.height - 1 - cell;
					int col = c->horizontal ? cell : across;
					const char *glyph;
					if (!unicode)
						glyph = cover >= 4 ? "#" : ".";
					else if (cover == 8 ||
						 (c->stacked &&
						  from > (long)cell * 8))
						glyph = UP[7];
					else
						glyph = c->horizontal
							    ? RIGHT[cover - 1]
							    : UP[cover - 1];
					ctui_put(y + l.plot.y + row,
						 x + l.plot.x + col, 1, glyph,
						 attr);
				}
			}
		}
	}
}

void ctui_barchart_draw(const ctui_barchart *c, int y, int x, int width,
			int height)
{
	ctui_style_begin(c->style);
	chart_draw(c, y, x, width, height);
	ctui_style_end();
}
