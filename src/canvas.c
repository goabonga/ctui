/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * canvas.c - a dot canvas for plotting, and nice axis scales (canvas.h).
 */
#include <ctui/canvas.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include <ctui/text.h>

bool ctui_canvas_init(ctui_canvas *c, int cols, int rows)
{
	*c = (ctui_canvas){0};
	if (cols < 1 || rows < 1)
		return false;
	size_t n = (size_t)cols * (size_t)rows;
	c->dots = calloc(n, 1);
	c->color = calloc(n, 1);
	if (!c->dots || !c->color) {
		ctui_canvas_free(c);
		return false;
	}
	c->cols = cols;
	c->rows = rows;
	return true;
}

void ctui_canvas_free(ctui_canvas *c)
{
	free(c->dots);
	free(c->color);
	*c = (ctui_canvas){0};
}

void ctui_canvas_clear(ctui_canvas *c)
{
	size_t n = (size_t)c->cols * (size_t)c->rows;
	for (size_t i = 0; i < n; i++)
		c->dots[i] = c->color[i] = 0;
}

int ctui_canvas_width(const ctui_canvas *c)
{
	return c->cols * 2;
}

int ctui_canvas_height(const ctui_canvas *c)
{
	return c->rows * 4;
}

/* Braille bit of the dot (dx, dy) of a cell. */
static unsigned char bit(int dx, int dy)
{
	static const unsigned char BITS[4][2] = {
	    {0x01, 0x08}, {0x02, 0x10}, {0x04, 0x20}, {0x40, 0x80}};
	return BITS[dy][dx];
}

void ctui_canvas_set(ctui_canvas *c, int x, int y, ctui_icon_color color)
{
	if (x < 0 || y < 0 || x >= ctui_canvas_width(c) ||
	    y >= ctui_canvas_height(c))
		return;
	size_t cell = (size_t)(y / 4) * (size_t)c->cols + (size_t)(x / 2);
	c->dots[cell] |= bit(x % 2, y % 4);
	c->color[cell] = (unsigned char)(color + 1);
}

bool ctui_canvas_get(const ctui_canvas *c, int x, int y)
{
	if (x < 0 || y < 0 || x >= ctui_canvas_width(c) ||
	    y >= ctui_canvas_height(c))
		return false;
	size_t cell = (size_t)(y / 4) * (size_t)c->cols + (size_t)(x / 2);
	return (c->dots[cell] & bit(x % 2, y % 4)) != 0;
}

void ctui_canvas_line(ctui_canvas *c, int x0, int y0, int x1, int y1,
		      ctui_icon_color color)
{
	int dx = abs(x1 - x0), dy = -abs(y1 - y0);
	int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
	int err = dx + dy;
	for (;;) {
		ctui_canvas_set(c, x0, y0, color);
		if (x0 == x1 && y0 == y1)
			return;
		int e2 = 2 * err;
		if (e2 >= dy) {
			err += dy;
			x0 += sx;
		}
		if (e2 <= dx) {
			err += dx;
			y0 += sy;
		}
	}
}

#define OUT_LEFT 1U
#define OUT_RIGHT 2U
#define OUT_TOP 4U
#define OUT_BOTTOM 8U

static unsigned region(double x, double y, double xmax, double ymax)
{
	return (x < 0 ? OUT_LEFT : 0) | (x > xmax ? OUT_RIGHT : 0) |
	       (y < 0 ? OUT_TOP : 0) | (y > ymax ? OUT_BOTTOM : 0);
}

void ctui_canvas_clipped_line(ctui_canvas *c, double x0, double y0, double x1,
			      double y1, ctui_icon_color color)
{
	if (!isfinite(x0) || !isfinite(y0) || !isfinite(x1) || !isfinite(y1))
		return;
	double xmax = ctui_canvas_width(c) - 1,
	       ymax = ctui_canvas_height(c) - 1;
	unsigned code0 = region(x0, y0, xmax, ymax),
		 code1 = region(x1, y1, xmax, ymax);
	for (int guard = 0; guard < 16;
	     guard++) { /* each pass moves one end onto an edge */
		if (!(code0 | code1)) {
			ctui_canvas_line(c, (int)lround(x0), (int)lround(y0),
					 (int)lround(x1), (int)lround(y1),
					 color);
			return;
		}
		if (code0 & code1)
			return;
		unsigned code = code0 ? code0 : code1;
		double x, y;
		if (code & OUT_BOTTOM) {
			x = x0 + (x1 - x0) * (ymax - y0) / (y1 - y0);
			y = ymax;
		} else if (code & OUT_TOP) {
			x = x0 + (x1 - x0) * (0 - y0) / (y1 - y0);
			y = 0;
		} else if (code & OUT_RIGHT) {
			y = y0 + (y1 - y0) * (xmax - x0) / (x1 - x0);
			x = xmax;
		} else {
			y = y0 + (y1 - y0) * (0 - x0) / (x1 - x0);
			x = 0;
		}
		if (code == code0) {
			x0 = x;
			y0 = y;
			code0 = region(x0, y0, xmax, ymax);
		} else {
			x1 = x;
			y1 = y;
			code1 = region(x1, y1, xmax, ymax);
		}
	}
}

