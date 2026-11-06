/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * gauge.h - horizontal bars drawn with block characters.
 *
 * bar(value, maximum, width): an absent value or maximum <= 0 draws an
 * empty gauge; otherwise ratio = clamp(value / maximum, 0, 1) and
 * filled = (int)(ratio * width) cells of the filled glyph followed by
 * width - filled empty glyphs.
 *
 * Two looks are offered:
 *  - the zoned bar: every cell is coloured by its position on the scale
 *    (0..20 green, 20..40 bright green, 40..60 yellow, 60..80 bright
 *    yellow, 80..90 red, 90..100 bright red), lit cells at full intensity
 *    and unlit cells dimmed, so the scale is readable on an empty gauge;
 *  - the labelled gauge "CPU [████░░░░] 12.0%", whose filled cells take
 *    the colour of the value: green below 70 %, yellow from 70 %, red from
 *    90 %.
 */
#ifndef CTUI_GAUGE_H
#define CTUI_GAUGE_H

#include <stdbool.h>
#include <stddef.h>

#include <ctui/theme.h>

/* Scale zone of a cell: 0..5 for 0-20, 20-40, 40-60, 60-80, 80-90, 90-100 %. */
unsigned ctui_gauge_zone(size_t cell, size_t width);

/* Pair and weight of a zone (odd zones are the bright, bold ones). */
ctui_pair ctui_gauge_zone_pair(unsigned zone);

/* Number of filled cells (0..width) for value/maximum. */
size_t ctui_gauge_filled(bool present, double value, double maximum,
			 size_t width);

/* Write the bar into buf (NUL-terminated; cut on a glyph boundary if cap
 * is too small). filled/empty NULL = the current glyph set. Returns buf. */
char *ctui_gauge_bar(char *buf, size_t cap, bool present, double value,
		     double maximum, size_t width, const char *filled,
		     const char *empty);

/* Colour of a percentage for the labelled gauge. */
ctui_pair ctui_gauge_level_pair(double percent);

/* Draw the zoned bar at (y, x); returns the columns used. */
int ctui_gauge_draw(int y, int x, int width, bool present, double value,
		    double maximum);

/* Draw "LABEL [bar] value" in width columns (at least 20 are needed).
 * label is cut to 4 columns; an invalid reading shows "N/A". text_attr is
 * the attribute of the label and the value text. Returns the columns used. */
int ctui_gauge_labeled(int y, int x, int width, const char *label,
		       double percent, bool valid, const char *value,
		       int text_attr);

#endif /* CTUI_GAUGE_H */
