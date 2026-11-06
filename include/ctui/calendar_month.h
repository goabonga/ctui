/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * calendar_month.h - the month grid at the size of its area.
 *
 * ctui_calendar is drawn compact (ctui_calendar_width() x
 * ctui_calendar_height(), 20 x 8) unless the caller gives it an area with
 * ctui_calendar_set_area(): when the area is large enough the grid then fills
 * all of it, in the same functions (ctui_calendar_draw(), ctui_calendar_hit(),
 * the clicks, the drag and the right click), so that keys, wheel, selection and
 * mouse work the same in both layouts. Coordinates are relative to the top-left
 * cell of the area, which is also where ctui_calendar_draw() is given.
 *
 * Full layout in a width x height area:
 *
 *   row 0         "<", the month and the year centred, ">"; the arrows are
 *                 the three leftmost and the three rightmost cells
 *   row 1         the weekday names
 *   rows 2 ..     the weeks, ctui_calendar_weeks() of them (5 or 6, the month
 *                 needs the sixth only when it starts late in a week), which
 *                 share the height; the seven columns share the width. The
 *                 remainder of the division is spread over them, so cells
 *                 differ by one cell at most
 *
 * A cell holds a day: its number on the first row. A cell of 4 rows or more
 * ends with a rule row, a column that is not the last ends with a vertical
 * bar; both belong to the cell for the mouse. The full layout needs at
 * least CTUI_CALENDAR_FULL_MIN_WIDTH columns and two rows for each week plus
 * the two header rows (12 rows for a month of 5 weeks, 14 for 6); in a
 * smaller area, or without an area, the compact grid is drawn.
 *
 * Events in the cells. In the full layout ctui_cal_month_events_draw() writes
 * under the number of each day one row for each event of that day: single
 * events, occurrences of series (with their exceptions left out) and the
 * days covered by events that last several days, ordered by the time they
 * start on that day (a day in the middle of a long event counts from
 * midnight). The row starts with a coloured block in the `pair` of the event,
 * or is filled with its colour (calendar_color.h), and the title is clipped
 * with the ellipsis of the theme. When the cell has too few rows for all
 * of them the last row says "+N" for the N that do not fit. The events come
 * from the same arrays as the week view (ctui_cal_month_events), never copied.
 * ctui_cal_month_event_at() tells which event is at a cell, so that a click
 * on a row can pick the event and a click elsewhere in the cell the day.
 */
#ifndef CTUI_CALENDAR_MONTH_H
#define CTUI_CALENDAR_MONTH_H

#include <stdbool.h>

#include <stddef.h>

#include <ctui/calendar.h>
#include <ctui/calendar_view.h>
#include <ctui/dialog.h>

#define CTUI_CALENDAR_FULL_MIN_WIDTH 40

/* Rows of weeks the month takes from the week start: 5 or 6. */
int ctui_calendar_weeks(int year, int month, int week_start);

/* Rows needed by the full layout for that many weeks (header included). */
int ctui_calendar_full_min_height(int weeks);

/* Does the full layout fit in the area, for this month? */
bool ctui_calendar_full_fits(int year, int month, int week_start, int width,
			     int height);

/* The cell (row 0..weeks-1, col 0..6) of the full layout in an area of
 * width x height; a zero rectangle when out of range. */
ctui_rect ctui_calendar_full_cell(int width, int height, int weeks, int row,
				  int col);

/* The weekday name of a column (one row at y 1), the "<" and ">" buttons
 * and the title between them (row 0). */
ctui_rect ctui_calendar_full_weekday(int width, int col);
ctui_rect ctui_calendar_full_arrow(int width, bool next);
ctui_rect ctui_calendar_full_title(int width);

/* Same as ctui_calendar_hit() for the full layout of that month; for
 * CTUI_CALENDAR_DAY, *day is the day (may be NULL). Cells of days outside the
 * month are outside. */
ctui_calendar_part ctui_calendar_full_hit(int year, int month, int week_start,
					  int width, int height, int y, int x,
					  int *day);

/* Give the calendar its area (0 x 0 for none: the compact grid) and say
 * whether the full layout is used now. Call it before drawing and handling
 * events, when the area or the month may have changed. */
bool ctui_calendar_set_area(ctui_calendar *cal, int width, int height);

/* Is the full layout in use (an area was given and it fits)? */
bool ctui_calendar_is_full(const ctui_calendar *cal);

/* ---- events in the cells ---- */

/* The caller's events, as for ctui_cal_view (recurs, spans and colors may be
 * NULL, each parallel to events). */
typedef struct ctui_cal_month_events {
	const ctui_cal_event *events;
	const ctui_cal_recur *recurs;
	const ctui_cal_span *spans;
	const ctui_cal_color *colors;
	int count;
	int selected; /* the picked event, drawn underlined, -1 for none */
} ctui_cal_month_events;

/* The events that occur on `day` (or last through it), in the order they
 * start on it, then by index. Up to `max` indexes are written to `index`
 * (may be NULL); the number of events of that day in all is returned. */
int ctui_cal_month_day_events(const ctui_cal_month_events *ev, long day,
			      int *index, int max);

/* Rows of a cell of that height that can hold events: the number row and, in
 * a cell of 4 rows or more, the rule row are not counted. */
int ctui_cal_month_rows(int cell_height);

/* Of n events in `rows` rows, how many are shown: all when they fit, else
 * one row less (the last one says "+N"); *hidden (may be NULL) is the N. */
int ctui_cal_month_visible(int rows, int n, int *hidden);

/* Copy title into out (cap bytes) in at most `cols` columns: cut with
 * `ellipsis` (NULL for "...") when it is longer, on a character boundary,
 * so that a wide character is never split. A title that fits is copied as
 * it is, NULL is the empty string. Returns the columns of the result. */
int ctui_cal_month_clip(const char *title, int cols, const char *ellipsis,
			char *out, size_t cap);

/* Draw the events in the cells of the full layout (nothing in the compact
 * one) at the same (y, x) as ctui_calendar_draw(), after it. */
void ctui_cal_month_events_draw(const ctui_calendar *cal,
				const ctui_cal_month_events *ev, int y, int x);

/* The event drawn at (y, x) in the area (its index), or -1 when the point
 * is on a day without an event there, on its "+N" row or off the cells;
 * *day (may be NULL) is the day of the cell, 0 when not in a day. */
int ctui_cal_month_event_at(const ctui_calendar *cal,
			    const ctui_cal_month_events *ev, int y, int x,
			    long *day);

#endif /* CTUI_CALENDAR_MONTH_H */
