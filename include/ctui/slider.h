/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * slider.h - a standalone horizontal or vertical fader.
 *
 * The slider holds a value in [min, max] that moves in steps of `step`
 * from min (step <= 0 means a continuous value). It is drawn as a track
 * with a filled part and a knob, a label and the value as text. With
 * `centre` set the track has a mark in the middle and the fill starts
 * there (a bipolar fader such as a pan or a detune); otherwise it starts
 * at the minimum.
 *
 * Layout, set with ctui_slider_place():
 *  - horizontal, one row: "LABEL [-----O----] value"; the label (cut to a
 *    third of the width) and the value (as wide as the widest of min and
 *    max) are optional parts around the track;
 *  - vertical, `height` rows in `width` columns: the value on the first
 *    row, the track below it with the maximum at the top, and the label on
 *    the last row; the track is centred in the width.
 *
 * Keys: Left/Down decrease and Right/Up increase by one step, PageUp /
 * PageDown by ten, Home / End jump to min / max.
 *
 * Mouse: a press on the track puts the knob under the pointer and starts a
 * drag, which follows the pointer along the track (also outside it); the
 * release ends it; the wheel moves by one step per notch. The value under
 * a cell comes from pure functions, so the mapping is testable and the
 * demo needs no curses to compute it. Events carry screen coordinates.
 */
#ifndef CTUI_SLIDER_H
#define CTUI_SLIDER_H

#include <stdbool.h>
#include <stddef.h>

#include <ctui/theme.h>

typedef struct ctui_slider {
	double value, min, max, step;
	int decimals; /* digits after the point in the value text */
	bool vertical;
	bool centre;	   /* mark in the middle, fill from it */
	const char *label; /* may be NULL; must outlive the slider */
	bool dragging;
	int y, x, width, height; /* area, set by ctui_slider_place() */
	const ctui_style *style; /* colours and glyphs of this instance, may be
				    NULL; set after init */
} ctui_slider;

/* A horizontal slider at `value` (clamped and snapped). max < min are
 * swapped; decimals follow the step (at most 4). */
void ctui_slider_init(ctui_slider *s, const char *label, double min, double max,
		      double step, double value);

/* Clamp v to [min, max] and snap it to the step. */
double ctui_slider_snap(const ctui_slider *s, double v);

/* Set the value (clamped and snapped); true when it changed. */
bool ctui_slider_set(ctui_slider *s, double v);

/* Position of the value in [0, 1] (0 when max == min). */
double ctui_slider_ratio(const ctui_slider *s);

/* Cell of the knob on a track of `track` cells, counted from the left, or
 * from the top for a vertical slider (where the maximum is at the top);
 * -1 when track < 1. */
int ctui_slider_pos(const ctui_slider *s, int track);

/* Value (snapped) of a cell of a track of `track` cells, counted as above;
 * a cell outside the track counts as the nearest end. */
double ctui_slider_value_at(const ctui_slider *s, int track, int cell);

/* Text of a value as drawn, with s->decimals digits (no "-0"). Returns buf. */
char *ctui_slider_text(const ctui_slider *s, double v, char *buf, size_t cap);

/* Place the slider in a width x height area at (y, x). */
void ctui_slider_place(ctui_slider *s, int y, int x, int width, int height);

/* The track in the placed area: its first cell (y, x) and its length; false
 * when the area is too small for one (fewer than 2 cells). */
bool ctui_slider_track(const ctui_slider *s, int *y, int *x, int *length);

/* Whether the screen cell is on the track (for a vertical slider, any
 * column of the area). */
bool ctui_slider_over(const ctui_slider *s, int y, int x);

/* Apply a key. Returns true when the key is one of the slider's. */
bool ctui_slider_key(ctui_slider *s, int key);

/* Left button pressed at (y, x): true when the slider took the event; the
 * value may have changed. */
bool ctui_slider_press(ctui_slider *s, int y, int x);

/* Pointer moved with the button down: true when the value changed. */
bool ctui_slider_drag(ctui_slider *s, int y, int x);

/* Button released: true when a drag ended. */
bool ctui_slider_release(ctui_slider *s);

/* Wheel: move by `notches` steps (positive = increase); true when the value
 * changed. */
bool ctui_slider_wheel(ctui_slider *s, int notches);

/* Draw the slider in its placed area; focused highlights the knob and the
 * label. */
void ctui_slider_draw(const ctui_slider *s, bool focused);

#endif /* CTUI_SLIDER_H */
