/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * calendar_month.c - layout and hit testing of the full month grid
 * (calendar_month.h).
 */
#include <ctui/calendar_month.h>

#include <curses.h>
#include <stdlib.h>
#include <string.h>

#include <ctui/text.h>
#include <ctui/theme.h>

#define HEADER_ROWS 2 /* the title and the weekday names */

int ctui_calendar_weeks(int year, int month, int week_start)
{
	int first = ctui_date_weekday(year, month, 1);
	int dim = ctui_date_days_in_month(year, month);
	if (first < 0 || dim < 1)
		return 5;
	int lead = ((first - week_start) % 7 + 7) % 7;
	int weeks = (lead + dim + 6) / 7;
	return weeks < 5 ? 5 : weeks;
}

int ctui_calendar_full_min_height(int weeks)
{
	return HEADER_ROWS + 2 * weeks;
}

bool ctui_calendar_full_fits(int year, int month, int week_start, int width,
			     int height)
{
	return width >= CTUI_CALENDAR_FULL_MIN_WIDTH &&
	       height >= ctui_calendar_full_min_height(
			     ctui_calendar_weeks(year, month, week_start));
}

/* The columns [x0, x1) of column col in a row of `width` cells. */
static void column_span(int width, int col, int *x0, int *x1)
{
	*x0 = col * width / 7;
	*x1 = (col + 1) * width / 7;
}

static void row_span(int height, int weeks, int row, int *y0, int *y1)
{
	int body = height - HEADER_ROWS;
	*y0 = HEADER_ROWS + row * body / weeks;
	*y1 = HEADER_ROWS + (row + 1) * body / weeks;
}

ctui_rect ctui_calendar_full_cell(int width, int height, int weeks, int row,
				  int col)
{
	if (weeks < 1 || row < 0 || row >= weeks || col < 0 || col > 6 ||
	    width < 7 || height <= HEADER_ROWS)
		return (ctui_rect){0, 0, 0, 0};
	int x0, x1, y0, y1;
	column_span(width, col, &x0, &x1);
	row_span(height, weeks, row, &y0, &y1);
	return (ctui_rect){y0, x0, y1 - y0, x1 - x0};
}

ctui_rect ctui_calendar_full_weekday(int width, int col)
{
	if (col < 0 || col > 6 || width < 7)
		return (ctui_rect){0, 0, 0, 0};
	int x0, x1;
	column_span(width, col, &x0, &x1);
	return (ctui_rect){1, x0, 1, x1 - x0};
}

ctui_rect ctui_calendar_full_arrow(int width, bool next)
{
	return (ctui_rect){0, next ? width - 3 : 0, 1, 3};
}

ctui_rect ctui_calendar_full_title(int width)
{
	return (ctui_rect){0, 3, 1, width > 6 ? width - 6 : 0};
}

ctui_calendar_part ctui_calendar_full_hit(int year, int month, int week_start,
					  int width, int height, int y, int x,
					  int *day)
{
	if (y < 0 || y >= height || x < 0 || x >= width)
		return CTUI_CALENDAR_OUTSIDE;
	if (y == 0) {
		if (x < 3)
			return CTUI_CALENDAR_PREV;
		if (x >= width - 3)
			return CTUI_CALENDAR_NEXT;
		return CTUI_CALENDAR_TITLE;
	}
	if (y == 1)
		return CTUI_CALENDAR_WEEKDAYS;
	int weeks = ctui_calendar_weeks(year, month, week_start);
	int row = -1, col = -1, y0, y1, x0, x1;
	for (int r = 0; r < weeks && row < 0; r++) {
		row_span(height, weeks, r, &y0, &y1);
		if (y >= y0 && y < y1)
			row = r;
	}
	for (int c = 0; c < 7 && col < 0; c++) {
		column_span(width, c, &x0, &x1);
		if (x >= x0 && x < x1)
			col = c;
	}
	if (row < 0 || col < 0)
		return CTUI_CALENDAR_OUTSIDE;
	int first = ctui_date_weekday(year, month, 1);
	int lead = ((first - week_start) % 7 + 7) % 7;
	int d = row * 7 + col - lead + 1;
	if (d < 1 || d > ctui_date_days_in_month(year, month))
		return CTUI_CALENDAR_OUTSIDE;
	if (day)
		*day = d;
	return CTUI_CALENDAR_DAY;
}

