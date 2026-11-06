/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * linechart.c - line chart of one or more series (linechart.h).
 */
#include <ctui/linechart.h>

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <ctui/text.h>

void ctui_linechart_init(ctui_linechart *c, const ctui_series *series,
			 size_t count)
{
	*c = (ctui_linechart){.series = series, .count = count, .legend = true};
}

size_t ctui_linechart_points(const ctui_linechart *c)
{
	size_t n = 0;
	for (size_t i = 0; i < c->count; i++)
		if (c->series[i].count > n)
			n = c->series[i].count;
	return n;
}

ctui_axis ctui_linechart_axis(const ctui_linechart *c, int rows)
{
	int ticks = rows / 2 + 1;
	if (c->fixed)
		return ctui_axis_nice(c->min, c->max, ticks);
	double lo = HUGE_VAL, hi = -HUGE_VAL;
	for (size_t i = 0; i < c->count; i++)
		for (size_t k = 0; k < c->series[i].count; k++) {
			double v = c->series[i].values[k];
			if (isnan(v))
				continue;
			if (v < lo)
				lo = v;
			if (v > hi)
				hi = v;
		}
	if (lo > hi)
		lo = 0, hi = 1;
	return ctui_axis_nice(lo, hi, ticks);
}

/* Columns of the widest series name, with its marker and the space after. */
static int legend_entry_width(const ctui_linechart *c)
{
	int w = 0;
	for (size_t i = 0; i < c->count; i++) {
		int n = c->series[i].name
			    ? (int)ctui_utf8_columns(c->series[i].name)
			    : 0;
		if (n > w)
			w = n;
	}
	return w + 2;
}

bool ctui_linechart_layout_for(const ctui_linechart *c, int width, int height,
			       ctui_linechart_layout *out)
{
	if (width < 8 || height < 3)
		return false;
	ctui_linechart_layout l = {.legend_row = -1,
				   .legend_col = -1,
				   .labels_row = -1,
				   .x_title_row = -1,
				   .y_title_row = -1};
	bool legend =
	    c->legend && c->count > 0 && c->legend_pos != CTUI_LEGEND_NONE;
	bool side = legend && c->legend_pos == CTUI_LEGEND_RIGHT;
	bool want_labels = c->x_labels && c->x_label_count > 0;
	int top = 0, bottom = 0;
	bool legend_top = false, legend_bottom = false, labels = false;
	if (legend && !side && height >= 5) {
		legend_top = c->legend_pos == CTUI_LEGEND_TOP;
		legend_bottom = !legend_top;
		if (legend_top)
			top++;
		else
			bottom++;
	}
	if (want_labels && height >= 5)
		labels = true, bottom++;
	bool x_title = false;
	if (c->x_title && *c->x_title && height - top - bottom - 1 >= 3)
		x_title = true, bottom++;
	if (c->y_title && *c->y_title && height - top - bottom - 1 >= 3) {
		l.y_title_row = top++;
	}
	if (legend_top)
		l.legend_row = 0;
	int rows = height - top - bottom;
	/* from the plot downwards: x labels, x title, legend */
	int next = top + rows;
	if (labels)
		l.labels_row = next++;
	if (x_title)
		l.x_title_row = next++;
	if (legend_bottom)
		l.legend_row = next++;

	l.axis = ctui_linechart_axis(c, rows);
	for (int i = 0; i < l.axis.ticks; i++) {
		char label[32];
		ctui_axis_label(&l.axis, l.axis.min + i * l.axis.step, label,
				sizeof label);
		int w = (int)ctui_utf8_columns(label);
		if (w > l.label_width)
			l.label_width = w;
	}
	if (l.label_width > width / 3)
		l.label_width = width / 3;
	int plot_width = width - l.label_width - 1;
	if (side) {
		int w = legend_entry_width(c);
		if (w > width / 3)
			w = width / 3;
		if (plot_width - w - 1 >=
		    8) { /* a column of space between plot and legend */
			plot_width -= w + 1;
			l.legend_col = width - w;
			l.legend_width = w;
		}
	}
	l.plot = (ctui_rect){.y = top,
			     .x = l.label_width + 1,
			     .width = plot_width,
			     .height = rows};
	if (l.plot.width < 2)
		return false;
	*out = l;
	return true;
}

/* Dot column of point i of n. */
static int dot_x(int i, size_t n, int dots)
{
	if (n < 2)
		return dots / 2;
	return (int)((double)i * (dots - 1) / (double)(n - 1) + 0.5);
}

int ctui_linechart_index_at(const ctui_linechart *c, int width, int height,
			    int x)
{
	ctui_linechart_layout l;
	size_t n = ctui_linechart_points(c);
	if (n == 0 || !ctui_linechart_layout_for(c, width, height, &l))
		return -1;
	int col = x - l.plot.x;
	if (col < 0 || col >= l.plot.width)
		return -1;
	if (n < 2)
		return 0;
	int dots = l.plot.width * 2;
	double i = (col * 2 + 0.5) * (double)(n - 1) / (dots - 1);
	long k = lround(i);
	return k < 0 ? 0 : k >= (long)n ? (int)n - 1 : (int)k;
}

