/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * piechart.c - pie and donut charts (piechart.h).
 */
#include <ctui/piechart.h>

#include <math.h>
#include <stdio.h>

#include <ctui/text.h>

#define LEGEND_MIN 12 /* narrowest legend worth drawing */

void ctui_piechart_init(ctui_piechart *c, const ctui_slice *slices,
			size_t count)
{
	*c = (ctui_piechart){.slices = slices, .count = count, .legend = true};
}

static double counted(const ctui_piechart *c, size_t i)
{
	double v = c->slices[i].value;
	return isnan(v) || v < 0 ? 0 : v;
}

double ctui_piechart_total(const ctui_piechart *c)
{
	double sum = 0;
	for (size_t i = 0; i < c->count; i++)
		sum += counted(c, i);
	return sum;
}

double ctui_piechart_share(const ctui_piechart *c, size_t i)
{
	double total = ctui_piechart_total(c);
	return total > 0 && i < c->count ? counted(c, i) / total : 0;
}

int ctui_piechart_slice_at(const ctui_piechart *c, double turn)
{
	double total = ctui_piechart_total(c);
	if (total <= 0)
		return -1;
	turn -= floor(turn);
	double edge = 0;
	int last = -1;
	for (size_t i = 0; i < c->count; i++) {
		double share = counted(c, i) / total;
		if (share <= 0)
			continue;
		last = (int)i;
		edge += share;
		if (turn < edge)
			return (int)i;
	}
	return last; /* rounding: the very end belongs to the last slice */
}

/* Width of the legend: marker, label, percentage. */
static int legend_width(const ctui_piechart *c, int room)
{
	int w = 0;
	for (size_t i = 0; i < c->count; i++) {
		int l = c->slices[i].label
			    ? (int)ctui_utf8_columns(c->slices[i].label)
			    : 0;
		if (l > w)
			w = l;
	}
	w += 2 + 2 + 6; /* marker and space, gap, "100.0%" */
	return w > room ? room : w;
}

bool ctui_piechart_layout_for(const ctui_piechart *c, int width, int height,
			      ctui_piechart_layout *out)
{
	if (width < 6 || height < 3)
		return false;
	ctui_piechart_layout l = {0};
	int legend = 0;
	if (c->legend && c->count > 0) {
		int room = width - (height * 2 +
				    1); /* what a full-height disc leaves */
		if (room >= LEGEND_MIN)
			legend = legend_width(c, room);
	}
	int disc_cols = width - (legend ? legend + 1 : 0);
	int diameter =
	    height * 4 < disc_cols * 2 ? height * 4 : disc_cols * 2; /* dots */
	int cols = (diameter + 1) / 2, rows = (diameter + 3) / 4;
	if (cols < 1 || rows < 1)
		return false;
	l.pie = (ctui_rect){
	    .y = (height - rows) / 2, .x = 0, .width = cols, .height = rows};
	l.radius = diameter / 2.0;
	if (legend) {
		int shown = (int)c->count < height ? (int)c->count : height;
		l.legend = (ctui_rect){.y = (height - shown) / 2,
				       .x = cols + 1,
				       .width = legend,
				       .height = shown};
	}
	*out = l;
	return true;
}

/* Slice at a dot of the disc, or -1 when the dot is not on it. */
static int slice_at_dot(const ctui_piechart *c, const ctui_piechart_layout *l,
			double px, double py)
{
	double cx = l->pie.width,
	       cy = l->pie.height * 2.0; /* centre of the cells, in dots */
	double dx = px - cx, dy = py - cy;
	double r2 = dx * dx + dy * dy;
	double r = l->radius;
	if (r2 > r * r)
		return -1;
	if (c->hole > 0 && c->hole < 1 && r2 < (r * c->hole) * (r * c->hole))
		return -1;
	double turn = atan2(dx, -dy) / (2 * M_PI);
	return ctui_piechart_slice_at(c, turn < 0 ? turn + 1 : turn);
}

int ctui_piechart_hit(const ctui_piechart *c, int width, int height, int y,
		      int x)
{
	ctui_piechart_layout l;
	if (!ctui_piechart_layout_for(c, width, height, &l))
		return -1;
	if (l.legend.width > 0 && y >= l.legend.y &&
	    y < l.legend.y + l.legend.height && x >= l.legend.x &&
	    x < l.legend.x + l.legend.width)
		return y - l.legend.y;
	int cx = x - l.pie.x, cy = y - l.pie.y;
	if (cx < 0 || cy < 0 || cx >= l.pie.width || cy >= l.pie.height)
		return -1;
	return slice_at_dot(c, &l, cx * 2 + 1.0, cy * 4 + 2.0);
}

static void chart_draw(const ctui_piechart *c, int y, int x, int width,
		       int height)
{
	ctui_piechart_layout l;
	if (!ctui_piechart_layout_for(c, width, height, &l))
		return;
	ctui_canvas canvas;
	if (ctui_canvas_init(&canvas, l.pie.width, l.pie.height)) {
		for (int py = 0; py < ctui_canvas_height(&canvas); py++)
			for (int px = 0; px < ctui_canvas_width(&canvas);
			     px++) {
				int s = slice_at_dot(c, &l, px + 0.5, py + 0.5);
				if (s >= 0)
					ctui_canvas_set(&canvas, px, py,
							c->slices[s].color);
			}
		ctui_canvas_draw(&canvas, y + l.pie.y, x + l.pie.x);
		ctui_canvas_free(&canvas);
	}
	if (l.legend.width <= 0)
		return;
	const ctui_glyphs *g = ctui_theme_glyphs();
	for (int i = 0; i < l.legend.height; i++) {
		const ctui_slice *s = &c->slices[i];
		int row = y + l.legend.y + i, col = x + l.legend.x;
		char pct[16];
		snprintf(pct, sizeof pct, "%.1f%%",
			 ctui_piechart_share(c, (size_t)i) * 100);
		int pw = (int)ctui_utf8_columns(pct);
		ctui_put(row, col, 1, g->knob, ctui_attr_icon(s->color, false));
		ctui_put(row, col + 2, l.legend.width - 2 - pw - 1,
			 s->label ? s->label : "",
			 ctui_attr(CTUI_PAIR_BODY, false));
		ctui_put(row, col + l.legend.width - pw, pw, pct,
			 ctui_attr(CTUI_PAIR_MUTED, false));
	}
}

void ctui_piechart_draw(const ctui_piechart *c, int y, int x, int width,
			int height)
{
	ctui_style_begin(c->style);
	chart_draw(c, y, x, width, height);
	ctui_style_end();
}
