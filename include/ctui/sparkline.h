/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * sparkline.h - a chart of a series, one column of blocks per value, on one
 * row or several.
 *
 * Each value is drawn as a block whose height follows its place between the
 * lowest and the highest value (or a range given by the caller): the eight
 * vertical blocks in UTF-8 locales, ASCII characters otherwise. A NaN
 * leaves a gap. When there are more values than columns the latest ones
 * are shown.
 *
 * On several rows, a value is a bar rising from the bottom row, in eighths
 * of a row: rows full below its top, the block of its last eighth on top,
 * nothing above. Cells above a bar are left as they are. On one row both
 * functions draw the same blocks.
 */
#ifndef CTUI_SPARKLINE_H
#define CTUI_SPARKLINE_H

#include <stddef.h>

#include <ctui/theme.h>

#define CTUI_SPARKLINE_LEVELS 8

/* Block level 0..levels-1 of v between lo and hi (clamped); 0 when the
 * range is empty. */
int ctui_sparkline_level(double v, double lo, double hi, int levels);

/* Draw values at (y, x) in at most width columns, with the lowest and
 * highest of the shown values as range, or [lo, hi] when lo < hi. Returns
 * the number of columns drawn. */
int ctui_sparkline_draw(int y, int x, int width, const double *values,
			size_t count, double lo, double hi,
			ctui_icon_color color);

/* Draw values as bars height rows tall, the bottom row at y + height - 1,
 * in at most width columns; the range is as for ctui_sparkline_draw(). The
 * lowest value of the range fills one eighth of the bottom row, the highest
 * every row. Returns the number of columns drawn. */
int ctui_sparkline_draw_rows(int y, int x, int width, int height,
			     const double *values, size_t count, double lo,
			     double hi, ctui_icon_color color);

#endif /* CTUI_SPARKLINE_H */
