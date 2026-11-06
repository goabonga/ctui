/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * spinner.h - activity indicator with an optional label.
 *
 * A spinner is one glyph that changes with time, followed by a label. The
 * frame is a pure function of the time the caller passes in (any monotonic
 * clock, in seconds), so nothing here reads a clock and frames are
 * testable. Without UTF-8 (or with CTUI_ASCII) the ASCII frame sets are
 * used.
 */
#ifndef CTUI_SPINNER_H
#define CTUI_SPINNER_H

#include <stdbool.h>
#include <stddef.h>

#include <ctui/theme.h>

#define CTUI_SPINNER_FPS                                                       \
	10.0 /* frames per second unless `fps` says otherwise */

typedef enum ctui_spinner_kind {
	CTUI_SPINNER_BRAILLE = 0, /* ten rotating braille dot patterns */
	CTUI_SPINNER_LINE,	  /* a turning bar */
	CTUI_SPINNER_DOTS /* braille dots running round the cell (ASCII: growing
			     dots) */
} ctui_spinner_kind;

typedef struct ctui_spinner {
	ctui_spinner_kind kind;
	const char *label;	 /* drawn after the glyph, may be NULL */
	double fps;		 /* frames per second: 0 = CTUI_SPINNER_FPS */
	const ctui_style *style; /* colours and glyphs of this instance, may be
				    NULL; set after init */
} ctui_spinner;

/* Frames of a set; 0 for an unknown kind. */
size_t ctui_spinner_frame_count(ctui_spinner_kind kind, bool unicode);

/* Frame shown at the given time: floor(seconds * fps) modulo the frame
 * count. A negative time counts as 0 and a rate of 0 or less as
 * CTUI_SPINNER_FPS. */
size_t ctui_spinner_index(ctui_spinner_kind kind, bool unicode, double seconds,
			  double fps);

/* The glyph of frame i (wrapped to the set), or "" for an unknown kind. */
const char *ctui_spinner_frame(ctui_spinner_kind kind, bool unicode, size_t i);

/* Columns the spinner takes: its widest frame, then a space and the label. */
int ctui_spinner_width(const ctui_spinner *s);

/* Draw at (y, x) in at most maxcols columns; returns the columns used. */
int ctui_spinner_draw(const ctui_spinner *s, int y, int x, int maxcols,
		      double seconds);

#endif /* CTUI_SPINNER_H */
