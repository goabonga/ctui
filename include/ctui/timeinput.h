/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * timeinput.h - a time of day field "HH:MM", "HH:MM:SS", "HH:MM:SS.fff" or
 * "HH:MM:SS.ffffff" edited segment by segment.
 *
 * The value is a number of microseconds since midnight, 0 to 86399999999,
 * cut to the precision of the field (a field in seconds keeps whole
 * seconds). It can be given an optional minimum and maximum: the value is
 * kept inside them, whatever is typed or stepped.
 *
 * Keys: Left / Right change the segment (hour, minute, second, fraction),
 * Up / Down step the segment by one and PgUp / PgDn by ten (every segment
 * wraps inside its range without carrying into the next one: 59 minutes
 * plus one is 00), digits are typed into the segment (a segment that cannot
 * take another digit moves on to the next one; the digits of the fraction
 * fill it from the left, so ".5" is 500 milliseconds), ':', '.' and space
 * move to the next segment, Enter is reported as submitted and Esc as
 * cancelled.
 *
 * Mouse: coordinates are relative to the top-left cell of the field as
 * drawn. A click on a segment selects it and the wheel steps it.
 *
 * The parse / format / validate functions are pure and need no screen.
 */
#ifndef CTUI_TIMEINPUT_H
#define CTUI_TIMEINPUT_H

#include <stdbool.h>
#include <stddef.h>

#include <ctui/theme.h>

#define CTUI_TIME_MICROS_PER_DAY 86400000000LL
#define CTUI_TIMEINPUT_TEXT 16 /* bytes of "HH:MM:SS.ffffff" and its NUL */

typedef enum ctui_time_precision {
	CTUI_TIME_MINUTES = 0, /* HH:MM */
	CTUI_TIME_SECONDS,     /* HH:MM:SS */
	CTUI_TIME_MILLIS,      /* HH:MM:SS.fff */
	CTUI_TIME_MICROS       /* HH:MM:SS.ffffff */
} ctui_time_precision;

typedef enum ctui_timeinput_segment {
	CTUI_TIMEINPUT_HOUR = 0,
	CTUI_TIMEINPUT_MINUTE,
	CTUI_TIMEINPUT_SECOND,
	CTUI_TIMEINPUT_FRACTION
} ctui_timeinput_segment;

typedef struct ctui_timeinput {
	long long micro; /* the value, microseconds since midnight */
	int precision;	 /* ctui_time_precision */
	bool has_min, has_max;
	long long min, max; /* the limits when set, in microseconds */
	int segment;	    /* ctui_timeinput_segment under the cursor */
	int typed;	    /* digits typed into the segment so far */
	long entry;	    /* the number they make */
	const ctui_style
	    *style; /* colours of this instance, may be NULL; set after init */
} ctui_timeinput;

typedef enum ctui_timeinput_event {
	CTUI_TIMEINPUT_NONE = 0,
	CTUI_TIMEINPUT_MOVED,	  /* the segment changed */
	CTUI_TIMEINPUT_CHANGED,	  /* the time changed */
	CTUI_TIMEINPUT_SUBMITTED, /* Enter */
	CTUI_TIMEINPUT_CANCELLED  /* Esc */
} ctui_timeinput_event;

/* ---- pure functions ---- */

/* Whether micro is a time of day. */
bool ctui_timeinput_valid(long long micro);

/* Parse "HH:MM", "HH:MM:SS" or "HH:MM:SS.f" with one to six fraction digits
 * (two digits for the hour, the minute and the second, no spaces or signs)
 * into *out. False, leaving *out alone, for anything else including an hour
 * above 23 or a minute or second above 59. */
bool ctui_timeinput_parse(const char *s, long long *out);

/* Write the time at the given precision into buf (at least
 * CTUI_TIMEINPUT_TEXT bytes) and return it; NULL when the time is invalid,
 * the precision unknown or the buffer too small. Finer digits are cut, not
 * rounded. */
char *ctui_timeinput_format(long long micro, ctui_time_precision precision,
			    char *buf, size_t cap);

/* Columns of a field of that precision. */
int ctui_timeinput_width(ctui_time_precision precision);

/* ---- component ---- */

/* A field showing the time cut to the precision; an invalid time gives
 * midnight and an unknown precision seconds. The hour is selected. */
void ctui_timeinput_init(ctui_timeinput *in, ctui_time_precision precision,
			 long long micro);

/* Set the value (cut to the precision); false, value kept, when it is not a
 * time of day or lies outside the limits. */
bool ctui_timeinput_set(ctui_timeinput *in, long long micro);

/* Set the limits (NULL for none) and bring the value inside them; false,
 * nothing changed, when the minimum is above the maximum. */
bool ctui_timeinput_set_limits(ctui_timeinput *in, const long long *min,
			       const long long *max);

/* Change the selected segment by delta steps (as Up / Down do); true when
 * the time changed. */
bool ctui_timeinput_step(ctui_timeinput *in, int delta);

ctui_timeinput_event ctui_timeinput_key(ctui_timeinput *in, int key);

/* Left press at (y, x) relative to the field. */
ctui_timeinput_event ctui_timeinput_click(ctui_timeinput *in, int y, int x);

/* Wheel: positive notches step up. */
ctui_timeinput_event ctui_timeinput_wheel(ctui_timeinput *in, int notches);

/* Which segment a column of a field of that precision lands on, or -1 for
 * the margins, the separators and the segments the precision hides. */
int ctui_timeinput_segment_at(ctui_time_precision precision, int x);

/* Draw the field at (y, x). */
void ctui_timeinput_draw(const ctui_timeinput *in, int y, int x, bool focused);

#endif /* CTUI_TIMEINPUT_H */
