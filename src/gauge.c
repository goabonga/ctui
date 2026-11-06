/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * gauge.c - horizontal bars drawn with block characters (gauge.h).
 */
#include <ctui/gauge.h>

#include <curses.h>
#include <math.h>
#include <string.h>

#include <ctui/labels.h>
#include <ctui/text.h>

unsigned ctui_gauge_zone(size_t cell, size_t width)
{
	if (!width)
		return 0;
	double percent = (double)cell / (double)width * 100;
	return percent < 20   ? 0
	       : percent < 40 ? 1
	       : percent < 60 ? 2
	       : percent < 80 ? 3
	       : percent < 90 ? 4
			      : 5;
}

ctui_pair ctui_gauge_zone_pair(unsigned zone)
{
	static const ctui_pair pairs[] = {
	    CTUI_PAIR_GAUGE_GREEN,  CTUI_PAIR_GAUGE_GREEN_BRIGHT,
	    CTUI_PAIR_GAUGE_YELLOW, CTUI_PAIR_GAUGE_YELLOW_BRIGHT,
	    CTUI_PAIR_GAUGE_RED,    CTUI_PAIR_GAUGE_RED_BRIGHT,
	};
	return pairs[zone > 5 ? 5 : zone];
}

size_t ctui_gauge_filled(bool present, double value, double maximum,
			 size_t width)
{
	if (!present || !(maximum > 0.0))
		return 0;
	double ratio = value / maximum;
	if (!(ratio > 0.0)) /* negative, zero or NaN */
		ratio = 0.0;
	if (ratio > 1.0)
		ratio = 1.0;
	size_t filled = (size_t)(ratio * (double)width);
	return filled > width ? width : filled;
}

char *ctui_gauge_bar(char *buf, size_t cap, bool present, double value,
		     double maximum, size_t width, const char *filled,
		     const char *empty)
{
	if (!buf || cap == 0)
		return buf;
	if (!filled)
		filled = ctui_theme_glyphs()->gauge_filled;
	if (!empty)
		empty = ctui_theme_glyphs()->gauge_empty;
	size_t nf = ctui_gauge_filled(present, value, maximum, width);
	size_t lf = strlen(filled), le = strlen(empty);
	size_t used = 0;
	buf[0] = '\0';
	for (size_t i = 0; i < width; i++) {
		const char *glyph = i < nf ? filled : empty;
		size_t len = i < nf ? lf : le;
		if (used + len >= cap)
			break; /* cut on a glyph boundary */
		memcpy(buf + used, glyph, len);
		used += len;
	}
	buf[used] = '\0';
	return buf;
}

ctui_pair ctui_gauge_level_pair(double percent)
{
	return percent >= 90   ? CTUI_PAIR_GAUGE_RED
	       : percent >= 70 ? CTUI_PAIR_GAUGE_YELLOW
			       : CTUI_PAIR_GAUGE_GREEN;
}

int ctui_gauge_draw(int y, int x, int width, bool present, double value,
		    double maximum)
{
	const ctui_glyphs *g = ctui_theme_glyphs();
	size_t filled = ctui_gauge_filled(present, value, maximum,
					  (size_t)(width > 0 ? width : 0));
	int used = 0;
	for (int i = 0; i < width; i++) {
		bool lit = (size_t)i < filled;
		unsigned zone = ctui_gauge_zone((size_t)i, (size_t)width);
		int attr = (int)((unsigned)ctui_attr(ctui_gauge_zone_pair(zone),
						     lit && zone % 2) |
				 (lit ? 0U : (unsigned)A_DIM));
		used += ctui_put(y, x + used, width - used,
				 lit ? g->gauge_filled : g->gauge_empty, attr);
	}
	return used;
}

int ctui_gauge_labeled(int y, int x, int width, const char *label,
		       double percent, bool valid, const char *value,
		       int text_attr)
{
	if (width < 20)
		return 0;
	const ctui_glyphs *g = ctui_theme_glyphs();
	int cells = width - 18;
	if (cells > 32)
		cells = 32;
	ctui_put(y, x, 4, label, text_attr);
	ctui_put(y, x + 4, 1, "[", text_attr);
	size_t filled =
	    valid ? ctui_gauge_filled(true, percent, 100.0, (size_t)cells) : 0;
	int level = ctui_attr(ctui_gauge_level_pair(percent), false);
	int empty = ctui_attr(CTUI_PAIR_GAUGE_EMPTY, false);
	for (int i = 0; i < cells; i++) {
		bool lit = (size_t)i < filled;
		ctui_put(y, x + 5 + i, 1,
			 lit ? g->gauge_filled : g->gauge_empty,
			 lit ? level : empty);
	}
	int at = x + 5 + cells;
	ctui_put(y, at, 2, "] ", text_attr);
	ctui_put(y, at + 2, width - cells - 7,
		 valid ? value : ctui_labels_resolve(NULL).not_available,
		 text_attr);
	return width;
}