static void chart_draw(const ctui_linechart *c, int y, int x, int width,
		       int height)
{
	ctui_linechart_layout l;
	if (!ctui_linechart_layout_for(c, width, height, &l))
		return;
	const ctui_glyphs *g = ctui_theme_glyphs();
	int muted = ctui_attr(CTUI_PAIR_MUTED, false);
	int border = ctui_attr(CTUI_PAIR_BORDER, false);

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
	if (l.legend_col >= 0) {
		for (size_t i = 0; i < c->count && (int)i < l.plot.height;
		     i++) {
			int row = y + l.plot.y + (int)i, w = l.legend_width;
			int col =
			    ctui_put(row, x + l.legend_col, w, g->knob,
				     ctui_attr_icon(c->series[i].color, false));
			col += ctui_put(row, x + l.legend_col + col, w - col,
					" ", muted);
			ctui_put(row, x + l.legend_col + col, w - col,
				 c->series[i].name ? c->series[i].name : "",
				 ctui_attr(CTUI_PAIR_BODY, false));
		}
	}
	if (l.y_title_row >= 0)
		ctui_put(y + l.y_title_row, x, width, c->y_title, muted);
	if (l.x_title_row >= 0) {
		int w = (int)ctui_utf8_columns(c->x_title);
		if (w > l.plot.width)
			w = l.plot.width;
		ctui_put(y + l.x_title_row,
			 x + l.plot.x + (l.plot.width - w) / 2, w, c->x_title,
			 muted);
	}

	double span = l.axis.max - l.axis.min;
	for (int r = 0; r < l.plot.height; r++)
		ctui_put(y + l.plot.y + r, x + l.label_width, 1, g->box_v,
			 border);
	for (int i = 0; i < l.axis.ticks; i++) {
		double v = l.axis.min + i * l.axis.step;
		int r = l.plot.height < 2
			    ? 0
			    : (int)lround((l.axis.max - v) / span *
					  (l.plot.height - 1));
		char label[32];
		ctui_axis_label(&l.axis, v, label, sizeof label);
		int w = (int)ctui_utf8_columns(label);
		if (w > l.label_width)
			continue;
		ctui_put(y + l.plot.y + r, x + l.label_width - w, w, label,
			 muted);
	}

	ctui_canvas canvas;
	if (ctui_canvas_init(&canvas, l.plot.width, l.plot.height)) {
		int dots_w = ctui_canvas_width(&canvas),
		    dots_h = ctui_canvas_height(&canvas);
		size_t n = ctui_linechart_points(c);
		for (size_t s = 0; s < c->count; s++) {
			const ctui_series *ser = &c->series[s];
			bool have = false;
			int px = 0, py = 0;
			for (size_t i = 0; i < ser->count; i++) {
				double v = ser->values[i];
				if (isnan(v)) {
					have = false;
					continue;
				}
				double t = (l.axis.max - v) / span;
				if (t < 0)
					t = 0;
				if (t > 1)
					t = 1;
				int cx = dot_x((int)i, n, dots_w),
				    cy = (int)lround(t * (dots_h - 1));
				if (ser->mode != CTUI_SERIES_POINTS && have)
					ctui_canvas_line(&canvas, px, py, cx,
							 cy, ser->color);
				else if (ser->mode != CTUI_SERIES_POINTS)
					ctui_canvas_set(&canvas, cx, cy,
							ser->color);
				if (ser->mode != CTUI_SERIES_LINE)
					ctui_canvas_circle(&canvas, cx, cy, 1,
							   ser->color, true);
				px = cx;
				py = cy;
				have = true;
			}
		}
		ctui_canvas_draw(&canvas, y + l.plot.y, x + l.plot.x);
		ctui_canvas_free(&canvas);
	}

	if (l.labels_row >= 0) {
		size_t last = c->x_label_count - 1;
		const char *first = c->x_labels[0], *end = c->x_labels[last];
		if (first)
			ctui_put(y + l.labels_row, x + l.plot.x, l.plot.width,
				 first, muted);
		if (end && last > 0) {
			int w = (int)ctui_utf8_columns(end);
			int col = l.plot.width - w;
			int first_w = first ? (int)ctui_utf8_columns(first) : 0;
			if (col > first_w)
				ctui_put(y + l.labels_row, x + l.plot.x + col,
					 w, end, muted);
		}
	}
}

void ctui_linechart_draw(const ctui_linechart *c, int y, int x, int width,
			 int height)
{
	ctui_style_begin(c->style);
	chart_draw(c, y, x, width, height);
	ctui_style_end();
}
