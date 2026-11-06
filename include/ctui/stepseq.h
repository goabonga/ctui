/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * stepseq.h - a step sequencer grid in the style of the electribe, volca
 * and TR drum machines: one row per track, one cell per step.
 *
 * The pattern is owned by the caller (a sequencer engine) and the widget
 * works on it in place:
 *  - pattern[track] is one word per track, bit n set = step n is on, so a
 *    track holds up to 32 steps (16 or 32 in practice);
 *  - velocity, when not NULL, holds tracks * steps bytes (0..127, indexed
 *    track * steps + step) and gives each step its level, drawn as the
 *    height of the cell glyph; a step that is turned on with a velocity of
 *    0 gets `fresh`. When NULL every step is at full level.
 *
 * Steps are grouped by beat: a gap is drawn every `group` steps (4 by
 * default) and the numbers above the grid mark the first step of a group.
 * The playhead column is highlighted while show_playhead is set, and the
 * cursor cell is drawn as selected unless hide_cursor is set. Bits above
 * `steps` are ignored.
 *
 * The grid is drawn from the top-left corner of its area: a header row
 * with the step numbers, then one row per track that fits the height
 * (label, then the cells). There is no scrolling.
 *
 * Keys: arrows move the cursor, Home / End jump to the first / last step
 * of the track, Space or Enter toggle the step, Delete turns it off, + (or =)
 * and - change its velocity, [ and ] rotate the track by one step, c
 * clears the track, C clears the whole pattern.
 *
 * Mouse: a press on a cell moves the cursor there and toggles it; dragging
 * across other cells paints them with the state the press gave. The wheel
 * over a cell changes its velocity. Positions are relative to the area
 * given to ctui_stepseq_draw().
 *
 * The functions on single words (ctui_stepseq_bit_*) and the playhead helpers
 * are pure, so an engine can use them on its own copy of the data.
 */
#ifndef CTUI_STEPSEQ_H
#define CTUI_STEPSEQ_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <ctui/theme.h>

#define CTUI_STEPSEQ_MAX_STEPS 32
#define CTUI_STEPSEQ_LABEL_MAX 8 /* columns of a track label */
#define CTUI_STEPSEQ_VELOCITY_MAX 127
#define CTUI_STEPSEQ_VELOCITY_STEP                                             \
	8 /* change made by + / - and one wheel notch */

typedef struct ctui_stepseq {
	uint32_t *pattern; /* one word per track */
	uint8_t *velocity; /* tracks * steps bytes, or NULL */
	const char *const
	    *labels;		/* one label per track, or NULL (T1, T2...) */
	unsigned tracks, steps; /* steps: 1..CTUI_STEPSEQ_MAX_STEPS */
	unsigned group;		/* steps per beat group, 0 = no grouping */
	unsigned fresh; /* velocity given to a step turned on without one */
	unsigned track, step; /* cursor */
	bool show_playhead;
	unsigned playhead; /* highlighted step */
	bool hide_cursor;  /* do not draw the cursor (a grid without focus) */
	bool dragging;	   /* a mouse paint is in progress */
	bool paint;	   /* state painted by the drag */
	const ctui_style *style; /* colours and glyphs of this instance, may be
				    NULL; set after init */
} ctui_stepseq;

/* ---- pure helpers on one track word ---- */

/* Word with the first `steps` bits set (steps is capped at 32). */
uint32_t ctui_stepseq_bit_mask(unsigned steps);

/* Whether the step is on (false at or past bit 32). */
bool ctui_stepseq_bit_test(uint32_t row, unsigned step);

/* The word with the step turned on or off; unchanged past bit 32. */
uint32_t ctui_stepseq_bit_set(uint32_t row, unsigned step, bool on);
uint32_t ctui_stepseq_bit_toggle(uint32_t row, unsigned step);

/* Steps that are on among the first `steps`. */
unsigned ctui_stepseq_bit_count(uint32_t row, unsigned steps);

/* Rotate the first `steps` bits towards higher steps by `by` (negative =
 * towards lower ones), wrapping around; bits above `steps` are dropped. */
