/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * dateinput.c - date field with per-segment editing and a month picker
 * (dateinput.h).
 */
#include <ctui/dateinput.h>

#include <curses.h>
#include <stdio.h>
#include <string.h>

#include <ctui/frame.h>
#include <ctui/text.h>
#include <ctui/theme.h>

#define MIN_YEAR 1
#define MAX_YEAR 9999
#define ARROW_X 12 /* first column of the arrow */
/* The open picker is framed under the field: the frame takes one row and one
 * column around the calendar. */
#define PICKER_TOP 2 /* row of the first calendar row, under the field */
#define PICKER_LEFT 1

static const char *const LETTERS[7] = {"M", "T", "W", "T", "F", "S", "S"};

/* ---- pure ------------------------------------------------------- */

bool ctui_dateinput_valid(int year, int month, int day)
{
	return year >= MIN_YEAR && year <= MAX_YEAR && month >= 1 &&
	       month <= 12 && day >= 1 &&
	       day <= ctui_date_days_in_month(year, month);
}

static bool digits(const char *s, int n, int *out)
{
	int v = 0;
	for (int i = 0; i < n; i++) {
		if (s[i] < '0' || s[i] > '9')
			return false;
		v = v * 10 + (s[i] - '0');
	}
	*out = v;
	return true;
}

bool ctui_dateinput_parse(const char *s, ctui_date *out)
{
	int y, m, d;
	if (!s || strlen(s) != 10 || s[4] != '-' || s[7] != '-' ||
	    !digits(s, 4, &y) || !digits(s + 5, 2, &m) ||
	    !digits(s + 8, 2, &d) || !ctui_dateinput_valid(y, m, d))
		return false;
	if (out)
		*out = (ctui_date){y, m, d};
	return true;
}

char *ctui_dateinput_format(ctui_date date, char *buf, size_t cap)
{
	if (!buf || cap < CTUI_DATEINPUT_TEXT ||
	    !ctui_dateinput_valid(date.year, date.month, date.day))
		return NULL;
	snprintf(buf, cap, "%04d-%02d-%02d", date.year, date.month, date.day);
	return buf;
}

long ctui_dateinput_min_days(void)
{
	return ctui_date_to_days(MIN_YEAR, 1, 1);
}

long ctui_dateinput_max_days(void)
{
	return ctui_date_to_days(MAX_YEAR, 12, 31);
}

/* ---- value ------------------------------------------------------ */

void ctui_dateinput_init(ctui_dateinput *in, int year, int month, int day)
{
	if (!ctui_dateinput_valid(year, month, day)) {
		year = 1970;
		month = day = 1;
	}
	*in = (ctui_dateinput){.year = year, .month = month, .day = day};
}

ctui_date ctui_dateinput_date(const ctui_dateinput *in)
{
	return (ctui_date){in->year, in->month, in->day};
}

long ctui_dateinput_days(const ctui_dateinput *in)
{
	return ctui_date_to_days(in->year, in->month, in->day);
}

static void reset_entry(ctui_dateinput *in)
{
	in->typed = in->entry = 0;
}

bool ctui_dateinput_set(ctui_dateinput *in, int year, int month, int day)
{
	if (!ctui_dateinput_valid(year, month, day))
		return false;
	in->year = year;
	in->month = month;
	in->day = day;
	in->open = false;
	reset_entry(in);
	return true;
}

bool ctui_dateinput_set_days(ctui_dateinput *in, long days)
{
	if (days < ctui_dateinput_min_days() ||
	    days > ctui_dateinput_max_days())
		return false;
	ctui_date d = ctui_date_from_days(days);
	return ctui_dateinput_set(in, d.year, d.month, d.day);
}

static void clamp_day(ctui_dateinput *in)
{
	int dim = ctui_date_days_in_month(in->year, in->month);
	if (in->day > dim)
		in->day = dim;
}

/* v + delta wrapped into 1..n. */
static int wrap_step(int v, long delta, int n)
{
	long r = ((v - 1 + delta) % n + n) % n;
	return (int)r + 1;
}

bool ctui_dateinput_step(ctui_dateinput *in, int delta)
{
	reset_entry(in);
	ctui_date before = ctui_dateinput_date(in);
	switch (in->segment) {
	case CTUI_DATEINPUT_YEAR: {
		long y = (long)in->year + delta;
		in->year = y < MIN_YEAR	  ? MIN_YEAR
			   : y > MAX_YEAR ? MAX_YEAR
					  : (int)y;
		clamp_day(in);
		break;
	}
	case CTUI_DATEINPUT_MONTH:
		in->month = wrap_step(in->month, delta, 12);
		clamp_day(in);
		break;
	default:
		in->day =
		    wrap_step(in->day, delta,
			      ctui_date_days_in_month(in->year, in->month));
		break;
	}
	return before.year != in->year || before.month != in->month ||
	       before.day != in->day;
}

