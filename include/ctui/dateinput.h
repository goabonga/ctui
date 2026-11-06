/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * dateinput.h - a date field "YYYY-MM-DD" edited segment by segment, with a
 * month picker that drops down from it.
 *
 * The value is always a real calendar date (proleptic Gregorian, years 1 to
 * 9999): editing can never leave a 31st of February, the day is clamped when
 * the month or the year changes. It converts to and from the day count of
 * calendar.h (days since 1970-01-01), so ctui_date_* helpers apply directly.
 *
 * Keys (closed): Left / Right change the segment (year, month, day), Up /
 * Down step the segment by one (months and days wrap inside their range,
 * the year stops at 1 and 9999), digits are typed into the segment (a
 * segment that cannot take another digit moves on to the next one), '-',
 * '/' and '.' move to the next segment, Enter opens the picker and Esc is
 * reported as cancelled.
 * Keys (open): the keys of ctui_calendar (arrows, PgUp / PgDn, Home for
 * today), Enter picks the date and closes the picker, Esc closes it
 * without changing the date.
 *
 * The open picker hangs under the field in a filled frame, like the dropdown
 * of a select, with the weekdays given by one letter each (M T W T F S S, or
 * the seven names, Monday first, of `weekday_names`, cut to two columns:
 * give letters to keep one).
 *
 * Mouse: coordinates are relative to the top-left cell of the field as
 * drawn. A click on a segment selects it, a click on the arrow toggles the
 * picker, and a click on a day of the open picker picks it; a click on its
 * frame does nothing and any other click closes it. The wheel steps the
 * selected segment, or the shown month while the picker is open.
 *
 * The parse / format / validate functions are pure and need no screen.
 */
#ifndef CTUI_DATEINPUT_H
#define CTUI_DATEINPUT_H

#include <stdbool.h>
#include <stddef.h>

#include <ctui/calendar.h>
#include <ctui/theme.h>

#define CTUI_DATEINPUT_WIDTH 14 /* columns of the field */
#define CTUI_DATEINPUT_TEXT 11	/* bytes of "YYYY-MM-DD" and its NUL */

typedef enum ctui_dateinput_segment {
	CTUI_DATEINPUT_YEAR = 0,
	CTUI_DATEINPUT_MONTH,
	CTUI_DATEINPUT_DAY
} ctui_dateinput_segment;

typedef struct ctui_dateinput {
	int year, month, day; /* the value, always a valid date */
	int segment;	      /* ctui_dateinput_segment under the cursor */
	int typed;	      /* digits typed into the segment so far */
	int entry;	      /* the number they make */
	bool open;	      /* the month picker is shown */
	ctui_calendar picker; /* its calendar, valid while open */
	ctui_date today;      /* underlined in the picker, year 0 for none */
	const char *const *weekday_names; /* 7 names or NULL, see above */
	const ctui_style
	    *style; /* colours of this instance, may be NULL; set after init */
} ctui_dateinput;

typedef enum ctui_dateinput_event {
	CTUI_DATEINPUT_NONE = 0,
	CTUI_DATEINPUT_MOVED,	 /* the segment or the picker cursor moved */
	CTUI_DATEINPUT_CHANGED,	 /* the date changed */
	CTUI_DATEINPUT_OPENED,	 /* the picker opened */
	CTUI_DATEINPUT_CLOSED,	 /* the picker closed */
	CTUI_DATEINPUT_CANCELLED /* Esc with the picker closed */
} ctui_dateinput_event;

/* ---- pure functions ---- */

/* Whether year-month-day is a real date with a year in 1..9999. */
bool ctui_dateinput_valid(int year, int month, int day);

/* Parse exactly "YYYY-MM-DD" (four digits, two, two; no spaces or signs)
 * into *out. False, leaving *out alone, for anything else including dates
 * that do not exist ("2023-02-29", year 0000). */
bool ctui_dateinput_parse(const char *s, ctui_date *out);

/* Write "YYYY-MM-DD" into buf (at least CTUI_DATEINPUT_TEXT bytes) and return
 * it; NULL when the date is invalid or the buffer too small. */
char *ctui_dateinput_format(ctui_date date, char *buf, size_t cap);

/* Days since 1970-01-01 of the first and last supported date. */
long ctui_dateinput_min_days(void);
long ctui_dateinput_max_days(void);

/* ---- component ---- */

/* A field showing the date; an invalid one gives 1970-01-01. The year
 * segment is selected. */
void ctui_dateinput_init(ctui_dateinput *in, int year, int month, int day);

ctui_date ctui_dateinput_date(const ctui_dateinput *in);

/* Days since 1970-01-01 of the value, as ctui_date_to_days() counts them. */
long ctui_dateinput_days(const ctui_dateinput *in);

/* Set the value from a day count / a date; false (value kept) outside the
 * supported years or for an invalid date. Closes the picker. */
bool ctui_dateinput_set_days(ctui_dateinput *in, long days);
bool ctui_dateinput_set(ctui_dateinput *in, int year, int month, int day);

/* Change the selected segment by delta steps (as Up / Down do); true when
 * the date changed. */
bool ctui_dateinput_step(ctui_dateinput *in, int delta);

ctui_dateinput_event ctui_dateinput_key(ctui_dateinput *in, int key);

/* Left press at (y, x) relative to the field. */
ctui_dateinput_event ctui_dateinput_click(ctui_dateinput *in, int y, int x);

/* Wheel: positive notches step up. */
ctui_dateinput_event ctui_dateinput_wheel(ctui_dateinput *in, int notches);

/* Which segment a column of the field lands on, or -1 for the arrow and
 * the margins around the text. */
int ctui_dateinput_segment_at(int x);

/* Draw the field at (y, x); the open picker covers the
 * ctui_calendar_height() + 2 rows under it and ctui_calendar_width() + 2
 * columns from x, so draw this component after what lies below. */
void ctui_dateinput_draw(const ctui_dateinput *in, int y, int x, bool focused);

#endif /* CTUI_DATEINPUT_H */
