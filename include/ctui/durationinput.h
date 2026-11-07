/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * durationinput.h - a duration field "012h 30m 00s 250ms 000us" edited unit
 * by unit.
 *
 * The value is a number of microseconds, never negative. The field shows the
 * units from `largest` down to `smallest` (hours, minutes, seconds,
 * milliseconds, microseconds): the largest one takes all that the smaller
 * ones do not (90 minutes shown in minutes only is 90m, shown with hours is
 * 01h 30m), the others stay inside their range. The value is cut to the
 * smallest unit shown. It can be given an optional maximum: the value is
 * kept under it, whatever is typed or stepped, and the largest unit gets as
 * many digits as the maximum needs (three without one, so 999 of the largest
 * unit at most).
 *
 * Keys: Left / Right change the unit, Up / Down add or subtract one of the
 * unit (carrying into the larger ones: 59 minutes plus one is 01h 00m) and
 * PgUp / PgDn ten, digits are typed into the unit (a unit that cannot take
 * another digit moves on to the next one, the last stays), ':' and space
 * move to the next unit, Enter is reported as submitted and Esc as cancelled.
 *
 * Mouse: coordinates are relative to the top-left cell of the field as
 * drawn. A click on a unit selects it and the wheel steps it.
 *
 * ctui_durationinput_parse() and ctui_durationinput_format() are pure.
 */
#ifndef CTUI_DURATIONINPUT_H
#define CTUI_DURATIONINPUT_H

#include <stdbool.h>
#include <stddef.h>

#include <ctui/theme.h>

#define CTUI_DURATION_TEXT 56 /* bytes enough for any field, with its NUL */
#define CTUI_DURATION_MAX_DIGITS 9
#define CTUI_DURATION_DEFAULT_DIGITS 3

typedef enum ctui_duration_unit {
	CTUI_DURATION_HOURS = 0,
	CTUI_DURATION_MINUTES,
	CTUI_DURATION_SECONDS,
	CTUI_DURATION_MILLIS,
	CTUI_DURATION_MICROS
} ctui_duration_unit;

typedef struct ctui_durationinput {
	long long micro; /* the value, microseconds */
	int largest;	 /* ctui_duration_unit, the first one shown */
	int smallest;	 /* ctui_duration_unit, the last one shown */
	int digits;	 /* digits of the largest unit */
	bool has_max;	 /* a maximum is set */
	long long max;	 /* in microseconds */
	int segment;	 /* ctui_duration_unit under the cursor */
	int typed;	 /* digits typed into the unit so far */
	long long entry; /* the number they make */
	const ctui_style
	    *style; /* colours of this instance, may be NULL; set after init */
} ctui_durationinput;

typedef enum ctui_durationinput_event {
	CTUI_DURATIONINPUT_NONE = 0,
	CTUI_DURATIONINPUT_MOVED,     /* the unit changed */
	CTUI_DURATIONINPUT_CHANGED,   /* the duration changed */
	CTUI_DURATIONINPUT_SUBMITTED, /* Enter */
	CTUI_DURATIONINPUT_CANCELLED  /* Esc */
} ctui_durationinput_event;

/* ---- pure functions ---- */

/* Microseconds in one unit; 0 for an unknown unit. */
long long ctui_duration_unit_micros(ctui_duration_unit unit);

/* Parse "1h30m", "90m", "1h 30m 5s 250ms 10us": whole numbers followed by
 * h, m, s, ms or us, from the largest unit to the smallest, each at most
 * once, spaces allowed between the parts. False, leaving *out alone, for
 * anything else including an empty text and a duration that does not fit
 * (above about 4e18 microseconds). */
bool ctui_durationinput_parse(const char *s, long long *out);

/* Write the duration as the field shows it: the units from largest to
 * smallest, the largest zero-padded to `digits`, the others to two digits
 * (three for milliseconds and microseconds), each followed by its suffix and
 * all separated by a space, into buf (at least CTUI_DURATION_TEXT bytes).
 * NULL when the value is negative, the units are unknown or in the wrong
 * order, the digits outside 1..CTUI_DURATION_MAX_DIGITS or the buffer too
 * small. Finer parts than `smallest` are cut. */
char *ctui_durationinput_format(long long micro, ctui_duration_unit largest,
				ctui_duration_unit smallest, int digits,
				char *buf, size_t cap);

/* ---- component ---- */

/* A field showing the duration from `largest` to `smallest`; units out of
 * order or unknown give hours to seconds, and a negative value zero. */
void ctui_durationinput_init(ctui_durationinput *in, ctui_duration_unit largest,
			     ctui_duration_unit smallest, long long micro);

/* The greatest value the field takes: its maximum, else what its digits
 * hold. */
long long ctui_durationinput_capacity(const ctui_durationinput *in);

/* Set the value (cut to the smallest unit); false, value kept, when it is
 * negative or above the capacity. */
bool ctui_durationinput_set(ctui_durationinput *in, long long micro);

/* Set the maximum (NULL for none), fit the digits of the largest unit to it
 * and bring the value under it; false, nothing changed, when it is negative
 * or needs more than CTUI_DURATION_MAX_DIGITS digits. */
bool ctui_durationinput_set_max(ctui_durationinput *in, const long long *max);

/* Change the selected unit by delta (as Up / Down do); true when the value
 * changed. */
bool ctui_durationinput_step(ctui_durationinput *in, int delta);

ctui_durationinput_event ctui_durationinput_key(ctui_durationinput *in,
						int key);

/* Left press at (y, x) relative to the field. */
ctui_durationinput_event ctui_durationinput_click(ctui_durationinput *in, int y,
						  int x);

/* Wheel: positive notches step up. */
ctui_durationinput_event ctui_durationinput_wheel(ctui_durationinput *in,
						  int notches);

/* Columns of the field. */
int ctui_durationinput_width(const ctui_durationinput *in);

/* Which unit a column of the field lands on, or -1 for the margins and
 * the gaps between units. */
int ctui_durationinput_segment_at(const ctui_durationinput *in, int x);

/* Draw the field at (y, x). */
void ctui_durationinput_draw(const ctui_durationinput *in, int y, int x,
			     bool focused);

#endif /* CTUI_DURATIONINPUT_H */