/* ---- typing ----------------------------------------------------- */

static ctui_dateinput_event move_segment(ctui_dateinput *in, int to)
{
	reset_entry(in);
	if (to < 0 || to > CTUI_DATEINPUT_DAY || to == in->segment)
		return CTUI_DATEINPUT_NONE;
	in->segment = to;
	return CTUI_DATEINPUT_MOVED;
}

static ctui_dateinput_event type_digit(ctui_dateinput *in, int d)
{
	int seg = in->segment;
	int limit = seg == CTUI_DATEINPUT_YEAR ? MAX_YEAR
		    : seg == CTUI_DATEINPUT_MONTH
			? 12
			: ctui_date_days_in_month(in->year, in->month);
	int max_digits = seg == CTUI_DATEINPUT_YEAR ? 4 : 2;
	int candidate = in->typed ? in->entry * 10 + d : d;
	if (in->typed &&
	    candidate > limit) { /* cannot continue: start over here */
		candidate = d;
		in->typed = 0;
	}
	in->entry = candidate;
	in->typed++;
	ctui_date before = ctui_dateinput_date(in);
	int value = candidate < 1 ? 1 : candidate;
	if (seg == CTUI_DATEINPUT_YEAR)
		in->year = value;
	else if (seg == CTUI_DATEINPUT_MONTH)
		in->month = value;
	else
		in->day = value;
	clamp_day(in);
	bool done = in->typed >= max_digits ||
		    (seg != CTUI_DATEINPUT_YEAR && candidate * 10 > limit);
	if (done) {
		reset_entry(in);
		if (seg < CTUI_DATEINPUT_DAY)
			in->segment++;
	}
	return before.year != in->year || before.month != in->month ||
		       before.day != in->day
		   ? CTUI_DATEINPUT_CHANGED
		   : CTUI_DATEINPUT_MOVED;
}

/* ---- picker ----------------------------------------------------- */

static void open_picker(ctui_dateinput *in)
{
	reset_entry(in);
	ctui_calendar_init(&in->picker, in->year, in->month, in->day);
	in->picker.style = in->style;
	in->picker.weekday_names =
	    in->weekday_names ? in->weekday_names : LETTERS;
	if (in->today.year)
		ctui_calendar_set_today(&in->picker, in->today.year,
					in->today.month, in->today.day);
	in->open = true;
}

/* Take the picker's cursor day as the value and close it. */
static ctui_dateinput_event pick(ctui_dateinput *in)
{
	ctui_date before = ctui_dateinput_date(in);
	int y = in->picker.year, m = in->picker.month, d = in->picker.day;
	in->open = false;
	if (!ctui_dateinput_valid(y, m, d))
		return CTUI_DATEINPUT_CLOSED;
	in->year = y;
	in->month = m;
	in->day = d;
	return before.year != y || before.month != m || before.day != d
		   ? CTUI_DATEINPUT_CHANGED
		   : CTUI_DATEINPUT_CLOSED;
}

static ctui_dateinput_event picker_key(ctui_dateinput *in, int key)
{
	if (key == 27) {
		in->open = false;
		return CTUI_DATEINPUT_CLOSED;
	}
	if (key == '\r')
		key = '\n';
	switch (ctui_calendar_key(&in->picker, key)) {
	case CTUI_CALENDAR_ACTIVATED:
		return pick(in);
	case CTUI_CALENDAR_NONE:
		return CTUI_DATEINPUT_NONE;
	default:
		return CTUI_DATEINPUT_MOVED;
	}
}

ctui_dateinput_event ctui_dateinput_key(ctui_dateinput *in, int key)
{
	if (in->open)
		return picker_key(in, key);
	switch (key) {
	case KEY_LEFT:
		return move_segment(in, in->segment - 1);
	case KEY_RIGHT:
	case '-':
	case '/':
	case '.':
		return move_segment(in, in->segment + 1);
	case KEY_UP:
		return ctui_dateinput_step(in, 1) ? CTUI_DATEINPUT_CHANGED
						  : CTUI_DATEINPUT_MOVED;
	case KEY_DOWN:
		return ctui_dateinput_step(in, -1) ? CTUI_DATEINPUT_CHANGED
						   : CTUI_DATEINPUT_MOVED;
	case '\n':
	case '\r':
	case KEY_ENTER:
		open_picker(in);
		return CTUI_DATEINPUT_OPENED;
	case 27:
		return CTUI_DATEINPUT_CANCELLED;
	default:
		if (key >= '0' && key <= '9')
			return type_digit(in, key - '0');
		return CTUI_DATEINPUT_NONE;
	}
}

