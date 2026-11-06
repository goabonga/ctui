/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * waveform.h - a zoomable waveform view with cursor, selection and playhead.
 *
 * The view looks at interleaved float samples in [-1, 1] (owned by the
 * caller). The visible window is [offset, offset + span) frames, spread
 * over the columns of the view; each column draws the vertical extent
 * between the lowest and the highest sample it covers, over all channels.
 *
 * Keys: Left/Right move the cursor by one column of the current zoom,
 * Home/End jump to the ends, + and - zoom in and out around the cursor,
 * [ and ] set the start and the end of the selection at the cursor,
 * r resets zoom, cursor and selection.
 *
 * Mouse: a press on a column puts the cursor there and starts a selection;
 * dragging stretches the selection from the pressed frame to the one under
 * the pointer (also leftwards), and the release ends it. A press without a
 * drag only moves the cursor. Columns are relative to the view.
 */
#ifndef CTUI_WAVEFORM_H
#define CTUI_WAVEFORM_H

#include <stdbool.h>

#include <ctui/theme.h>
#include <stddef.h>

typedef struct ctui_waveform {
	const float *samples;
	size_t frames;
	unsigned channels;
	size_t offset, span; /* visible window, span >= 1 */
	size_t cursor;
	bool selection;
	size_t first, end; /* selection [first, end) when selection is set */
	bool dragging;	   /* a mouse selection is in progress */
	size_t anchor;	   /* frame where the press happened */
	bool filled; /* solid bars from the middle line, in half rows, instead
			of thin lines (UTF-8 only); the info tone up to the
			playhead, the idle tone after it */
	bool show_playhead;
	size_t playhead;
	const ctui_style *style; /* colours and glyphs of this instance, may be
				    NULL; set after init */
} ctui_waveform;

/* Show the whole signal; frames and channels must be at least 1. */
void ctui_waveform_init(ctui_waveform *w, const float *samples, size_t frames,
			unsigned channels);

/* Frames covered by column x of cols: [*a, *b), at least one frame. */
void ctui_waveform_column(const ctui_waveform *w, int x, int cols, size_t *a,
			  size_t *b);

/* Lowest and highest sample over frames [a, b) and all channels (both 0
 * for an empty range). */
void ctui_waveform_peak(const ctui_waveform *w, size_t a, size_t b, float *low,
			float *high);

/* Column of a frame, or -1 when it is outside the window. */
int ctui_waveform_x(const ctui_waveform *w, size_t frame, int cols);

/* Apply a key; cols is the width of the view. Returns true when handled. */
bool ctui_waveform_key(ctui_waveform *w, int key, int cols);

/* Mouse selection of a range; x is a column of the view, cols its width.
 * ctui_waveform_drag() and ctui_waveform_release() return true when a press had
 * started a drag. A column outside the view counts as the nearest one. */
void ctui_waveform_press(ctui_waveform *w, int x, int cols);
bool ctui_waveform_drag(ctui_waveform *w, int x, int cols);
bool ctui_waveform_release(ctui_waveform *w);

/* Show the whole signal again: no selection, no zoom, cursor at the start
 * (what the r key does; a right click can call it too). */
void ctui_waveform_reset(ctui_waveform *w);

/* Draw the waveform in a width x height area. */
void ctui_waveform_draw(const ctui_waveform *w, int y, int x, int width,
			int height);

#endif /* CTUI_WAVEFORM_H */
