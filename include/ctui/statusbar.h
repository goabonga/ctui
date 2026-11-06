/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * statusbar.h - the powerline status bar.
 *
 * One row on the bar background made of two runs of segments: a left run
 * growing from the left edge and a right run anchored to the right edge.
 * Each segment is " text " on the background of its tone. Between two
 * segments of different tones a hard divider blends one into the other;
 * between two segments of the same tone a soft divider is drawn. The left
 * run ends with a divider into the bar, the right run starts with one out
 * of the bar. The right run is dropped when it would overlap the left one.
 */
#ifndef CTUI_STATUSBAR_H
#define CTUI_STATUSBAR_H

#include <stdbool.h>
#include <stddef.h>

#include <ctui/theme.h>

typedef struct ctui_segment {
	const char *text;
	ctui_tone tone;
	bool bold;
} ctui_segment;

/* Columns taken by a run of segments, dividers included. */
int ctui_statusbar_width(const ctui_segment *segments, size_t count);

/* Draw the bar on row y across the whole screen width. */
void ctui_statusbar_draw(int y, const ctui_segment *left, size_t left_count,
			 const ctui_segment *right, size_t right_count);

#endif /* CTUI_STATUSBAR_H */