void ctui_canvas_rect(ctui_canvas *c, int x0, int y0, int x1, int y1,
		      ctui_icon_color color, bool filled)
{
	if (x0 > x1) {
		int t = x0;
		x0 = x1;
		x1 = t;
	}
	if (y0 > y1) {
		int t = y0;
		y0 = y1;
		y1 = t;
	}
	/* Stay inside the canvas so a huge rectangle costs no more than the
	 * canvas. */
	int cx0 = x0 < 0 ? 0 : x0,
	    cx1 = x1 >= ctui_canvas_width(c) ? ctui_canvas_width(c) - 1 : x1;
	int cy0 = y0 < 0 ? 0 : y0,
	    cy1 = y1 >= ctui_canvas_height(c) ? ctui_canvas_height(c) - 1 : y1;
	for (int y = cy0; y <= cy1; y++)
		for (int x = cx0; x <= cx1; x++)
			if (filled || y == y0 || y == y1 || x == x0 || x == x1)
				ctui_canvas_set(c, x, y, color);
}

void ctui_canvas_circle(ctui_canvas *c, int cx, int cy, int r,
			ctui_icon_color color, bool filled)
{
	if (r < 0)
		return;
	int x = r, y = 0, err = 1 - r;
	while (x >= y) {
		if (filled) {
			ctui_canvas_line(c, cx - x, cy + y, cx + x, cy + y,
					 color);
			ctui_canvas_line(c, cx - x, cy - y, cx + x, cy - y,
					 color);
			ctui_canvas_line(c, cx - y, cy + x, cx + y, cy + x,
					 color);
			ctui_canvas_line(c, cx - y, cy - x, cx + y, cy - x,
					 color);
		} else {
			ctui_canvas_set(c, cx + x, cy + y, color);
			ctui_canvas_set(c, cx - x, cy + y, color);
			ctui_canvas_set(c, cx + x, cy - y, color);
			ctui_canvas_set(c, cx - x, cy - y, color);
			ctui_canvas_set(c, cx + y, cy + x, color);
			ctui_canvas_set(c, cx - y, cy + x, color);
			ctui_canvas_set(c, cx + y, cy - x, color);
			ctui_canvas_set(c, cx - y, cy - x, color);
		}
		y++;
		if (err < 0) {
			err += 2 * y + 1;
		} else {
			x--;
			err += 2 * (y - x) + 1;
		}
	}
}

void ctui_canvas_polyline(ctui_canvas *c, const ctui_dot *points, size_t n,
			  ctui_icon_color color)
{
	if (n == 1)
		ctui_canvas_set(c, points[0].x, points[0].y, color);
	for (size_t i = 1; i < n; i++)
		ctui_canvas_line(c, points[i - 1].x, points[i - 1].y,
				 points[i].x, points[i].y, color);
}

void ctui_canvas_glyph(unsigned char dots, char out[4])
{
	unsigned cp = 0x2800u + dots;
	out[0] = (char)(0xE0U | (cp >> 12U));
	out[1] = (char)(0x80U | ((cp >> 6U) & 0x3FU));
	out[2] = (char)(0x80U | (cp & 0x3FU));
	out[3] = '\0';
}

void ctui_canvas_draw(const ctui_canvas *c, int y, int x)
{
	bool unicode = ctui_theme_unicode();
	for (int r = 0; r < c->rows; r++)
		for (int k = 0; k < c->cols; k++) {
			size_t cell = (size_t)r * (size_t)c->cols + (size_t)k;
			if (!c->dots[cell])
				continue;
			char glyph[4] = "*";
			if (unicode)
				ctui_canvas_glyph(c->dots[cell], glyph);
			ctui_put(
			    y + r, x + k, 1, glyph,
			    ctui_attr_icon(
				(ctui_icon_color)(c->color[cell] - 1), false));
		}
}

/* The 1, 2, 5 or 10 times a power of ten nearest to v (rounding the
 * fraction when round is set, else going up). */
static double nice_number(double v, bool round)
{
	double exponent = floor(log10(v));
	double fraction = v / pow(10, exponent);
	double nice;
	if (round)
		nice = fraction < 1.5 ? 1
		       : fraction < 3 ? 2
		       : fraction < 7 ? 5
				      : 10;
	else
		nice = fraction <= 1   ? 1
		       : fraction <= 2 ? 2
		       : fraction <= 5 ? 5
				       : 10;
	return nice * pow(10, exponent);
}

ctui_axis ctui_axis_nice(double lo, double hi, int max_ticks)
{
	if (max_ticks < 2)
		max_ticks = 2;
	if (hi < lo) {
		double t = lo;
		lo = hi;
		hi = t;
	}
	if (hi - lo < 1e-12) {
		double pad = fabs(lo) > 1e-12 ? fabs(lo) * 0.5 : 1;
		lo -= pad;
		hi += pad;
	}
	double range = nice_number(hi - lo, false);
	double step = nice_number(range / (max_ticks - 1), true);
	ctui_axis axis = {.step = step};
	axis.min = floor(lo / step + 1e-9) * step;
	axis.max = ceil(hi / step - 1e-9) * step;
	axis.ticks = (int)lround((axis.max - axis.min) / step) + 1;
	while (axis.ticks >
	       max_ticks) { /* rounding gave too many: take a coarser step */
		step = nice_number(step * 1.01, false);
		axis.step = step;
		axis.min = floor(lo / step + 1e-9) * step;
		axis.max = ceil(hi / step - 1e-9) * step;
		axis.ticks = (int)lround((axis.max - axis.min) / step) + 1;
	}
	return axis;
}

void ctui_axis_label(const ctui_axis *axis, double value, char *out, size_t cap)
{
	int decimals = 0;
	if (axis->step < 1)
		decimals = (int)ceil(-log10(axis->step) - 1e-9);
	if (decimals > 6)
		decimals = 6;
	if (fabs(value) < axis->step * 1e-6)
		value = 0; /* no "-0" */
	snprintf(out, cap, "%.*f", decimals, value);
}