bool ctui_calendar_set_area(ctui_calendar *cal, int width, int height)
{
	cal->area_width = width > 0 && height > 0 ? width : 0;
	cal->area_height = width > 0 && height > 0 ? height : 0;
	return ctui_calendar_is_full(cal);
}

bool ctui_calendar_is_full(const ctui_calendar *cal)
{
	return cal->area_width > 0 &&
	       ctui_calendar_full_fits(cal->year, cal->month, cal->week_start,
				       cal->area_width, cal->area_height);
}

/* ---- events in the cells ---- */

/* The minute event i starts on `day`, or -1 when it is not there. */
static int start_on(const ctui_cal_month_events *ev, int i, long day)
{
	const ctui_cal_recur *r = ev->recurs ? &ev->recurs[i] : NULL;
	const ctui_cal_span *sp = ev->spans ? &ev->spans[i] : NULL;
	long occ;
	int s, t;
	if (!ctui_cal_event_covers(&ev->events[i], r, sp, day, &occ) ||
	    !ctui_cal_event_segment(&ev->events[i], sp, occ, day, &s, &t))
		return -1;
	return s;
}

int ctui_cal_month_day_events(const ctui_cal_month_events *ev, long day,
			      int *index, int max)
{
	if (!ev || !ev->events || ev->count < 1)
		return 0;
	int *start = malloc(sizeof(int) * (size_t)ev->count);
	if (!start)
		return 0;
	int n = 0;
	for (int i = 0; i < ev->count; i++) {
		start[i] = start_on(ev, i, day);
		n += start[i] >= 0;
	}
	int last = -1; /* the index of the last one written: later (start,
			  index) pairs follow it */
	for (int k = 0; k < max && k < n && index; k++) {
		int best = -1;
		for (int i = 0; i < ev->count; i++) {
			if (start[i] < 0)
				continue;
			bool after = last < 0 || start[i] > start[last] ||
				     (start[i] == start[last] && i > last);
			if (after && (best < 0 || start[i] < start[best]))
				best = i;
		}
		if (best < 0)
			break;
		index[k] = best;
		last = best;
	}
	free(start);
	return n;
}

int ctui_cal_month_rows(int cell_height)
{
	int rows = cell_height - 1 - (cell_height >= 4 ? 1 : 0);
	return rows < 0 ? 0 : rows;
}

int ctui_cal_month_visible(int rows, int n, int *hidden)
{
	int shown = n <= rows ? n : rows - 1;
	if (shown < 0)
		shown = 0;
	if (n < 0)
		n = shown = 0;
	if (hidden)
		*hidden = n - shown;
	return shown;
}

int ctui_cal_month_clip(const char *title, int cols, const char *ellipsis,
			char *out, size_t cap)
{
	if (cap == 0)
		return 0;
	out[0] = '\0';
	if (!title)
		title = "";
	if (cols < 0)
		cols = 0;
	if (!ellipsis)
		ellipsis = "...";
	size_t have = ctui_utf8_columns(title);
	const char *tail = "";
	size_t keep;
	if (have <= (size_t)cols) {
		keep = strlen(title);
	} else {
		size_t ew = ctui_utf8_columns(ellipsis);
		if (ew >=
		    (size_t)cols) { /* only room for (a part of) the ellipsis */
			title = ellipsis;
			keep = ctui_utf8_prefix_bytes(ellipsis, (size_t)cols);
		} else {
			keep = ctui_utf8_prefix_bytes(title, (size_t)cols - ew);
			tail = ellipsis;
		}
	}
	if (keep >= cap)
		keep = cap - 1;
	memcpy(out, title, keep);
	out[keep] = '\0';
	size_t room = cap - 1 - keep;
	size_t tl = strlen(tail);
	if (tl > room)
		tl = room;
	memcpy(out + keep, tail, tl);
	out[keep + tl] = '\0';
	return (int)ctui_utf8_columns(out);
}

/* The width of a cell for its events: the vertical bar is not theirs. */
static int inner_width(const ctui_rect *c, int col)
{
	return c->width - (col < 6);
}

