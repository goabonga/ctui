/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * calendar.h - a month grid with a cursor day, chosen by keys or mouse.
 *
 * The calendar never reads the clock: the caller gives today's date, which
 * is drawn underlined and where Home goes. The first day of the week is
 * configurable (Monday by default). Days of the shown month can be marked
 * with a bitmask supplied by the caller (bit d-1 is day d); when the month
 * changes (CTUI_CALENDAR_MONTH) the caller refreshes `marked`.
 *
 * Month and weekday names are English unless `month_names` (12 names,
 * January first) or `weekday_names` (7 names, Monday first) are set; the
 * names must outlive the calendar. Weekday names are cut to two columns.
 *
 * Layout, drawn in ctui_calendar_width() x ctui_calendar_height() columns and
 * rows: a title row with "<" and ">" to change month, a row of weekday
 * names and six rows of weeks. Coordinates of the event functions are
 * relative to the top-left cell. Years 1 to 9999 are supported.
 *
 * Several days can be selected (`sel`, see calendar_select.h): Shift+Left /
 * Shift+Right (or H / L) extend a range by a day from the anchor,
 * Shift+Up / Shift+Down (or K / J) by a week, Space adds or removes the
 * cursor day (a selection that is not contiguous), Esc clears it. With the
 * mouse a drag over days selects the range, a right click toggles a day
 * (a Ctrl+click is not reported by every terminal) and a plain click clears
 * the selection and moves the cursor. Selected days are drawn on the info
 * tone, the days between two selected days of a row joined by a band.
 *
 * The grid can also fill an area of the screen (calendar_month.h): after
 * ctui_calendar_set_area() the same functions draw and hit-test it at that
 * size, with the compact layout as fallback in a small area.
 */
#ifndef CTUI_CALENDAR_H
#define CTUI_CALENDAR_H

#include <stdbool.h>
#include <stdint.h>

#include <ctui/calendar_select.h>
#include <ctui/theme.h>

typedef struct ctui_date {
	int year, month, day; /* month 1..12, day 1..31 */
} ctui_date;

typedef struct ctui_calendar {
	int year, month;  /* the month shown */
	int day;	  /* cursor day in that month */
	ctui_date today;  /* given by the caller, year 0 for none */
	int week_start;	  /* 0 = Monday ... 6 = Sunday */
	uint32_t marked;  /* bit d-1 set: day d of the shown month is marked */
	uint32_t spanned; /* bit d-1 set: an event lasting several days covers
			     day d (drawn in yellow, after the marked days; see
			     ctui_cal_span_month_mask()) */
	const char *const *month_names;	  /* 12 names or NULL */
	const char *const *weekday_names; /* 7 names, Monday first, or NULL */
	const ctui_style *style; /* colours and glyphs of this calendar, may be
				    NULL; set after init */
	ctui_cal_select sel;	 /* the selected days, see calendar_select.h */
	int area_width,
	    area_height; /* the area of the full layout, 0 for the compact grid;
			    set by ctui_calendar_set_area() (calendar_month.h)
			  */
} ctui_calendar;

typedef enum ctui_calendar_event {
	CTUI_CALENDAR_NONE = 0,
	CTUI_CALENDAR_MOVED, /* the cursor moved inside the month */
	CTUI_CALENDAR_MONTH, /* the shown month changed (cursor too): refresh
				`marked` */
	CTUI_CALENDAR_ACTIVATED, /* Enter */
	CTUI_CALENDAR_SELECTED /* the selected days changed (the cursor may have
				  moved too) */
} ctui_calendar_event;

typedef enum ctui_calendar_part {
	CTUI_CALENDAR_OUTSIDE = 0,
	CTUI_CALENDAR_PREV,  /* the "<" button */
	CTUI_CALENDAR_NEXT,  /* the ">" button */
	CTUI_CALENDAR_TITLE, /* month and year */
	CTUI_CALENDAR_WEEKDAYS,
	CTUI_CALENDAR_DAY
} ctui_calendar_part;