uint32_t ctui_stepseq_bit_rotate(uint32_t row, unsigned steps, int by);

/* A position wrapped into 0 .. steps - 1 (0 when steps is 0). */
unsigned ctui_stepseq_wrap(long pos, unsigned steps);

/* The step after pos, wrapping at the end of the pattern. */
unsigned ctui_stepseq_next(unsigned pos, unsigned steps);

/* Whether the step starts a beat group (group 0 = only step 0 does). */
bool ctui_stepseq_group_start(unsigned step, unsigned group);

/* Level of a velocity for the cell glyph: 0..3 (1-31, 32-63, 64-95, 96+). */
unsigned ctui_stepseq_level(unsigned velocity);

/* ---- the widget ---- */

/* Work on `tracks` words of `steps` steps; the cursor is at the first
 * cell, group is 4 and fresh is 100. The words are used as they are. */
void ctui_stepseq_init(ctui_stepseq *s, uint32_t *pattern, unsigned tracks,
		       unsigned steps);

/* Whether a step is on; false outside the pattern. */
bool ctui_stepseq_on(const ctui_stepseq *s, unsigned track, unsigned step);

/* Velocity of a step: 0 when off, else 1..127 (127 without a velocity
 * array). */
unsigned ctui_stepseq_velocity(const ctui_stepseq *s, unsigned track,
			       unsigned step);

/* Turn a step on or off; true when it changed. */
bool ctui_stepseq_put(ctui_stepseq *s, unsigned track, unsigned step, bool on);

/* Toggle a step; returns its new state. */
bool ctui_stepseq_flip(ctui_stepseq *s, unsigned track, unsigned step);

/* Add delta to the velocity of a step that is on (clamped to 1..127);
 * true when it changed. False without a velocity array. */
bool ctui_stepseq_adjust(ctui_stepseq *s, unsigned track, unsigned step,
			 int delta);

/* Turn off every step of a track, or of the whole pattern. */
void ctui_stepseq_clear_track(ctui_stepseq *s, unsigned track);
void ctui_stepseq_clear(ctui_stepseq *s);

/* Rotate a track (and its velocities) by `by` steps, see
 * ctui_stepseq_bit_rotate(). */
void ctui_stepseq_rotate(ctui_stepseq *s, unsigned track, int by);

/* Apply a key; true when it was used. */
bool ctui_stepseq_key(ctui_stepseq *s, int key);

/* Geometry, relative to the area: columns of the labels, of the whole
 * grid, rows of the grid, and the column of the first cell of a step
 * (each cell is 2 columns wide). */
int ctui_stepseq_label_width(const ctui_stepseq *s);
int ctui_stepseq_width(const ctui_stepseq *s);
int ctui_stepseq_height(const ctui_stepseq *s);
int ctui_stepseq_x(const ctui_stepseq *s, unsigned step);

/* The cell under (y, x); false on the labels, the header, a gap or
 * outside the grid. */
bool ctui_stepseq_hit(const ctui_stepseq *s, int y, int x, unsigned *track,
		      unsigned *step);

/* Left button pressed: moves the cursor to the cell and toggles it, and
 * starts a drag. True when it hit a cell. */
bool ctui_stepseq_press(ctui_stepseq *s, int y, int x);

/* Pointer moved with the button down: paints the cell under it with the
 * state of the press; true when a drag is in progress. */
bool ctui_stepseq_drag(ctui_stepseq *s, int y, int x);

/* Button released; true when a drag ended. */
bool ctui_stepseq_release(ctui_stepseq *s);

/* Wheel over a cell: notches (positive = up) change its velocity; true
 * when it changed. */
bool ctui_stepseq_wheel(ctui_stepseq *s, int y, int x, int notches);

/* Draw the grid with its top-left corner at (y, x), clipped to width x height.
 */
void ctui_stepseq_draw(const ctui_stepseq *s, int y, int x, int width,
		       int height);

#endif /* CTUI_STEPSEQ_H */