/* Draw one event row of `iw` cells at (y, x). */
static void event_row(const ctui_cal_month_events *ev, int i, int y, int x,
		      int iw)
{
	const ctui_cal_event *e = &ev->events[i];
	const ctui_glyphs *g = ctui_theme_glyphs();
	int color = ev->colors ? ev->colors[i].index : 0;
	bool picked = i == ev->selected;
	int extra =
	    picked ? (int)((int)((unsigned)A_UNDERLINE | (unsigned)A_BOLD)) : 0;
	char text[CTUI_TEXT_MAX];
	int start;
	int attr;
	if (color) {
		attr = (int)((unsigned)ctui_cal_color_attr(color, false) |
			     (unsigned)extra);
		ctui_fill(y, x, iw, attr);
		start = 1;
	} else {
		int pair = ctui_attr(
		    e->pair ? (ctui_pair)e->pair : CTUI_PAIR_DIALOG, false);
		attr = (int)((unsigned)ctui_attr(CTUI_PAIR_BODY, false) |
			     (unsigned)extra);
		ctui_put(y, x, 1, g->gauge_filled, pair);
		start = 2;
	}
	if (iw <= start)
		return;
	ctui_cal_month_clip(e->title ? e->title : "", iw - start, g->ellipsis,
			    text, sizeof text);
	ctui_put(y, x + start, iw - start, text, attr);
}

void ctui_cal_month_events_draw(const ctui_calendar *cal,
				const ctui_cal_month_events *ev, int y, int x)
{
	if (!ev || !ctui_calendar_is_full(cal))
		return;
	int w = cal->area_width, h = cal->area_height;
	int weeks = ctui_calendar_weeks(cal->year, cal->month, cal->week_start);
	int dim = ctui_date_days_in_month(cal->year, cal->month);
	ctui_style_begin(cal->style);
	for (int d = 1; d <= dim; d++) {
		int row, col;
		ctui_calendar_cell(cal->year, cal->month, cal->week_start, d,
				   &row, &col);
		ctui_rect c = ctui_calendar_full_cell(w, h, weeks, row, col);
		int rows = ctui_cal_month_rows(c.height),
		    iw = inner_width(&c, col);
		if (rows < 1 || iw < 2)
			continue;
		int idx[32];
		int cap = rows < 32 ? rows : 32;
		int n = ctui_cal_month_day_events(
		    ev, ctui_date_to_days(cal->year, cal->month, d), idx, cap);
		int hidden, shown = ctui_cal_month_visible(rows, n, &hidden);
		if (shown > cap)
			shown = cap;
		for (int k = 0; k < shown; k++)
			event_row(ev, idx[k], y + c.y + 1 + k, x + c.x, iw);
		if (hidden > 0) {
			char more[16];
			snprintf(more, sizeof more, "+%d", hidden);
			ctui_put(y + c.y + 1 + shown, x + c.x + 1, iw - 1, more,
				 ctui_attr(CTUI_PAIR_MUTED, true));
		}
	}
	ctui_style_end();
}

int ctui_cal_month_event_at(const ctui_calendar *cal,
			    const ctui_cal_month_events *ev, int y, int x,
			    long *day)
{
	int d = 0;
	if (day)
		*day = 0;
	if (!ctui_calendar_is_full(cal) ||
	    ctui_calendar_hit(cal, y, x, &d) != CTUI_CALENDAR_DAY)
		return -1;
	long days = ctui_date_to_days(cal->year, cal->month, d);
	if (day)
		*day = days;
	if (!ev)
		return -1;
	int weeks = ctui_calendar_weeks(cal->year, cal->month, cal->week_start);
	int row, col;
	ctui_calendar_cell(cal->year, cal->month, cal->week_start, d, &row,
			   &col);
	ctui_rect c = ctui_calendar_full_cell(cal->area_width, cal->area_height,
					      weeks, row, col);
	int rows = ctui_cal_month_rows(c.height), k = y - c.y - 1;
	if (k < 0 || k >= rows || x - c.x >= inner_width(&c, col))
		return -1;
	int idx[32];
	int cap = rows < 32 ? rows : 32;
	int n = ctui_cal_month_day_events(ev, days, idx, cap);
	int shown = ctui_cal_month_visible(rows, n, NULL);
	if (shown > cap)
		shown = cap;
	return k < shown ? idx[k] : -1;
}