/* ---- pure date arithmetic (proleptic Gregorian) ---- */

bool ctui_date_leap(int year);

/* Days in month 1..12 of year; 0 when the month is out of range. */
int ctui_date_days_in_month(int year, int month);

/* Weekday of a valid date, 0 = Monday ... 6 = Sunday; -1 when invalid. */
int ctui_date_weekday(int year, int month, int day);

/* Days since 1970-01-01 (negative before), and the date of such a count. */
long ctui_date_to_days(int year, int month, int day);
ctui_date ctui_date_from_days(long days);

/* ---- layout ---- */

int ctui_calendar_width(void);
int ctui_calendar_height(void);

/* Grid position of `day` of the month: *row 0..5 (below the weekday row),
 * *col 0..6. False when the day is not in the month. */
bool ctui_calendar_cell(int year, int month, int week_start, int day, int *row,
			int *col);

/* Show year/month with the cursor on day (clamped to the month). An
 * invalid month or year leaves January 1970. */
void ctui_calendar_init(ctui_calendar *cal, int year, int month, int day);

/* Give today's date (an invalid date means none); Home goes there. */
void ctui_calendar_set_today(ctui_calendar *cal, int year, int month, int day);

/* Show year/month (year clamped to 1..9999), keeping the cursor day as far
 * as the month allows. True when the month changed. */
bool ctui_calendar_show(ctui_calendar *cal, int year, int month);

/* Move the cursor to a date, changing the month when needed. Dates outside
 * the supported years or the month are refused. */
ctui_calendar_event ctui_calendar_goto(ctui_calendar *cal, int year, int month,
				       int day);

/* Left/Right/h/l: a day, Up/Down/k/j: a week, PgUp/PgDn: a month, Home:
 * today (when given), Enter. With Shift (KEY_SLEFT, KEY_SRIGHT, KEY_SR,
 * KEY_SF, or H L K J) the move extends the selection from its anchor, Space
 * toggles the cursor day, Esc clears the selection. The result is
 * CTUI_CALENDAR_MONTH when the month changed, else SELECTED when the
 * selection changed, else MOVED. A move that would grow the selection past
 * CTUI_CAL_SELECT_MAX days is refused. */
ctui_calendar_event ctui_calendar_key(ctui_calendar *cal, int key);

/* What is at (y, x); for CTUI_CALENDAR_DAY, *day is the day of the shown
 * month (may be NULL). Cells before the first and after the last day, and
 * the gaps between columns, are outside. */
ctui_calendar_part ctui_calendar_hit(const ctui_calendar *cal, int y, int x,
				     int *day);

/* Left press: the buttons change the month, a day moves the cursor to it,
 * clears the selection and starts a drag. */
ctui_calendar_event ctui_calendar_press(ctui_calendar *cal, int y, int x);

/* Pointer moved with the left button down, and released: dragging over
 * days selects the range from the pressed day; the cursor follows. Days
 * outside the month are ignored. */
ctui_calendar_event ctui_calendar_drag(ctui_calendar *cal, int y, int x);
ctui_calendar_event ctui_calendar_release(ctui_calendar *cal);

/* Toggle the day at (y, x) in the selection and move the cursor to it (the
 * demo binds it to the right button). */
ctui_calendar_event ctui_calendar_toggle_at(ctui_calendar *cal, int y, int x);

/* The selected days of the shown month as a bitmask (bit d-1 is day d). */
uint32_t ctui_calendar_selected_mask(const ctui_calendar *cal);

/* Wheel: change the month by `months` (negative = earlier). */
ctui_calendar_event ctui_calendar_wheel(ctui_calendar *cal, int months);

void ctui_calendar_draw(const ctui_calendar *cal, int y, int x);

#endif /* CTUI_CALENDAR_H */
