/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * heatmap.c - grid of values drawn with a colour ramp (heatmap.h).
 */
#include <ctui/heatmap.h>

#include <curses.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>

#include <ctui/text.h>

static const char DENSITY[CTUI_HEATMAP_STEPS + 1] = ".:=*#@";

void ctui_heatmap_init(ctui_heatmap *h, const double *values, size_t rows,
		       size_t cols)
{
	*h = (ctui_heatmap){.values = values,
			    .rows = rows,
			    .cols = cols,
			    .legend = true,
			    .show_cursor = true};
}

bool ctui_heatmap_range(const ctui_heatmap *h, double *lo, double *hi)
{
	if (h->fixed) {
		*lo = h->min;
		*hi = h->max;
		return true;
	}
	*lo = 0;
	*hi = 1;
	if (!h->values || h->cols == 0 || h->rows > SIZE_MAX / h->cols)
		return false;
	bool any = false;
	for (size_t i = 0; i < h->rows * h->cols; i++) {
		double v = h->values[i];
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

int ctui_heatmap_step(double value, double lo, double hi, int steps)
{
	if (isnan(value))
		return -1;
	if (steps < 1 || !(hi > lo) || value <= lo)
		return 0;
	if (value >= hi)
		return steps - 1;
	int s = (int)((value - lo) / (hi - lo) * steps);
	return s < 0 ? 0 : s >= steps ? steps - 1 : s;
}

static const char *label_of(const char *const *labels, size_t i)
{
	return labels && labels[i] ? labels[i] : "";
}

bool ctui_heatmap_layout_for(const ctui_heatmap *h, int width, int height,
			     ctui_heatmap_layout *out)
{
	if (!h->values || h->rows == 0 || h->cols == 0 || h->rows > INT_MAX ||
	    h->cols > INT_MAX || width < 1 || height < 1)
		return false;
	ctui_heatmap_layout l = {.header_row = -1, .legend_row = -1};
	int top = 0, bottom = 0;
	if (h->col_labels && height >= 3)
		l.header_row = top++;
	if (h->legend && height >= 3 + top)
		l.legend_row = height - 1, bottom = 1;
	if (h->row_labels) {
		size_t w = 0;
		for (size_t r = 0; r < h->rows; r++) {
			size_t n =
			    ctui_utf8_columns(label_of(h->row_labels, r));
			if (n > w)
				w = n;
		}
		if (w > (size_t)width / 3)
			w = (size_t)width / 3;
		l.label_width =
		    w > 0 ? (int)w + 1 : 0; /* and a space before the cells */
	}
	int gw = width - l.label_width, gh = height - top - bottom;
	if (gw < (int)h->cols || gh < (int)h->rows)
		return false;
	l.cell_w = gw / (int)h->cols;
	l.cell_h = gh / (int)h->rows;
	if (l.cell_w > 8)
		l.cell_w = 8;
	if (l.cell_h > 4)
		l.cell_h = 4;
	l.grid = (ctui_rect){.y = top,
			     .x = l.label_width,
			     .width = l.cell_w * (int)h->cols,
			     .height = l.cell_h * (int)h->rows};
	*out = l;
	return true;
}

bool ctui_heatmap_hit(const ctui_heatmap *h, int width, int height, int y,
		      int x, size_t *row, size_t *col, double *value)
{
	ctui_heatmap_layout l;
	if (!ctui_heatmap_layout_for(h, width, height, &l))
		return false;
	int px = x - l.grid.x, py = y - l.grid.y;
	if (px < 0 || py < 0 || px >= l.grid.width || py >= l.grid.height)
		return false;
	size_t r = (size_t)(py / l.cell_h), c = (size_t)(px / l.cell_w);
	if (row)
		*row = r;
	if (col)
		*col = c;
	if (value)
		*value = h->values[r * h->cols + c];
	return true;
}

bool ctui_heatmap_key(ctui_heatmap *h, int key)
{
	bool has = h->rows > 0 && h->cols > 0;
	switch (key) {
	case KEY_UP:
		if (h->cursor_row > 0)
			h->cursor_row--;
		return true;
	case KEY_DOWN:
		if (h->cursor_row + 1 < h->rows)
			h->cursor_row++;
		return true;
	case KEY_LEFT:
		if (h->cursor_col > 0)
			h->cursor_col--;
		return true;
	case KEY_RIGHT:
		if (h->cursor_col + 1 < h->cols)
			h->cursor_col++;
		return true;
	case KEY_HOME:
		h->cursor_row = h->cursor_col = 0;
		return true;
	case KEY_END:
		h->cursor_row = has ? h->rows - 1 : 0;
		h->cursor_col = has ? h->cols - 1 : 0;
		return true;
	default:
		return false;
	}
}

/* The colour of a step of the ramp, as a background. */
static int step_attr(int step)
{
	const ctui_palette *p = ctui_theme_palette();
	const int ramp[CTUI_HEATMAP_STEPS] = {p->info,	p->accent, p->cyan,
					      p->green, p->yellow, p->red};
	return ctui_attr_color(-1, ramp[step]);
}

static void run_of(char *out, int w, char ch)
{
	if (w > 15)
		w = 15;
	for (int i = 0; i < w; i++)
		out[i] = ch;
	out[w] = '\0';
}

static void heat_draw(const ctui_heatmap *h, int y, int x, int width,
		      int height)
{
	ctui_heatmap_layout l;
	if (!ctui_heatmap_layout_for(h, width, height, &l))
		return;
	bool ramp = ctui_theme_unicode() && ctui_theme_has_colors();
	int muted = ctui_attr(CTUI_PAIR_MUTED, false);
	int body = ctui_attr(CTUI_PAIR_BODY, false);
	double lo, hi;
	ctui_heatmap_range(h, &lo, &hi);
	char run[16];

	for (size_t c = 0; l.header_row >= 0 && c < h->cols; c++)
		ctui_put(y + l.header_row, x + l.grid.x + (int)c * l.cell_w,
			 l.cell_w, label_of(h->col_labels, c), muted);
	for (size_t r = 0; r < h->rows; r++) {
		if (l.label_width > 0)
			ctui_put(y + l.grid.y + (int)r * l.cell_h +
				     (l.cell_h - 1) / 2,
				 x, l.label_width - 1,
				 label_of(h->row_labels, r), muted);
		for (size_t c = 0; c < h->cols; c++) {
			int step =
			    ctui_heatmap_step(h->values[r * h->cols + c], lo,
					      hi, CTUI_HEATMAP_STEPS);
			bool cursor = h->show_cursor && r == h->cursor_row &&
				      c == h->cursor_col;
			char ch = step < 0 || ramp ? ' ' : DENSITY[step];
			int attr = cursor ? ctui_attr(CTUI_PAIR_PLAYHEAD, false)
				   : step < 0 ? 0
				   : ramp     ? step_attr(step)
					      : body;
			run_of(run, l.cell_w, ch);
			for (int k = 0; k < l.cell_h; k++)
				ctui_put(y + l.grid.y + (int)r * l.cell_h + k,
					 x + l.grid.x + (int)c * l.cell_w,
					 l.cell_w, run, attr);
		}
	}
	if (l.legend_row >= 0) {
		char lo_s[32], hi_s[32];
		snprintf(lo_s, sizeof lo_s, "%.4g", lo);
		snprintf(hi_s, sizeof hi_s, "%.4g", hi);
		int row = y + l.legend_row, end = x + width, col = x;
		col += ctui_put(row, col, end - col, lo_s, muted);
		col += ctui_put(row, col, end - col, " ", muted);
		for (int s = 0; s < CTUI_HEATMAP_STEPS && col < end; s++) {
			run_of(run, 2, ramp ? ' ' : DENSITY[s]);
			col += ctui_put(row, col, end - col, run,
					ramp ? step_attr(s) : body);
		}
		col += ctui_put(row, col, end - col, " ", muted);
		ctui_put(row, col, end - col, hi_s, muted);
	}
}

void ctui_heatmap_draw(const ctui_heatmap *h, int y, int x, int width,
		       int height)
{
	ctui_style_begin(h->style);
	heat_draw(h, y, x, width, height);
	ctui_style_end();
}
