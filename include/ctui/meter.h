/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * meter.h - an audio level meter (VU / peak), vertical or horizontal.
 *
 * Levels are in dBFS (0 = full scale) and are mapped on a scale from
 * CTUI_METER_FLOOR_DB (-60) to 0 dBFS, spread linearly over the cells of the
 * meter; ctui_meter_db() and ctui_meter_linear() convert from and to linear
 * amplitudes (1.0 = 0 dBFS). The cells are coloured by their place on the
 * scale: green up to CTUI_METER_YELLOW_DB, yellow up to CTUI_METER_RED_DB, red
 * above. Cells above the level are dimmed so the zones show on a silent
 * meter.
 *
 * The meter keeps a peak-hold marker: it follows the level up at once, stays
 * for `hold` seconds, then falls by `decay` dB per second until it meets the
 * level. Time is an argument (ctui_meter_set_db() takes the seconds elapsed
 * since the previous call), so nothing reads a clock. The last cell of the
 * length (the top one when vertical, the right one when horizontal) is the
 * clip indicator: it lights when a level reaches 0 dBFS and stays lit until
 * ctui_meter_clear().
 *
 * The meter does not own the signal: the caller feeds levels with
 * ctui_meter_set_db() or ctui_meter_set_linear(), places the meter with
 * ctui_meter_place() (or two of them with ctui_meter_place_pair() for a stereo
 * pair) and draws it.
 *
 * Keys: Up / Right and Down / Left move the level by 1 dB, PgUp / PgDn by
 * 6 dB, Home sets the floor, End sets 0 dBFS, c clears the peak and the clip
 * indicator.
 *
 * Mouse: pressing or dragging on the meter sets the level under the pointer
 * (also past its ends, which count as the nearest cell); a press on the clip
 * indicator clears it; the wheel moves the level by 1 dB per row. Events
 * carry screen coordinates.
 */
#ifndef CTUI_METER_H
#define CTUI_METER_H

#include <stdbool.h>

#include <ctui/theme.h>

#define CTUI_METER_FLOOR_DB (-60.0) /* bottom of the scale */
#define CTUI_METER_CLIP_DB                                                     \
	0.0 /* level from which the clip indicator lights                      \
	     */
#define CTUI_METER_YELLOW_DB (-18.0) /* first yellow level */
#define CTUI_METER_RED_DB (-6.0)     /* first red level */
#define CTUI_METER_MAX_DB 12.0	     /* levels above are brought back to this */

#define CTUI_METER_HOLD 1.5 /* default hold time of the peak, seconds */
#define CTUI_METER_DECAY                                                       \
	20.0 /* default fall of the peak after the hold, dB / s */

typedef enum ctui_meter_zone {
	CTUI_METER_GREEN,
	CTUI_METER_YELLOW,
	CTUI_METER_RED
} ctui_meter_zone;

/* Peak-hold state: the held level and the seconds since it was set. */
typedef struct ctui_meter_peak {
	double db;
	double held;
} ctui_meter_peak;

typedef struct ctui_meter {
	bool vertical;	      /* rising bar, instead of a horizontal one */
	double level;	      /* current level, dBFS, CTUI_METER_FLOOR_DB ..
				 CTUI_METER_MAX_DB */
	ctui_meter_peak peak; /* held peak */
	double hold,
	    decay;	/* seconds before the peak falls, then dB per second */
	bool show_peak; /* draw the peak marker */
	bool clip; /* a level reached CTUI_METER_CLIP_DB since the last clear */
	int y, x, width, height; /* the area, set by ctui_meter_place() */
	bool dragging;
	const ctui_style *style; /* colours and glyphs of this instance, may be
				    NULL; set after init */
} ctui_meter;

/* Linear amplitude to dBFS: 20 log10 |linear|. Zero (and NaN) give -inf. */
double ctui_meter_db(double linear);

/* dBFS to linear amplitude: 10 ^ (db / 20). */
double ctui_meter_linear(double db);

/* Position of a level on the scale: 0 at the floor (and below, or NaN) up
 * to 1 at 0 dBFS (and above). */
double ctui_meter_fraction(double db);

/* Cells of a scale of `length` cells lit by a level (0..length), rounded to
 * the nearest cell. */
int ctui_meter_cells(double db, int length);

/* Cell (0..length - 1, 0 = the floor end) holding a level, for the peak
 * marker; -1 when length < 1. */
int ctui_meter_cell(double db, int length);

/* Level at the middle of a cell of a scale of `length` cells; the inverse of
 * ctui_meter_cell() up to the cell size. Cells outside 0..length - 1 are
 * brought back to the nearest one. */
double ctui_meter_cell_db(int cell, int length);

/* Colour zone of a level. */
ctui_meter_zone ctui_meter_zone_of(double db);

/* Pair of a zone; bright selects the lit variant. */
ctui_pair ctui_meter_zone_pair(ctui_meter_zone zone, bool bright);

/* Cells of the scale in a meter `length` cells long: the clip indicator
 * takes the last one (0 when length < 2). */
int ctui_meter_scale_length(int length);

/* Peak-hold update after dt seconds at `level`: a level at or above the peak
 * becomes the peak and restarts the hold; otherwise the peak waits until
 * `hold` seconds have passed, then falls by `decay` dB per second, never
 * below the level. A negative dt counts as 0. */
void ctui_meter_peak_update(ctui_meter_peak *peak, double level, double dt,
			    double hold, double decay);

/* A silent meter with the default hold and decay; vertical or horizontal. */
void ctui_meter_init(ctui_meter *m, bool vertical);

/* Feed a level in dBFS (NaN and levels below the floor count as the floor)
 * and advance the peak by dt seconds; latches the clip indicator. */
void ctui_meter_set_db(ctui_meter *m, double db, double dt);

/* Same with a linear amplitude. */
void ctui_meter_set_linear(ctui_meter *m, double linear, double dt);

/* Clear the peak (it falls to the level) and the clip indicator. */
void ctui_meter_clear(ctui_meter *m);

/* Place the meter in a width x height area at (y, x): it runs along the
 * height when vertical, the width otherwise, and fills the other side. */
void ctui_meter_place(ctui_meter *m, int y, int x, int width, int height);

/* Place a stereo pair in the area: left and right, side by side when
 * vertical (top and bottom when horizontal), with one empty line between
 * them when the area is at least 3 thick. Both get the orientation of
 * `left`. */
void ctui_meter_place_pair(ctui_meter *left, ctui_meter *right, int y, int x,
			   int width, int height);

/* Apply a key; returns true when handled. */
bool ctui_meter_key(ctui_meter *m, int key);

/* Whether the screen cell is in the meter. */
bool ctui_meter_over(const ctui_meter *m, int y, int x);

/* Left button pressed at (y, x). Returns true when the meter took the event;
 * the level may have changed. */
bool ctui_meter_press(ctui_meter *m, int y, int x);

/* Pointer moved with the button down; true when a drag is going on. */
bool ctui_meter_drag(ctui_meter *m, int y, int x);

/* Button released; true when a drag ended. */
bool ctui_meter_release(ctui_meter *m);

/* Wheel over the meter: move the level by `rows` dB (positive = up); true
 * when the pointer is on the meter. */
bool ctui_meter_wheel(ctui_meter *m, int y, int x, int rows);

/* Draw the meter in its area. */
void ctui_meter_draw(const ctui_meter *m);

#endif /* CTUI_METER_H */
