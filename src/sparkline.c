/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * sparkline.c - a one-row chart of a series (sparkline.h).
 */
#include <ctui/sparkline.h>

#include <math.h>

#include <ctui/text.h>

int ctui_sparkline_level(double v, double lo, double hi, int levels)
{
	if (levels < 1 || !(hi > lo))
		return 0;
	double t = (v - lo) / (hi - lo);
	if (t < 0)
		t = 0;
	if (t > 1)
		t = 1;
	int level = (int)(t * levels);
	return level >= levels ? levels - 1 : level;
}

int ctui_sparkline_draw(int y, int x, int width, const double *values,
			size_t count, double lo, double hi,
			ctui_icon_color color)
{
	return ctui_sparkline_draw_rows(y, x, width, 1, values, count, lo, hi,
					color);
}

int ctui_sparkline_draw_rows(int y, int x, int width, int height,
			     const double *values, size_t count, double lo,
			     double hi, ctui_icon_color color)
{
	static const char *const BLOCKS[CTUI_SPARKLINE_LEVELS] = {
	    "\xe2\x96\x81", "\xe2\x96\x82", "\xe2\x96\x83", "\xe2\x96\x84",
	    "\xe2\x96\x85", "\xe2\x96\x86", "\xe2\x96\x87", "\xe2\x96\x88"};
	static const char *const PLAIN[CTUI_SPARKLINE_LEVELS] = {
	    "_", ",", ".", "-", "~", "=", "*", "#"};
	if (width < 1 || height < 1 || count == 0)
		return 0;
	size_t shown = count < (size_t)width ? count : (size_t)width;
	const double *v = values + (count - shown);
	if (!(lo < hi)) {
		lo = HUGE_VAL;
		hi = -HUGE_VAL;
		for (size_t i = 0; i < shown; i++) {
			if (isnan(v[i]))
				continue;
			if (v[i] < lo)
				lo = v[i];
			if (v[i] > hi)
				hi = v[i];
		}
	}
	const char *const *glyphs = ctui_theme_unicode() ? BLOCKS : PLAIN;
	int attr = ctui_attr_icon(color, false);
	int levels = height * CTUI_SPARKLINE_LEVELS;
	for (size_t i = 0; i < shown; i++) {
		if (isnan(v[i]))
			continue;
		/* eighths lit, from 1 at the bottom of the range to all of
		 * them; a flat series is drawn at mid height */
		int lit = (hi > lo ? ctui_sparkline_level(v[i], lo, hi, levels)
				   : levels / 2) +
			  1;
		for (int row = 0;
		     row < height && lit > row * CTUI_SPARKLINE_LEVELS; row++) {
			int part = lit - row * CTUI_SPARKLINE_LEVELS;
			if (part > CTUI_SPARKLINE_LEVELS)
				part = CTUI_SPARKLINE_LEVELS;
			ctui_put(y + height - 1 - row, x + (int)i, 1,
				 glyphs[part - 1], attr);
		}
	}
	return (int)shown;
}