/* ---- mouse ------------------------------------------------------ */

int ctui_dateinput_segment_at(int x)
{
	if (x >= 1 && x <= 5)
		return CTUI_DATEINPUT_YEAR;
	if (x >= 6 && x <= 8)
		return CTUI_DATEINPUT_MONTH;
	if (x >= 9 && x <= 10)
		return CTUI_DATEINPUT_DAY;
	return -1;
}

ctui_dateinput_event ctui_dateinput_click(ctui_dateinput *in, int y, int x)
{
	if (in->open) {
		int cy = y - PICKER_TOP, cx = x - PICKER_LEFT;
		if (cy >= 0 && cy < ctui_calendar_height() && cx >= 0 &&
		    cx < ctui_calendar_width()) {
			ctui_calendar_part part =
			    ctui_calendar_hit(&in->picker, cy, cx, NULL);
			ctui_calendar_event e =
			    ctui_calendar_press(&in->picker, cy, cx);
			if (part == CTUI_CALENDAR_DAY)
				return pick(in);
			return e == CTUI_CALENDAR_NONE ? CTUI_DATEINPUT_NONE
						       : CTUI_DATEINPUT_MOVED;
		}
		/* the frame and the margin around the calendar are part of it
		 */
		if (y >= 1 && y <= ctui_calendar_height() + 2 && x >= 0 &&
		    x < ctui_calendar_width() + 2)
			return CTUI_DATEINPUT_NONE;
		in->open = false;
		return CTUI_DATEINPUT_CLOSED;
	}
	if (y != 0 || x < 0 || x >= CTUI_DATEINPUT_WIDTH)
		return CTUI_DATEINPUT_NONE;
	if (x >= ARROW_X) {
		open_picker(in);
		return CTUI_DATEINPUT_OPENED;
	}
	int seg = ctui_dateinput_segment_at(x);
	return seg < 0 ? CTUI_DATEINPUT_NONE : move_segment(in, seg);
}

ctui_dateinput_event ctui_dateinput_wheel(ctui_dateinput *in, int notches)
{
	if (notches == 0)
		return CTUI_DATEINPUT_NONE;
	if (in->open)
		return ctui_calendar_wheel(&in->picker, -notches) ==
			       CTUI_CALENDAR_NONE
			   ? CTUI_DATEINPUT_NONE
			   : CTUI_DATEINPUT_MOVED;
	return ctui_dateinput_step(in, notches) ? CTUI_DATEINPUT_CHANGED
						: CTUI_DATEINPUT_NONE;
}

/* ---- drawing ---------------------------------------------------- */

void ctui_dateinput_draw(const ctui_dateinput *in, int y, int x, bool focused)
{
	ctui_style_begin(in->style);
	int attr = ctui_attr(focused ? CTUI_PAIR_SELECTION : CTUI_PAIR_DIALOG,
			     focused);
	ctui_fill(y, x, CTUI_DATEINPUT_WIDTH, attr);
	char text[CTUI_DATEINPUT_TEXT];
	snprintf(text, sizeof text, "%04d-%02d-%02d", in->year, in->month,
		 in->day);
	ctui_put(y, x + 1, 10, text, attr);
	if (focused) {
		static const int start[3] = {1, 6, 9}, len[3] = {4, 2, 2};
		int s = in->segment < 0 || in->segment > 2 ? 0 : in->segment;
		char part[8];
		snprintf(part, sizeof part, "%.*s", len[s],
			 text + start[s] - 1);
		ctui_put(y, x + start[s], len[s], part,
			 (int)((unsigned)attr | (unsigned)A_REVERSE));
	}
	const ctui_glyphs *g = ctui_theme_glyphs();
	ctui_put(y, x + ARROW_X, 1, in->open ? g->select_open : g->select_shut,
		 attr);
	if (in->open) {
		ctui_frame_draw(y + 1, x, ctui_calendar_height() + 2,
				ctui_calendar_width() + 2, false, true, NULL);
		/* the calendar takes the colours of the frame it sits in */
		ctui_style over = in->style ? *in->style : (ctui_style){0};
		int dialog = ctui_attr(CTUI_PAIR_DIALOG, false);
		over.attr[CTUI_PAIR_BODY] = dialog;
		over.attr[CTUI_PAIR_HEADING] =
		    ctui_attr(CTUI_PAIR_DIALOG, true);
		over.attr[CTUI_PAIR_MUTED] =
		    (int)((unsigned)dialog | (unsigned)A_DIM);
		ctui_calendar cal = in->picker;
		cal.style = &over;
		ctui_calendar_draw(&cal, y + PICKER_TOP, x + PICKER_LEFT);
	}
	ctui_style_end();
}
