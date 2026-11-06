/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * pads.h - a grid of trigger pads, as on a drum machine or an MPC.
 *
 * Each pad has a label and a level in [0, 1] (the velocity of the last hit)
 * that decays linearly with time: ctui_pads_tick() takes the current time in
 * seconds from the caller, so the animation needs no clock and can be
 * tested. A lit pad is coloured by its level (green, yellow, red) and a
 * dark one is drawn with the empty gauge glyph; the pad under the cursor is
 * framed by brackets around its label. The grid holds at most 8 x 8 pads
 * and is stored in the struct; labels are owned by the caller.
 *
 * Keys: the arrows move the cursor, Home / End jump to the first and the
 * last pad, Enter and space hit the pad under the cursor. With `key_map`
 * set, the keys 1234 / qwer / asdf / zxcv hit the pads of the first four
 * rows and columns, as on the pads of a controller.
 *
 * Mouse: a press hits the pad under the pointer and holds it; dragging onto
 * another pad hits that one; the release lets go. A press higher in a pad
 * is harder (velocity 1 on its first row down to 0.4 on its last).
 * Coordinates are relative to the area given to the call.
 */
#ifndef CTUI_PADS_H
#define CTUI_PADS_H

#include <stdbool.h>
#include <stddef.h>

#include <ctui/theme.h>

#define CTUI_PADS_MAX 8 /* rows and columns */

typedef struct ctui_pads {
	unsigned rows, cols;
	const char *const *labels; /* rows * cols labels in reading order; NULL
				      or a NULL entry shows the pad number */
	float level[CTUI_PADS_MAX *
		    CTUI_PADS_MAX]; /* current level of each pad, 0 = dark */
	size_t cursor;		    /* selected pad */
	int held;		    /* pad held by the mouse, -1 when none */
	float velocity; /* level of a hit from the keyboard, default 0.8 */
	float decay;	/* level lost per second, default 4 (a full hit fades in
			   0.25 s) */
	bool key_map;	/* 1234 / qwer / asdf / zxcv hit the pads */
	bool ticked;	/* last_time is valid */
	double last_time;
	const ctui_style *style; /* colours and glyphs of this instance, may be
				    NULL; set after init */
} ctui_pads;

/* A rows x cols grid, all pads dark, cursor on the first one. rows and
 * cols are clamped to 1..CTUI_PADS_MAX; 0 means 4. */
void ctui_pads_init(ctui_pads *p, unsigned rows, unsigned cols,
		    const char *const *labels);

/* Number of pads. */
size_t ctui_pads_count(const ctui_pads *p);

/* Light pad `index` with `velocity` (clamped to 0..1). False when the pad
 * does not exist. */
bool ctui_pads_hit(ctui_pads *p, size_t index, float velocity);

/* Let the levels decay up to `seconds` (a monotonic time chosen by the
 * caller; the first call only records it, and a time going backwards is
 * ignored). Returns true while some pad is still lit. */
bool ctui_pads_tick(ctui_pads *p, double seconds);

/* Whether any pad is lit. */
bool ctui_pads_active(const ctui_pads *p);

/* Move the cursor by whole pads, clamped to the grid. */
void ctui_pads_move(ctui_pads *p, int drow, int dcol);

/* Pad hit by a key of the 4x4 mapping, or -1 (when the grid is smaller
 * than the key's place, or the key is not mapped). Independent of key_map. */
int ctui_pads_key_pad(const ctui_pads *p, int key);

/* Apply a key. Returns true when handled; *hit (may be NULL) receives the
 * pad that was hit, or -1. */
bool ctui_pads_key(ctui_pads *p, int key, int *hit);

/* Cell of pad `index` in a width x height area: top-left corner relative
 * to the area and size. Pads are separated by one column, and by one row
 * when the area is high enough. False when the pads would be empty. */
bool ctui_pads_cell(const ctui_pads *p, int width, int height, size_t index,
		    int *y, int *x, int *w, int *h);

/* Pad under (y, x) of the area, or -1. */
int ctui_pads_at(const ctui_pads *p, int width, int height, int y, int x);

/* Level of a hit at row `row` of a pad `height` rows high. */
float ctui_pads_velocity(int row, int height);

/* Colour pair of a level: GAUGE_EMPTY when dark, then green, yellow, red. */
ctui_pair ctui_pads_level_pair(float level);

/* Mouse press: hits the pad at (y, x), puts the cursor on it and holds it.
 * Returns the pad, or -1 (nothing happened). */
int ctui_pads_press(ctui_pads *p, int width, int height, int y, int x);

/* Pointer moved with the button down: hits the pad it enters. Returns that
 * pad, or -1 when nothing was hit (including when no press is held). */
int ctui_pads_drag(ctui_pads *p, int width, int height, int y, int x);

/* Button released; true when a pad was held. */
bool ctui_pads_release(ctui_pads *p);

/* Draw the grid in a width x height area at (y, x). */
void ctui_pads_draw(const ctui_pads *p, int y, int x, int width, int height);

#endif /* CTUI_PADS_H */
