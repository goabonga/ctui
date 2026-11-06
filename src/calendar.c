/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * calendar.c - month grid with a cursor day (calendar.h).
 */
#include <ctui/calendar.h>

#include <curses.h>
#include <stdio.h>
#include <string.h>

#include <ctui/calendar_month.h>
#include <ctui/text.h>
#include <ctui/theme.h>

#define CELL 3		     /* columns per day: two digits and a space */
#define WIDTH (7 * CELL - 1) /* the last cell has no trailing space */
#define HEIGHT 8	     /* title, weekdays, six weeks */
#define MIN_YEAR 1
#define MAX_YEAR 9999

static const char *const MONTHS[12] = {
    "January", "February", "March",	"April",   "May",      "June",
    "July",    "August",   "September", "October", "November", "December"};
static const char *const WEEKDAYS[7] = {"Mo", "Tu", "We", "Th",
					"Fr", "Sa", "Su"};

bool ctui_date_leap(int year)
{
	return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

int ctui_date_days_in_month(int year, int month)
{
	static const int days[12] = {31, 28, 31, 30, 31, 30,
				     31, 31, 30, 31, 30, 31};
	if (month < 1 || month > 12)
		return 0;
	return days[month - 1] + (month == 2 && ctui_date_leap(year));
}

/* Days from civil (H. Hinnant), 1970-01-01 = 0. */
long ctui_date_to_days(int year, int month, int day)
{
	long y = year - (month <= 2);
	long era = (y >= 0 ? y : y - 399) / 400;
	long yoe = y - era * 400;
	long doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
	long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
	return era * 146097 + doe - 719468;
}

ctui_date ctui_date_from_days(long days)
{
	days += 719468;
	long era = (days >= 0 ? days : days - 146096) / 146097;
	long doe = days - era * 146097;
	long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
	long doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
	long mp = (5 * doy + 2) / 153;
	int day = (int)(doy - (153 * mp + 2) / 5 + 1);
	int month = (int)(mp < 10 ? mp + 3 : mp - 9);
	long year = yoe + era * 400 + (month <= 2);
	return (ctui_date){(int)year, month, day};
}

int ctui_date_weekday(int year, int month, int day)
{
	if (day < 1 || day > ctui_date_days_in_month(year, month))
		return -1;
	long d =
	    ctui_date_to_days(year, month, day); /* 1970-01-01 was a Thursday */
	return (int)(((d + 3) % 7 + 7) % 7);
}

int ctui_calendar_width(void)
{
	return WIDTH;
}

int ctui_calendar_height(void)
{
	return HEIGHT;
}

/* Days the first of the month lies after the start of the week. */
static int lead(int year, int month, int week_start)
{
	int first = ctui_date_weekday(year, month, 1);
	return first < 0 ? 0 : ((first - week_start) % 7 + 7) % 7;
}

bool ctui_calendar_cell(int year, int month, int week_start, int day, int *row,
			int *col)
{
	if (ctui_date_weekday(year, month, 1) < 0 || day < 1 ||
	    day > ctui_date_days_in_month(year, month))
		return false;
	int offset = lead(year, month, week_start) + day - 1;
	if (row)
		*row = offset / 7;
	if (col)
		*col = offset % 7;
	return true;
}

static bool in_range(int year, int month)
{
	return year >= MIN_YEAR && year <= MAX_YEAR && month >= 1 &&
	       month <= 12;
}

static void clamp_day(ctui_calendar *cal)
{
	int dim = ctui_date_days_in_month(cal->year, cal->month);
	if (cal->day > dim)
		cal->day = dim;
	if (cal->day < 1)
		cal->day = 1;
}

void ctui_calendar_init(ctui_calendar *cal, int year, int month, int day)
{
	*cal = (ctui_calendar){.year = 1970, .month = 1, .day = 1};
	if (in_range(year, month)) {
		cal->year = year;
		cal->month = month;
		cal->day = day;
		clamp_day(cal);
	}
}

void ctui_calendar_set_today(ctui_calendar *cal, int year, int month, int day)
{
	if (in_range(year, month) && day >= 1 &&
	    day <= ctui_date_days_in_month(year, month))
		cal->today = (ctui_date){year, month, day};
	else
		cal->today = (ctui_date){0, 0, 0};
}

bool ctui_calendar_show(ctui_calendar *cal, int year, int month)
{
	if (month < 1 || month > 12)
		return false;
	if (year < MIN_YEAR)
		year = MIN_YEAR;
	if (year > MAX_YEAR)
		year = MAX_YEAR;
	bool changed = year != cal->year || month != cal->month;
	cal->year = year;
	cal->month = month;
	clamp_day(cal);
	return changed;
}

ctui_calendar_event ctui_calendar_goto(ctui_calendar *cal, int year, int month,
				       int day)
{
	if (!in_range(year, month) || day < 1 ||
	    day > ctui_date_days_in_month(year, month))
		return CTUI_CALENDAR_NONE;
	bool month_changed = year != cal->year || month != cal->month;
	bool moved = month_changed || day != cal->day;
	cal->year = year;
	cal->month = month;
	cal->day = day;
	return month_changed ? CTUI_CALENDAR_MONTH
	       : moved	     ? CTUI_CALENDAR_MOVED
			     : CTUI_CALENDAR_NONE;
}

static long cursor_days(const ctui_calendar *cal)
{
	return ctui_date_to_days(cal->year, cal->month, cal->day);
}

/* Move by a number of days; refused beyond the supported years. */
static ctui_calendar_event step_days(ctui_calendar *cal, long delta)
{
	ctui_date d = ctui_date_from_days(cursor_days(cal) + delta);
	return ctui_calendar_goto(cal, d.year, d.month, d.day);
}

/* A move of the cursor with Shift: the selection becomes the range from its
 * anchor to the new cursor. */
static ctui_calendar_event extend_days(ctui_calendar *cal, long delta)
{
	long from = cursor_days(cal);
	ctui_cal_select next = cal->sel;
	if (!ctui_cal_select_extend(&next, from, from + delta))
		return CTUI_CALENDAR_NONE;
	ctui_calendar_event e = step_days(cal, delta);
	if (e == CTUI_CALENDAR_NONE)
		return e;
	cal->sel = next;
	return e == CTUI_CALENDAR_MONTH ? e : CTUI_CALENDAR_SELECTED;
}

/* A move without Shift forgets the anchor of the selection. */
static ctui_calendar_event plain(ctui_calendar *cal, ctui_calendar_event e)
{
	if (e != CTUI_CALENDAR_NONE)
		ctui_cal_select_move(&cal->sel);
	return e;
}

/* Move by months, keeping the day as far as the month allows. */
static ctui_calendar_event step_months(ctui_calendar *cal, int delta)
{
	long index = (long)cal->year * 12 + (cal->month - 1) + delta;
	int year = (int)(index / 12), month = (int)(index % 12) + 1;
	if (!in_range(year, month))
		return CTUI_CALENDAR_NONE;
	int dim = ctui_date_days_in_month(year, month);
	return ctui_calendar_goto(cal, year, month,
				  cal->day > dim ? dim : cal->day);
}

ctui_calendar_event ctui_calendar_key(ctui_calendar *cal, int key)
{
	switch (key) {
	case '\n':
	case KEY_ENTER:
		return CTUI_CALENDAR_ACTIVATED;
	case KEY_LEFT:
	case 'h':
		return plain(cal, step_days(cal, -1));
	case KEY_RIGHT:
	case 'l':
		return plain(cal, step_days(cal, 1));
	case KEY_UP:
	case 'k':
		return plain(cal, step_days(cal, -7));
	case KEY_DOWN:
	case 'j':
		return plain(cal, step_days(cal, 7));
	case KEY_SLEFT:
	case 'H':
		return extend_days(cal, -1);
	case KEY_SRIGHT:
	case 'L':
		return extend_days(cal, 1);
	case KEY_SR:
	case 'K':
		return extend_days(cal, -7);
	case KEY_SF:
	case 'J':
		return extend_days(cal, 7);
	case ' ':
		return ctui_cal_select_toggle(&cal->sel, cursor_days(cal))
			   ? CTUI_CALENDAR_SELECTED
			   : CTUI_CALENDAR_NONE;
	case 27:
		if (cal->sel.count == 0)
			return CTUI_CALENDAR_NONE;
		ctui_cal_select_clear(&cal->sel);
		return CTUI_CALENDAR_SELECTED;
	case KEY_PPAGE:
		return plain(cal, step_months(cal, -1));
	case KEY_NPAGE:
		return plain(cal, step_months(cal, 1));
	case KEY_HOME:
		return cal->today.year
			   ? plain(cal, ctui_calendar_goto(cal, cal->today.year,
							   cal->today.month,
							   cal->today.day))
			   : CTUI_CALENDAR_NONE;
	default:
		return CTUI_CALENDAR_NONE;
	}
}

ctui_calendar_part ctui_calendar_hit(const ctui_calendar *cal, int y, int x,
				     int *day)
{
	if (ctui_calendar_is_full(cal))
		return ctui_calendar_full_hit(cal->year, cal->month,
					      cal->week_start, cal->area_width,
					      cal->area_height, y, x, day);
	if (y < 0 || y >= HEIGHT || x < 0 || x >= WIDTH)
		return CTUI_CALENDAR_OUTSIDE;
	if (y == 0) {
		if (x < 3)
			return CTUI_CALENDAR_PREV;
		if (x >= WIDTH - 3)
			return CTUI_CALENDAR_NEXT;
		return CTUI_CALENDAR_TITLE;
	}
	if (y == 1)
		return CTUI_CALENDAR_WEEKDAYS;
	if (x % CELL == CELL - 1)
		return CTUI_CALENDAR_OUTSIDE; /* the gap between two columns */
	int d = (y - 2) * 7 + x / CELL -
		lead(cal->year, cal->month, cal->week_start) + 1;
	if (d < 1 || d > ctui_date_days_in_month(cal->year, cal->month))
		return CTUI_CALENDAR_OUTSIDE;
	if (day)
		*day = d;
	return CTUI_CALENDAR_DAY;
}

ctui_calendar_event ctui_calendar_press(ctui_calendar *cal, int y, int x)
{
	int day = 0;
	switch (ctui_calendar_hit(cal, y, x, &day)) {
	case CTUI_CALENDAR_PREV:
		return step_months(cal, -1);
	case CTUI_CALENDAR_NEXT:
		return step_months(cal, 1);
	case CTUI_CALENDAR_DAY: {
		bool cleared = ctui_cal_select_begin(
		    &cal->sel, ctui_date_to_days(cal->year, cal->month, day));
		ctui_calendar_event e =
		    ctui_calendar_goto(cal, cal->year, cal->month, day);
		return e == CTUI_CALENDAR_NONE && cleared
			   ? CTUI_CALENDAR_SELECTED
			   : e;
	}
	default:
		return CTUI_CALENDAR_NONE;
	}
}

ctui_calendar_event ctui_calendar_drag(ctui_calendar *cal, int y, int x)
{
	int day = 0;
	if (!cal->sel.dragging ||
	    ctui_calendar_hit(cal, y, x, &day) != CTUI_CALENDAR_DAY)
		return CTUI_CALENDAR_NONE;
	bool changed = ctui_cal_select_drag_to(
	    &cal->sel, ctui_date_to_days(cal->year, cal->month, day));
	ctui_calendar_event e =
	    ctui_calendar_goto(cal, cal->year, cal->month, day);
	return e == CTUI_CALENDAR_MOVED && changed  ? CTUI_CALENDAR_SELECTED
	       : e == CTUI_CALENDAR_NONE && changed ? CTUI_CALENDAR_SELECTED
						    : e;
}

ctui_calendar_event ctui_calendar_release(ctui_calendar *cal)
{
	ctui_cal_select_end(&cal->sel);
	return CTUI_CALENDAR_NONE;
}

ctui_calendar_event ctui_calendar_toggle_at(ctui_calendar *cal, int y, int x)
{
	int day = 0;
	if (ctui_calendar_hit(cal, y, x, &day) != CTUI_CALENDAR_DAY)
		return CTUI_CALENDAR_NONE;
	ctui_cal_select_toggle(&cal->sel,
			       ctui_date_to_days(cal->year, cal->month, day));
	ctui_calendar_goto(cal, cal->year, cal->month, day);
	return CTUI_CALENDAR_SELECTED;
}

uint32_t ctui_calendar_selected_mask(const ctui_calendar *cal)
{
	uint32_t mask = 0;
	int dim = ctui_date_days_in_month(cal->year, cal->month);
	for (int d = 1; d <= dim; d++)
		if (ctui_cal_select_contains(
			&cal->sel, ctui_date_to_days(cal->year, cal->month, d)))
			mask |= 1U << (unsigned)(d - 1);
	return mask;
}

ctui_calendar_event ctui_calendar_wheel(ctui_calendar *cal, int months)
{
	return months ? step_months(cal, months) : CTUI_CALENDAR_NONE;
}

/* ---- drawing ---- */

static void calendar_draw_impl(const ctui_calendar *cal, int y, int x)
{
	int body = ctui_attr(CTUI_PAIR_BODY, false);
	int heading = ctui_attr(CTUI_PAIR_HEADING, true);
	int muted = ctui_attr(CTUI_PAIR_MUTED, false);
	int button = ctui_attr(CTUI_PAIR_BORDER_SELECTED, true);

	/* title: "<", month and year centred, ">" */
	char title[CTUI_TEXT_MAX];
	const char *name = cal->month_names && cal->month_names[cal->month - 1]
			       ? cal->month_names[cal->month - 1]
			       : MONTHS[cal->month - 1];
	snprintf(title, sizeof title, "%s %d", name, cal->year);
	ctui_fill(y, x, WIDTH, heading);
	ctui_put(y, x, 1, "<", button);
	ctui_put(y, x + WIDTH - 1, 1, ">", button);
	int cols = (int)ctui_utf8_columns(title);
	int inner = WIDTH - 4;
	ctui_put(y, x + 2 + (inner > cols ? (inner - cols) / 2 : 0), inner,
		 title, heading);

	/* weekday names, two columns each */
	for (int c = 0; c < 7; c++) {
		int wd = (cal->week_start + c) % 7;
		const char *s = cal->weekday_names && cal->weekday_names[wd]
				    ? cal->weekday_names[wd]
				    : WEEKDAYS[wd];
		char buf[16];
		size_t n = ctui_utf8_prefix_bytes(s, 2);
		snprintf(buf, sizeof buf, "%.*s",
			 (int)(n < sizeof buf - 1 ? n : sizeof buf - 1), s);
		ctui_fill(y + 1, x + c * CELL, CELL - 1, muted);
		ctui_put(y + 1, x + c * CELL, 2, buf, muted);
	}

	for (int row = 0; row < 6; row++)
		ctui_fill(y + 2 + row, x, WIDTH, body);
	int dim = ctui_date_days_in_month(cal->year, cal->month);
	uint32_t sel_mask = ctui_calendar_selected_mask(cal);
	for (int d = 1; d <= dim; d++) {
		int row, col;
		ctui_calendar_cell(cal->year, cal->month, cal->week_start, d,
				   &row, &col);
		bool is_today = cal->today.year == cal->year &&
				cal->today.month == cal->month &&
				cal->today.day == d;
		bool marked = (cal->marked >> (unsigned)(d - 1)) & 1U;
		bool selected = (sel_mask >> (unsigned)(d - 1)) & 1U;
		int attr = d == cal->day ? ctui_attr(CTUI_PAIR_SELECTION, true)
			   : selected
			       ? ctui_attr(CTUI_PAIR_CURSOR_SELECTED, true)
			   : marked ? ctui_attr(CTUI_PAIR_CURSOR, true)
			   : (cal->spanned >> (unsigned)(d - 1)) & 1U
			       ? ctui_attr(CTUI_PAIR_GAUGE_YELLOW, true)
			       : body;
		if (is_today)
			attr = (int)((unsigned)attr | (unsigned)A_UNDERLINE);
		if (selected && col < 6 && d < dim &&
		    ((sel_mask >> (unsigned)d) & 1U))
			ctui_fill(y + 2 + row, x + col * CELL + 2, 1,
				  ctui_attr(CTUI_PAIR_CURSOR_SELECTED, false));
		char buf[12];
		snprintf(buf, sizeof buf, "%2d", d);
		ctui_put(y + 2 + row, x + col * CELL, 2, buf, attr);
	}
}

/* ---- the full layout ---- */

static const char *const WEEKDAYS3[7] = {"Mon", "Tue", "Wed", "Thu",
					 "Fri", "Sat", "Sun"};

static void full_draw_impl(const ctui_calendar *cal, int y, int x)
{
	int w = cal->area_width, h = cal->area_height;
	int body = ctui_attr(CTUI_PAIR_BODY, false);
	int heading = ctui_attr(CTUI_PAIR_HEADING, true);
	int muted = ctui_attr(CTUI_PAIR_MUTED, false);
	int button = ctui_attr(CTUI_PAIR_BORDER_SELECTED, true);
	const ctui_glyphs *g = ctui_theme_glyphs();

	char title[CTUI_TEXT_MAX];
	const char *name = cal->month_names && cal->month_names[cal->month - 1]
			       ? cal->month_names[cal->month - 1]
			       : MONTHS[cal->month - 1];
	snprintf(title, sizeof title, "%s %d", name, cal->year);
	ctui_fill(y, x, w, heading);
	ctui_put(y, x, 1, "<", button);
	ctui_put(y, x + w - 1, 1, ">", button);
	int cols = (int)ctui_utf8_columns(title);
	int inner = w - 6;
	ctui_put(y, x + 3 + (inner > cols ? (inner - cols) / 2 : 0), inner,
		 title, heading);

	for (int c = 0; c < 7; c++) {
		int wd = (cal->week_start + c) % 7;
		ctui_rect r = ctui_calendar_full_weekday(w, c);
		const char *s = cal->weekday_names && cal->weekday_names[wd]
				    ? cal->weekday_names[wd]
				    : WEEKDAYS3[wd];
		ctui_fill(y + r.y, x + r.x, r.width, muted);
		ctui_put(y + r.y, x + r.x, r.width - 1, s, muted);
	}

	for (int row = 2; row < h; row++)
		ctui_fill(y + row, x, w, body);
	int weeks = ctui_calendar_weeks(cal->year, cal->month, cal->week_start);
	int dim = ctui_date_days_in_month(cal->year, cal->month);
	uint32_t sel_mask = ctui_calendar_selected_mask(cal);
	for (int row = 0; row < weeks; row++) {
		for (int col = 0; col < 7; col++) {
			ctui_rect c =
			    ctui_calendar_full_cell(w, h, weeks, row, col);
			if (col < 6)
				for (int r = 0; r < c.height; r++)
					ctui_put(y + c.y + r,
						 x + c.x + c.width - 1, 1,
						 g->box_v, muted);
			if (c.height >= 4)
				for (int k = 0; k < c.width - (col < 6); k++)
					ctui_put(y + c.y + c.height - 1,
						 x + c.x + k, 1, g->box_h,
						 muted);
		}
	}
	for (int d = 1; d <= dim; d++) {
		int row, col;
		ctui_calendar_cell(cal->year, cal->month, cal->week_start, d,
				   &row, &col);
		ctui_rect c = ctui_calendar_full_cell(w, h, weeks, row, col);
		int iw = c.width - (col < 6);
		bool is_today = cal->today.year == cal->year &&
				cal->today.month == cal->month &&
				cal->today.day == d;
		bool marked = (cal->marked >> (unsigned)(d - 1)) & 1U;
		bool selected = (sel_mask >> (unsigned)(d - 1)) & 1U;
		int attr = d == cal->day ? ctui_attr(CTUI_PAIR_SELECTION, true)
			   : selected
			       ? ctui_attr(CTUI_PAIR_CURSOR_SELECTED, true)
			   : marked ? ctui_attr(CTUI_PAIR_CURSOR, true)
			   : (cal->spanned >> (unsigned)(d - 1)) & 1U
			       ? ctui_attr(CTUI_PAIR_GAUGE_YELLOW, true)
			       : body;
		if (is_today)
			attr = (int)((unsigned)attr | (unsigned)A_UNDERLINE);
		if (d == cal->day || selected)
			ctui_fill(y + c.y, x + c.x, iw, attr);
		char buf[12];
		snprintf(buf, sizeof buf, "%d", d);
		ctui_put(y + c.y, x + c.x, iw, buf, attr);
	}
}

void ctui_calendar_draw(const ctui_calendar *cal, int y, int x)
{
	ctui_style_begin(cal->style);
	if (ctui_calendar_is_full(cal))
		full_draw_impl(cal, y, x);
	else
		calendar_draw_impl(cal, y, x);
	ctui_style_end();
}
