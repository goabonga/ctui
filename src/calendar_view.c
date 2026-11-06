/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * calendar_view.c - week and day views of the calendar (calendar_view.h).
 */
#include <ctui/calendar_view.h>

#include <curses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ctui/text.h>
#include <ctui/theme.h>

#define AXIS 6 /* "09:00" and a space */
#define DAY_MINUTES 1440
#define MIN_YEAR 1
#define MAX_YEAR 9999

static const char *const WEEKDAYS[7] = {"Mon", "Tue", "Wed", "Thu",
					"Fri", "Sat", "Sun"};
static const char *const LONG_WEEKDAYS[7] = {"Monday",	 "Tuesday", "Wednesday",
					     "Thursday", "Friday",  "Saturday",
					     "Sunday"};

static int slot_of(const ctui_cal_view *v)
{
	return v->slot_minutes >= 1 && v->slot_minutes <= 720 ? v->slot_minutes
							      : 30;
}

static int fdiv(int a, int b)
{
	int q = a / b;
	return a % b < 0 ? q - 1 : q;
}

static bool valid_date(int year, int month, int day)
{
	return year >= MIN_YEAR && year <= MAX_YEAR && month >= 1 &&
	       month <= 12 && day >= 1 &&
	       day <= ctui_date_days_in_month(year, month);
}

void ctui_cal_view_init(ctui_cal_view *v, ctui_cal_view_mode mode, int year,
			int month, int day)
{
	*v = (ctui_cal_view){.mode = mode,
			     .date = {1970, 1, 1},
			     .minute = 8 * 60,
			     .top = 7 * 60,
			     .slot_minutes = 30,
			     .now_minute = -1,
			     .selected = -1};
	if (valid_date(year, month, day))
		v->date = (ctui_date){year, month, day};
}

void ctui_cal_view_set_today(ctui_cal_view *v, int year, int month, int day,
			     int now_minute)
{
	v->today = valid_date(year, month, day) ? (ctui_date){year, month, day}
						: (ctui_date){0, 0, 0};
	v->now_minute =
	    now_minute >= 0 && now_minute < DAY_MINUTES ? now_minute : -1;
}

void ctui_cal_view_set_recurs(ctui_cal_view *v, const ctui_cal_recur *recurs)
{
	v->recurs = recurs;
}

void ctui_cal_view_set_spans(ctui_cal_view *v, const ctui_cal_span *spans)
{
	v->spans = spans;
}

void ctui_cal_view_set_events(ctui_cal_view *v, const ctui_cal_event *events,
			      int count)
{
	v->events = events;
	v->event_count = events && count > 0 ? count : 0;
	v->selected = -1;
	v->edit_events = NULL;
	v->edit_recurs = NULL;
	v->edit = (ctui_cal_edit){0};
	v->scope_pending = false;
}

int ctui_cal_view_columns(const ctui_cal_view *v)
{
	return v->mode == CTUI_CAL_VIEW_DAY ? 1 : 7;
}

long ctui_cal_view_first_day(const ctui_cal_view *v)
{
	long d = ctui_date_to_days(v->date.year, v->date.month, v->date.day);
	if (v->mode == CTUI_CAL_VIEW_DAY)
		return d;
	int wd = ctui_date_weekday(v->date.year, v->date.month, v->date.day);
	return d - ((wd - v->week_start) % 7 + 7) % 7;
}

long ctui_cal_view_day_of(const ctui_cal_view *v, int col)
{
	return ctui_cal_view_first_day(v) + col;
}

int ctui_cal_view_rows(int height)
{
	return height > 1 ? height - 1 : 0;
}

/* Column width including the separator, and the usable width. 0 when the
 * box is too narrow. */
static void geometry(const ctui_cal_view *v, int width, int *cw, int *inner)
{
	int n = ctui_cal_view_columns(v);
	int w = (width - AXIS) / n;
	if (w < 2)
		w = 0;
	*cw = w;
	*inner = n > 1 && w > 0 ? w - 1 : w;
}

static int last_slot_start(int slot)
{
	return (DAY_MINUTES - 1) / slot * slot;
}

/* The highest top: the last row reaches midnight, or goes past it. */
static int max_top(int rows, int slot)
{
	if (rows <= 0)
		return 0;
	int m = DAY_MINUTES - rows * slot;
	return m <= 0 ? 0 : (m + slot - 1) / slot * slot;
}

void ctui_cal_view_reveal(ctui_cal_view *v, int height)
{
	int slot = slot_of(v), rows = ctui_cal_view_rows(height);
	v->minute = v->minute < 0 ? 0 : v->minute / slot * slot;
	if (v->minute > last_slot_start(slot))
		v->minute = last_slot_start(slot);
	v->top = v->top < 0 ? 0 : v->top / slot * slot;
	if (rows > 0) {
		if (v->minute < v->top)
			v->top = v->minute;
		else if (v->minute + slot > v->top + rows * slot)
			v->top = v->minute - (rows - 1) * slot;
	}
	if (v->top > max_top(rows, slot))
		v->top = max_top(rows, slot);
	if (v->top < 0)
		v->top = 0;
}

/* ---- event layout ---- */

/* The minutes of an event clipped to its day; false when it has none. */
static bool span(const ctui_cal_event *e, int *s, int *t)
{
	int a = e->start_minute, b = e->end_minute;
	if (a < 0)
		a = 0;
	if (a >= DAY_MINUTES)
		return false;
	if (b <= a)
		b = a + 1;
	if (b > DAY_MINUTES)
		b = DAY_MINUTES;
	*s = a;
	*t = b;
	return true;
}

static const ctui_cal_recur *recur_of(const ctui_cal_view *v, int i)
{
	return v->recurs ? &v->recurs[i] : NULL;
}

static const ctui_cal_span *span_of(const ctui_cal_view *v, int i)
{
	return v->spans ? &v->spans[i] : NULL;
}

/* The minutes of event i on `day`, and the day its occurrence there starts. */
static bool seg_of(const ctui_cal_view *v, int i, long day, int *s, int *t,
		   long *occ)
{
	long o;
	if (!ctui_cal_event_covers(&v->events[i], recur_of(v, i), span_of(v, i),
				   day, &o) ||
	    !ctui_cal_event_segment(&v->events[i], span_of(v, i), o, day, s, t))
		return false;
	if (occ)
		*occ = o;
	return true;
}

int ctui_cal_layout(const ctui_cal_event *events, int count, long day,
		    int *lane, int *lanes)
{
	return ctui_cal_layout_span(events, NULL, NULL, count, day, lane,
				    lanes);
}

int ctui_cal_layout_recur(const ctui_cal_event *events,
			  const ctui_cal_recur *recurs, int count, long day,
			  int *lane, int *lanes)
{
	return ctui_cal_layout_span(events, recurs, NULL, count, day, lane,
				    lanes);
}

int ctui_cal_layout_span(const ctui_cal_event *events,
			 const ctui_cal_recur *recurs,
			 const ctui_cal_span *spans, int count, long day,
			 int *lane, int *lanes)
{
	int n = 0;
	int *from = count > 0 ? malloc(sizeof(int) * 2 * (size_t)count) : NULL;
	if (count > 0 && !from) {
		for (int i = 0; i < count; i++)
			lane[i] = lanes[i] = -1;
		return 0;
	}
	int *to = from + count;
	for (int i = 0; i < count; i++) {
		long occ;
		bool here =
		    ctui_cal_event_covers(
			&events[i], recurs ? &recurs[i] : NULL,
			spans ? &spans[i] : NULL, day, &occ) &&
		    ctui_cal_event_segment(&events[i], spans ? &spans[i] : NULL,
					   occ, day, &from[i], &to[i]);
		lane[i] = here ? -2 : -1; /* -2: to place */
		lanes[i] = -1;
		n += here;
	}
	/* Place the events by start time; a cluster is a run of overlapping
	 * ones. lanes[] holds the cluster number until the end. */
	int cluster = 0, cluster_end = -1;
	for (int placed = 0; placed < n; placed++) {
		int best = -1, bs = 0, bt = 0;
		for (int i = 0; i < count; i++) {
			if (lane[i] != -2)
				continue;
			if (best < 0 || from[i] < bs ||
			    (from[i] == bs && to[i] < bt)) {
				best = i;
				bs = from[i];
				bt = to[i];
			}
		}
		if (bs >= cluster_end) {
			cluster++;
			cluster_end = bt;
		} else if (bt > cluster_end) {
			cluster_end = bt;
		}
		int l = 0;
		for (;; l++) {
			bool used = false;
			for (int j = 0; j < count && !used; j++)
				used = lane[j] == l && lanes[j] == cluster &&
				       to[j] > bs;
			if (!used)
				break;
		}
		// NOLINTNEXTLINE(clang-analyzer-security.ArrayBound)
		lane[best] = l;
		lanes[best] = cluster;
	}
	/* Turn the cluster numbers into lane counts (encoded below -1 while
	 * converting). */
	for (int c = 1; c <= cluster; c++) {
		int m = 0;
		for (int j = 0; j < count; j++)
			if (lanes[j] == c && lane[j] + 1 > m)
				m = lane[j] + 1;
		for (int j = 0; j < count; j++)
			if (lanes[j] == c)
				lanes[j] = -m - 1;
	}
	for (int j = 0; j < count; j++)
		if (lanes[j] < -1)
			lanes[j] = -lanes[j] - 1;
	free(from);
	return n;
}

/* Rectangle of an event of column col, given its minutes s..t of that day
 * and its place in the layout of the day. */
static bool rect_of(const ctui_cal_view *v, int width, int height, int s, int t,
		    int col, int lane, int lanes, int *y, int *x, int *h,
		    int *w)
{
	int cw, inner;
	geometry(v, width, &cw, &inner);
	int rows = ctui_cal_view_rows(height), slot = slot_of(v);
	if (inner < 1 || rows < 1 || lanes < 1)
		return false;
	int r0 = fdiv(s - v->top, slot);
	int r1 = fdiv(t - v->top + slot - 1, slot);
	if (r1 <= r0)
		r1 = r0 + 1;
	if (r0 < 0)
		r0 = 0;
	if (r1 > rows)
		r1 = rows;
	if (r1 <= r0)
		return false;
	int lw = inner / lanes;
	if (lw < 1)
		lw = 1;
	int x0 = lane * lw;
	if (x0 >= inner)
		return false;
	int x1 = lane == lanes - 1 || x0 + lw > inner ? inner : x0 + lw;
	*y = 1 + r0;
	*h = r1 - r0;
	*x = AXIS + col * cw + x0;
	*w = x1 - x0;
	return true;
}

static int column_of(const ctui_cal_view *v, long day)
{
	long c = day - ctui_cal_view_first_day(v);
	return c >= 0 && c < ctui_cal_view_columns(v) ? (int)c : -1;
}

bool ctui_cal_view_event_rect(const ctui_cal_view *v, int width, int height,
			      int index, int *y, int *x, int *h, int *w)
{
	if (index < 0 || index >= v->event_count)
		return false;
	int col = -1, s = 0, t = 0;
	for (int c = 0; c < ctui_cal_view_columns(v) && col < 0; c++)
		if (seg_of(v, index, ctui_cal_view_day_of(v, c), &s, &t, NULL))
			col = c;
	if (col < 0)
		return false;
	int *buf = malloc(sizeof(int) * 2 * (size_t)v->event_count);
	if (!buf)
		return false;
	ctui_cal_layout_span(v->events, v->recurs, v->spans, v->event_count,
			     ctui_cal_view_day_of(v, col), buf,
			     buf + v->event_count);
	int ry, rx, rh, rw;
	bool ok = rect_of(v, width, height, s, t, col, buf[index],
			  buf[v->event_count + index], &ry, &rx, &rh, &rw);
	free(buf);
	if (ok) {
		if (y)
			*y = ry;
		if (x)
			*x = rx;
		if (h)
			*h = rh;
		if (w)
			*w = rw;
	}
	return ok;
}

/* ---- hit testing ---- */

ctui_cal_view_part ctui_cal_view_hit(const ctui_cal_view *v, int width,
				     int height, int y, int x, long *day,
				     int *minute)
{
	int cw, inner;
	geometry(v, width, &cw, &inner);
	if (y < 0 || y >= height || x < 0 || x >= width || cw == 0)
		return CTUI_CAL_VIEW_OUTSIDE;
	int slot = slot_of(v), m = 0;
	if (y > 0) {
		m = v->top + (y - 1) * slot;
		if (m >= DAY_MINUTES)
			return CTUI_CAL_VIEW_OUTSIDE;
	}
	if (x < AXIS) {
		if (y == 0)
			return CTUI_CAL_VIEW_OUTSIDE;
		if (minute)
			*minute = m;
		return CTUI_CAL_VIEW_AXIS;
	}
	int col = (x - AXIS) / cw;
	if (col >= ctui_cal_view_columns(v))
		return CTUI_CAL_VIEW_OUTSIDE;
	if (day)
		*day = ctui_cal_view_day_of(v, col);
	if (y == 0)
		return CTUI_CAL_VIEW_HEADER;
	if (minute)
		*minute = m;
	return CTUI_CAL_VIEW_SLOT;
}

int ctui_cal_view_event_at(const ctui_cal_view *v, int width, int height, int y,
			   int x)
{
	long day = 0;
	if (v->event_count < 1 ||
	    ctui_cal_view_hit(v, width, height, y, x, &day, NULL) !=
		CTUI_CAL_VIEW_SLOT)
		return -1;
	int col = column_of(v, day);
	int *buf =
	    col < 0 ? NULL : malloc(sizeof(int) * 2 * (size_t)v->event_count);
	if (!buf)
		return -1;
	int *lanes = buf + v->event_count, found = -1;
	ctui_cal_layout_span(v->events, v->recurs, v->spans, v->event_count,
			     day, buf, lanes);
	for (int i = 0; i < v->event_count && found < 0; i++) {
		int ry, rx, rh, rw, s, t;
		if (buf[i] >= 0 && seg_of(v, i, day, &s, &t, NULL) &&
		    rect_of(v, width, height, s, t, col, buf[i], lanes[i], &ry,
			    &rx, &rh, &rw) &&
		    y >= ry && y < ry + rh && x >= rx && x < rx + rw)
			found = i;
	}
	free(buf);
	return found;
}

/* ---- editing helpers ---- */

ctui_cal_edge ctui_cal_edge_of(int row, int rows, bool top_cut, bool bottom_cut)
{
	if (row < 0 || row >= rows)
		return CTUI_CAL_EDGE_NONE;
	if (rows >= 3 && row == 0 && !top_cut)
		return CTUI_CAL_EDGE_TOP;
	if (rows >= 2 && row == rows - 1 && !bottom_cut)
		return CTUI_CAL_EDGE_BOTTOM;
	return CTUI_CAL_EDGE_BODY;
}

ctui_cal_edge ctui_cal_view_event_part(const ctui_cal_view *v, int width,
				       int height, int y, int x, int *index,
				       long *day)
{
	long d = 0;
	int i = ctui_cal_view_event_at(v, width, height, y, x);
	if (i < 0 || ctui_cal_view_hit(v, width, height, y, x, &d, NULL) !=
			 CTUI_CAL_VIEW_SLOT)
		return CTUI_CAL_EDGE_NONE;
	int col = column_of(v, d);
	int *buf =
	    col < 0 ? NULL : malloc(sizeof(int) * 2 * (size_t)v->event_count);
	if (!buf)
		return CTUI_CAL_EDGE_NONE;
	ctui_cal_layout_span(v->events, v->recurs, v->spans, v->event_count, d,
			     buf, buf + v->event_count);
	int ry, rx, rh, rw, s = 0, t = 0, slot = slot_of(v);
	long occ = d;
	bool ok = seg_of(v, i, d, &s, &t, &occ) &&
		  rect_of(v, width, height, s, t, col, buf[i],
			  buf[v->event_count + i], &ry, &rx, &rh, &rw);
	free(buf);
	if (!ok)
		return CTUI_CAL_EDGE_NONE;
	/* the edges of a several-day event are on its first and last day only
	 */
	bool top_cut = fdiv(s - v->top, slot) < 0 || d > occ;
	bool bottom_cut =
	    fdiv(t - v->top + slot - 1, slot) > ctui_cal_view_rows(height) ||
	    d < occ + ctui_cal_span_extra(span_of(v, i));
	if (index)
		*index = i;
	if (day)
		*day = d;
	return ctui_cal_edge_of(y - ry, rh, top_cut, bottom_cut);
}

void ctui_cal_move_clamp(int duration, long day, int start, int slot,
			 long first, long last, long *out_day, int *out_start)
{
	if (slot < 1)
		slot = 30;
	if (duration < 1)
		duration = 1;
	if (duration > DAY_MINUTES)
		duration = DAY_MINUTES;
	int s = fdiv(start, slot) * slot;
	if (s > DAY_MINUTES - duration)
		s = DAY_MINUTES - duration;
	if (s < 0)
		s = 0;
	if (day < first)
		day = first;
	if (day > last)
		day = last;
	if (out_day)
		*out_day = day;
	if (out_start)
		*out_start = s;
}

bool ctui_cal_event_resize(ctui_cal_event *e, bool top, int minute, int slot)
{
	int s, t;
	if (slot < 1)
		slot = 30;
	if (!span(e, &s, &t))
		return false;
	int m = fdiv(minute, slot) * slot;
	int ns = s, nt = t;
	if (top) {
		int hi = t - slot < 0 ? 0 : t - slot;
		ns = m < 0 ? 0 : m > hi ? hi : m;
	} else {
		int lo = s + slot > DAY_MINUTES ? DAY_MINUTES : s + slot;
		nt = m < lo ? lo : m > DAY_MINUTES ? DAY_MINUTES : m;
	}
	bool changed = ns != e->start_minute || nt != e->end_minute;
	e->start_minute = ns;
	e->end_minute = nt;
	return changed;
}

bool ctui_cal_range(int a, int b, int slot, int *start, int *end)
{
	if (slot < 1)
		slot = 30;
	if (a > b) {
		int t = a;
		a = b;
		b = t;
	}
	if (a < 0 || b >= DAY_MINUTES)
		return false;
	*start = a / slot * slot;
	*end = b / slot * slot + slot;
	if (*end > DAY_MINUTES)
		*end = DAY_MINUTES;
	return true;
}

/* ---- moves ---- */

ctui_cal_view_event ctui_cal_view_goto(ctui_cal_view *v, int year, int month,
				       int day, int minute)
{
	if (!valid_date(year, month, day))
		return CTUI_CAL_VIEW_NONE;
	int slot = slot_of(v);
	int m = minute < 0 ? v->minute : minute / slot * slot;
	if (m > last_slot_start(slot))
		m = last_slot_start(slot);
	bool date = year != v->date.year || month != v->date.month ||
		    day != v->date.day;
	bool moved = date || m != v->minute;
	v->date = (ctui_date){year, month, day};
	v->minute = m;
	v->selected = -1;
	return date    ? CTUI_CAL_VIEW_DATE
	       : moved ? CTUI_CAL_VIEW_MOVED
		       : CTUI_CAL_VIEW_NONE;
}

static ctui_cal_view_event step_days(ctui_cal_view *v, long delta)
{
	ctui_date d = ctui_date_from_days(
	    ctui_date_to_days(v->date.year, v->date.month, v->date.day) +
	    delta);
	return ctui_cal_view_goto(v, d.year, d.month, d.day, -1);
}

static ctui_cal_view_event step_slots(ctui_cal_view *v, int delta)
{
	int slot = slot_of(v);
	int m = v->minute / slot * slot + delta * slot;
	if (m < 0 || m > last_slot_start(slot))
		return CTUI_CAL_VIEW_NONE;
	return ctui_cal_view_goto(v, v->date.year, v->date.month, v->date.day,
				  m);
}

/* ---- editing ---- */

void ctui_cal_view_set_editable(ctui_cal_view *v, ctui_cal_event *events,
				ctui_cal_recur *recurs, int count, int capacity)
{
	v->edit = (ctui_cal_edit){0};
	v->scope_pending = false;
	v->events = events;
	v->recurs = recurs;
	v->edit_events = events;
	v->edit_recurs = recurs;
	v->event_count = events && count > 0 ? count : 0;
	v->event_capacity =
	    events && capacity > v->event_count ? capacity : v->event_count;
	v->selected = -1;
}

void ctui_cal_view_set_editable_spans(ctui_cal_view *v, ctui_cal_span *spans)
{
	v->spans = spans;
	v->edit_spans = spans;
}

void ctui_cal_view_set_colors(ctui_cal_view *v, const ctui_cal_color *colors)
{
	v->colors = colors;
}

void ctui_cal_view_set_editable_colors(ctui_cal_view *v, ctui_cal_color *colors)
{
	v->colors = colors;
	v->edit_colors = colors;
}

static bool editable(const ctui_cal_view *v)
{
	return v->edit_events && v->events == v->edit_events;
}

static long cursor_day(const ctui_cal_view *v)
{
	return ctui_date_to_days(v->date.year, v->date.month, v->date.day);
}

static bool is_series(const ctui_cal_view *v, int i)
{
	return v->edit_recurs &&
	       v->edit_recurs[i].repeat != CTUI_CAL_REPEAT_NONE;
}

bool ctui_cal_view_set_color(ctui_cal_view *v, int index, int color)
{
	if (!v->edit_colors || index < 0 || index >= v->event_count ||
	    !ctui_cal_color_valid(color))
		return false;
	v->edit_colors[index].index = color;
	return true;
}

int ctui_cal_view_event_under_cursor(const ctui_cal_view *v)
{
	int slot = slot_of(v), found = -1;
	long day = cursor_day(v);
	for (int i = 0; i < v->event_count; i++) {
		int s, t;
		if (!seg_of(v, i, day, &s, &t, NULL) || s >= v->minute + slot ||
		    t <= v->minute)
			continue;
		if (i == v->selected)
			return i;
		if (found < 0)
			found = i;
	}
	return found;
}

static ctui_cal_view_event report(ctui_cal_view *v, ctui_cal_view_event e,
				  int index, long day)
{
	v->changed = index;
	v->changed_day = day;
	return e;
}

static ctui_cal_view_event create_range(ctui_cal_view *v, long day, int start,
					int end)
{
	if (v->event_count >= v->event_capacity)
		return CTUI_CAL_VIEW_FULL;
	int i = v->event_count;
	v->edit_events[i] = (ctui_cal_event){day, start, end, NULL, 0};
	if (v->edit_recurs)
		v->edit_recurs[i] = (ctui_cal_recur){0};
	if (v->edit_spans)
		v->edit_spans[i] = (ctui_cal_span){0};
	if (v->edit_colors)
		v->edit_colors[i] = (ctui_cal_color){0};
	v->event_count++;
	v->selected = i;
	return report(v, CTUI_CAL_VIEW_CREATED, i, day);
}

ctui_cal_view_event ctui_cal_view_create_range(ctui_cal_view *v, long start,
					       long end,
					       const ctui_cal_recur *rule)
{
	ctui_cal_event e = {0};
	ctui_cal_span sp = {0};
	if (!editable(v) || v->edit.grab != CTUI_CAL_GRAB_NONE ||
	    !ctui_cal_event_set_range(&e, &sp, start, end) ||
	    (sp.extra_days > 0 && !v->edit_spans))
		return CTUI_CAL_VIEW_NONE;
	if (v->event_count >= v->event_capacity)
		return CTUI_CAL_VIEW_FULL;
	ctui_cal_view_event r =
	    create_range(v, e.day, e.start_minute, e.end_minute);
	int i = v->changed;
	if (v->edit_spans)
		v->edit_spans[i] = sp;
	if (rule && v->edit_recurs)
		v->edit_recurs[i] = *rule;
	return r;
}

ctui_cal_view_event ctui_cal_view_place(ctui_cal_view *v,
					const ctui_cal_place *plan,
					int start_minute, int end_minute)
{
	if (!editable(v) || v->edit.grab != CTUI_CAL_GRAB_NONE ||
	    start_minute < 0 || end_minute <= start_minute || end_minute > 1440)
		return CTUI_CAL_VIEW_NONE;
	long first = plan->first;
	switch (plan->kind) {
	case CTUI_CAL_PLACE_SINGLE:
		return ctui_cal_view_create_range(
		    v, first * 1440L + start_minute, first * 1440L + end_minute,
		    NULL);
	case CTUI_CAL_PLACE_SERIES:
		if (!v->edit_recurs)
			return CTUI_CAL_VIEW_NONE;
		return ctui_cal_view_create_range(
		    v, first * 1440L + start_minute, first * 1440L + end_minute,
		    &plan->rule);
	case CTUI_CAL_PLACE_SPAN:
		if (!v->edit_spans || plan->days < 2)
			return CTUI_CAL_VIEW_NONE;
		return ctui_cal_view_create_range(
		    v, first * 1440L + start_minute,
		    (first + plan->days - 1) * 1440L + end_minute, NULL);
	case CTUI_CAL_PLACE_EACH: {
		if (v->event_count + v->sel.count > v->event_capacity)
			return CTUI_CAL_VIEW_FULL;
		int firstn = v->event_count;
		for (int k = 0; k < v->sel.count; k++) {
			long d = v->sel.days[k];
			if (ctui_cal_view_create_range(
				v, d * 1440L + start_minute,
				d * 1440L + end_minute,
				NULL) != CTUI_CAL_VIEW_CREATED)
				break;
		}
		if (v->event_count == firstn)
			return CTUI_CAL_VIEW_NONE;
		v->selected = firstn;
		return report(v, CTUI_CAL_VIEW_CREATED, firstn,
			      v->edit_events[firstn].day);
	}
	default:
		return CTUI_CAL_VIEW_NONE;
	}
}

ctui_cal_view_event ctui_cal_view_create(ctui_cal_view *v, int height)
{
	if (!editable(v) || v->edit.grab != CTUI_CAL_GRAB_NONE)
		return CTUI_CAL_VIEW_NONE;
	int slot = slot_of(v), start = v->minute / slot * slot,
	    end = start + slot;
	ctui_cal_view_event e = create_range(
	    v, cursor_day(v), start, end > DAY_MINUTES ? DAY_MINUTES : end);
	ctui_cal_view_reveal(v, height);
	return e;
}

ctui_cal_view_event ctui_cal_view_delete(ctui_cal_view *v, bool series)
{
	if (!editable(v) || v->edit.grab != CTUI_CAL_GRAB_NONE)
		return CTUI_CAL_VIEW_NONE;
	int i = ctui_cal_view_event_under_cursor(v);
	long day = cursor_day(v);
	if (i < 0)
		return CTUI_CAL_VIEW_NONE;
	if (is_series(v, i) && !series) {
		ctui_cal_recur *r = &v->edit_recurs[i];
		ctui_cal_event_covers(&v->edit_events[i], r, span_of(v, i), day,
				      &day); /* the day it starts */
		if (r->exception_count < 0)
			r->exception_count = 0;
		if (r->exception_count >= CTUI_CAL_EXCEPTIONS)
			return CTUI_CAL_VIEW_FULL;
		r->exceptions[r->exception_count++] = day;
		return report(v, CTUI_CAL_VIEW_EXCEPTED, i, day);
	}
	for (int j = i; j + 1 < v->event_count; j++) {
		v->edit_events[j] = v->edit_events[j + 1];
		if (v->edit_recurs)
			v->edit_recurs[j] = v->edit_recurs[j + 1];
		if (v->edit_spans)
			v->edit_spans[j] = v->edit_spans[j + 1];
		if (v->edit_colors)
			v->edit_colors[j] = v->edit_colors[j + 1];
	}
	v->event_count--;
	v->selected = -1;
	return report(v, CTUI_CAL_VIEW_DELETED, i, day);
}

ctui_cal_view_event ctui_cal_view_grab(ctui_cal_view *v, bool resize)
{
	if (!editable(v) || v->edit.grab != CTUI_CAL_GRAB_NONE)
		return CTUI_CAL_VIEW_NONE;
	int i = ctui_cal_view_event_under_cursor(v);
	if (i < 0)
		return CTUI_CAL_VIEW_NONE;
	v->edit = (ctui_cal_edit){.grab = resize ? CTUI_CAL_GRAB_RESIZE
						 : CTUI_CAL_GRAB_MOVE,
				  .index = i,
				  .saved = v->edit_events[i],
				  .grab_day = cursor_day(v),
				  .cursor_date = v->date,
				  .cursor_minute = v->minute};
	if (v->edit_recurs)
		v->edit.saved_recur = v->edit_recurs[i];
	if (v->edit_spans)
		v->edit.saved_span = v->edit_spans[i];
	v->selected = i;
	return report(v, CTUI_CAL_VIEW_GRABBED, i, v->edit.grab_day);
}

/* Did the edit in progress change the grabbed event? */
static bool edit_changed(const ctui_cal_view *v)
{
	const ctui_cal_edit *ed = &v->edit;
	const ctui_cal_event *e = &v->edit_events[ed->index];
	return e->day != ed->saved.day ||
	       e->start_minute != ed->saved.start_minute ||
	       e->end_minute != ed->saved.end_minute ||
	       (v->edit_spans &&
		ctui_cal_span_extra(&v->edit_spans[ed->index]) !=
		    ctui_cal_span_extra(&ed->saved_span));
}

ctui_cal_view_event ctui_cal_view_drop(ctui_cal_view *v)
{
	if (v->edit.grab == CTUI_CAL_GRAB_NONE)
		return CTUI_CAL_VIEW_NONE;
	int i = v->edit.index;
	v->edit.mouse = false;
	if (v->scope_pending ||
	    (v->ask_scope && v->edit.grab != CTUI_CAL_GRAB_CREATE && i >= 0 &&
	     i < v->event_count && is_series(v, i) && edit_changed(v))) {
		v->scope_pending =
		    true; /* the edit stays until the caller chooses */
		return report(v, CTUI_CAL_VIEW_ASK_SCOPE, i, cursor_day(v));
	}
	v->edit.grab = CTUI_CAL_GRAB_NONE;
	return report(v, CTUI_CAL_VIEW_DROPPED, i, cursor_day(v));
}

ctui_cal_view_event ctui_cal_view_cancel(ctui_cal_view *v, int height)
{
	if (v->edit.grab == CTUI_CAL_GRAB_NONE)
		return CTUI_CAL_VIEW_NONE;
	int i = v->edit.index;
	if (v->edit.grab != CTUI_CAL_GRAB_CREATE && i >= 0 &&
	    i < v->event_count) {
		v->edit_events[i] = v->edit.saved;
		if (v->edit_recurs)
			v->edit_recurs[i] = v->edit.saved_recur;
		if (v->edit_spans)
			v->edit_spans[i] = v->edit.saved_span;
	}
	v->date = v->edit.cursor_date;
	v->minute = v->edit.cursor_minute;
	v->edit.grab = CTUI_CAL_GRAB_NONE;
	v->scope_pending = false;
	ctui_cal_view_reveal(v, height);
	return report(v, CTUI_CAL_VIEW_CANCELLED, i, cursor_day(v));
}

ctui_cal_detach_result ctui_cal_detach(ctui_cal_event *events,
				       ctui_cal_recur *recurs,
				       ctui_cal_span *spans, int *count,
				       int capacity, int index, long occ,
				       long start, long end, int *out_index)
{
	if (!events || !recurs || !count || index < 0 || index >= *count ||
	    recurs[index].repeat == CTUI_CAL_REPEAT_NONE ||
	    !ctui_cal_event_occurs(&events[index], &recurs[index], occ))
		return CTUI_CAL_DETACH_INVALID;
	ctui_cal_event moved = events[index];
	ctui_cal_span sp = {0};
	if (!ctui_cal_event_set_range(&moved, &sp, start, end) ||
	    (sp.extra_days > 0 && !spans))
		return CTUI_CAL_DETACH_INVALID;
	ctui_cal_recur *r = &recurs[index];
	if (r->exception_count < 0)
		r->exception_count = 0;
	if (*count >= capacity)
		return CTUI_CAL_DETACH_NO_ROOM;
	if (r->exception_count >= CTUI_CAL_EXCEPTIONS)
		return CTUI_CAL_DETACH_NO_EXCEPTIONS;
	int n = *count;
	events[n] = moved;
	recurs[n] = (ctui_cal_recur){0};
	if (spans)
		spans[n] = sp;
	r->exceptions[r->exception_count++] = occ;
	*count = n + 1;
	if (out_index)
		*out_index = n;
	return CTUI_CAL_DETACH_OK;
}

ctui_cal_view_event
ctui_cal_view_resolve_scope(ctui_cal_view *v, ctui_cal_scope scope, int height)
{
	if (!v->scope_pending || v->edit.grab == CTUI_CAL_GRAB_NONE)
		return CTUI_CAL_VIEW_NONE;
	ctui_cal_edit ed = v->edit;
	int i = ed.index;
	if (scope == CTUI_CAL_SCOPE_SERIES) {
		v->scope_pending = false;
		v->edit.grab = CTUI_CAL_GRAB_NONE;
		return report(v, CTUI_CAL_VIEW_DROPPED, i, cursor_day(v));
	}
	/* the occurrence as it was, and the event as it is now */
	const ctui_cal_span *cur_span =
	    v->edit_spans ? &v->edit_spans[i] : NULL;
	ctui_cal_event cur = v->edit_events[i];
	ctui_cal_span cur_sp = cur_span ? *cur_span : (ctui_cal_span){0};
	ctui_cal_recur cur_recur = v->edit_recurs[i];
	long occ = 0;
	if (!ctui_cal_event_covers(&ed.saved, &ed.saved_recur,
				   v->edit_spans ? &ed.saved_span : NULL,
				   ed.grab_day, &occ))
		occ = ed.saved.day;
	long start =
	    (occ + (cur.day - ed.saved.day)) * 1440L + cur.start_minute;
	long length = ctui_cal_event_end_abs(&cur, cur_span) -
		      ctui_cal_event_start_abs(&cur);
	v->edit_events[i] = ed.saved;
	v->edit_recurs[i] = ed.saved_recur;
	if (v->edit_spans)
		v->edit_spans[i] = ed.saved_span;
	int created = -1;
	ctui_cal_detach_result r = ctui_cal_detach(
	    v->edit_events, v->edit_recurs, v->edit_spans, &v->event_count,
	    v->event_capacity, i, occ, start, start + length, &created);
	if (r != CTUI_CAL_DETACH_OK) {
		v->edit_events[i] = cur; /* still pending, as it was */
		v->edit_recurs[i] = cur_recur;
		if (v->edit_spans)
			v->edit_spans[i] = cur_sp;
		return r == CTUI_CAL_DETACH_INVALID ? CTUI_CAL_VIEW_NONE
						    : CTUI_CAL_VIEW_FULL;
	}
	if (v->edit_colors)
		v->edit_colors[created] = v->edit_colors[i];
	v->scope_pending = false;
	v->edit.grab = CTUI_CAL_GRAB_NONE;
	v->selected = created;
	v->changed_series = i;
	ctui_cal_view_reveal(v, height);
	return report(v, CTUI_CAL_VIEW_DETACHED, created,
		      occ + (cur.day - ed.saved.day));
}

/* The series of the grabbed event, rebuilt from the saved one shifted by
 * `delta` days. */
static void shift_series(ctui_cal_view *v, long delta)
{
	ctui_cal_edit *ed = &v->edit;
	if (!v->edit_recurs)
		return;
	ctui_cal_recur *r = &v->edit_recurs[ed->index];
	*r = ed->saved_recur;
	if (r->end == CTUI_CAL_END_UNTIL)
		r->until += delta;
	for (int k = 0; k < r->exception_count && k < CTUI_CAL_EXCEPTIONS; k++)
		r->exceptions[k] += delta;
}

/* Put the grabbed occurrence on `day` at `start` minutes: the series is
 * rebuilt from the saved one shifted by the same days, so that a move can
 * be repeated and cancelled exactly. An event of several days moves as a
 * whole: `day` is the column grabbed, whatever day of the event it is. */
static ctui_cal_view_event move_grabbed(ctui_cal_view *v, long day, int start)
{
	ctui_cal_edit *ed = &v->edit;
	int s, t, slot = slot_of(v);
	long first = ctui_cal_view_first_day(v),
	     last = first + ctui_cal_view_columns(v) - 1;
	ctui_cal_event *e = &v->edit_events[ed->index];
	ctui_cal_event old = *e;
	ctui_cal_span old_span =
	    v->edit_spans ? v->edit_spans[ed->index] : (ctui_cal_span){0};
	if (ctui_cal_span_extra(&ed->saved_span) > 0) {
		long duration =
		    ctui_cal_event_end_abs(&ed->saved, &ed->saved_span) -
		    ctui_cal_event_start_abs(&ed->saved);
		day = day < first ? first : day > last ? last : day;
		long target = ed->saved.day + (day - ed->grab_day);
		ctui_cal_move_clamp_span(
		    duration, target, start, slot, ctui_date_to_days(1, 1, 1),
		    ctui_date_to_days(9999, 12, 31), &target, &start);
		*e = ed->saved;
		e->day = target;
		e->start_minute = start;
		ctui_cal_span *sp = &v->edit_spans[ed->index];
		// NOLINTNEXTLINE(clang-analyzer-core.NullDereference)
		*sp = ed->saved_span;
		if (!ctui_cal_event_set_range(e, sp, target * 1440L + start,
					      target * 1440L + start +
						  duration)) {
			*e = old;
			*sp = old_span;
			return CTUI_CAL_VIEW_NONE;
		}
		shift_series(v, target - ed->saved.day);
		v->date = ctui_date_from_days(day);
		v->minute = start / slot * slot;
		bool same = old.day == e->day &&
			    old.start_minute == e->start_minute &&
			    old.end_minute == e->end_minute &&
			    old_span.extra_days == sp->extra_days;
		return same ? CTUI_CAL_VIEW_NONE
			    : report(v, CTUI_CAL_VIEW_EVENT_MOVED, ed->index,
				     day);
	}
	if (!ctui_cal_event_segment(&ed->saved, NULL, ed->saved.day,
				    ed->saved.day, &s, &t))
		return CTUI_CAL_VIEW_NONE;
	ctui_cal_move_clamp(t - s, day, start, slot, first, last, &day, &start);
	long delta = day - ed->grab_day;
	*e = ed->saved;
	e->day += delta;
	e->start_minute = start;
	e->end_minute = start + (t - s);
	shift_series(v, delta);
	v->date = ctui_date_from_days(day);
	v->minute = start / slot * slot;
	bool same = old.day == e->day && old.start_minute == e->start_minute &&
		    old.end_minute == e->end_minute;
	return same ? CTUI_CAL_VIEW_NONE
		    : report(v, CTUI_CAL_VIEW_EVENT_MOVED, ed->index, day);
}

/* Move one edge of an event one slot (delta -1 or 1): the target is the
 * next slot boundary in that direction. */
static ctui_cal_view_event resize_step(ctui_cal_view *v, int index, bool top,
				       int delta)
{
	int slot = slot_of(v), s, t;
	ctui_cal_event *e = &v->edit_events[index];
	if (v->edit_spans && ctui_cal_span_extra(&v->edit_spans[index]) > 0) {
		/* an event of several days: the edge goes on across midnight */
		ctui_cal_span *sp = &v->edit_spans[index];
		long at = top ? ctui_cal_event_start_abs(e)
			      : ctui_cal_event_end_abs(e, sp);
		long day = at >= 0 ? at / 1440 : -((-at + 1439) / 1440);
		long m = at - day * 1440;
		long target = delta > 0 ? m / slot * slot + slot
					: -fdiv((int)-m, slot) * slot - slot;
		if (!ctui_cal_event_resize_range(e, sp, top,
						 day * 1440 + target, slot,
						 is_series(v, index)))
			return CTUI_CAL_VIEW_NONE;
		int minute = top ? e->start_minute : e->end_minute - 1;
		ctui_date d = ctui_date_from_days(
		    top ? e->day : e->day + ctui_cal_span_extra(sp));
		v->date = d;
		v->minute = minute / slot * slot;
		return report(v, CTUI_CAL_VIEW_RESIZED, index,
			      ctui_date_to_days(d.year, d.month, d.day));
	}
	if (!span(e, &s, &t))
		return CTUI_CAL_VIEW_NONE;
	int at = top ? s : t;
	int target = delta > 0 ? fdiv(at, slot) * slot + slot
			       : -fdiv(-at, slot) * slot - slot;
	if (!ctui_cal_event_resize(e, top, target, slot))
		return CTUI_CAL_VIEW_NONE;
	span(e, &s, &t);
	v->minute = (top ? s : t - 1) / slot * slot;
	return report(v, CTUI_CAL_VIEW_RESIZED, index, cursor_day(v));
}

static ctui_cal_view_event grabbed_key(ctui_cal_view *v, int key, int height)
{
	ctui_cal_edit *ed = &v->edit;
	int slot = slot_of(v), dd = 0, ds = 0;
	if (key == 27)
		return ctui_cal_view_cancel(v, height);
	if (v->scope_pending) /* waiting for ctui_cal_view_resolve_scope():
				 Enter asks again */
		return key == '\n' || key == KEY_ENTER ? ctui_cal_view_drop(v)
						       : CTUI_CAL_VIEW_NONE;
	if (ed->mouse || ed->grab == CTUI_CAL_GRAB_CREATE)
		return CTUI_CAL_VIEW_NONE;
	if (key == '\n' || key == KEY_ENTER ||
	    key == (ed->grab == CTUI_CAL_GRAB_MOVE ? 'm' : 'r'))
		return ctui_cal_view_drop(v);
	if (key == KEY_LEFT || key == 'h')
		dd = -1;
	else if (key == KEY_RIGHT || key == 'l')
		dd = 1;
	else if (key == KEY_UP || key == 'k')
		ds = -1;
	else if (key == KEY_DOWN || key == 'j')
		ds = 1;
	else if (key != '\t')
		return CTUI_CAL_VIEW_NONE;
	ctui_cal_view_event e = CTUI_CAL_VIEW_NONE;
	if (ed->grab == CTUI_CAL_GRAB_MOVE) {
		if (key == '\t')
			return CTUI_CAL_VIEW_NONE;
		const ctui_cal_event *ev = &v->edit_events[ed->index];
		e = move_grabbed(v, cursor_day(v) + dd,
				 fdiv(ev->start_minute, slot) * slot +
				     ds * slot);
	} else if (ds) {
		e = resize_step(v, ed->index, ed->top, ds);
	} else {
		ed->top = !ed->top; /* the other edge */
		e = CTUI_CAL_VIEW_GRABBED;
	}
	ctui_cal_view_reveal(v, height);
	return e;
}

/* The day and slot under (y, x), clamped to the grid. */
static bool locate(const ctui_cal_view *v, int width, int height, int y, int x,
		   long *day, int *minute)
{
	int cw, inner, rows = ctui_cal_view_rows(height), slot = slot_of(v);
	geometry(v, width, &cw, &inner);
	if (cw == 0 || rows < 1)
		return false;
	int col = (x - AXIS) / cw;
	col = x < AXIS				? 0
	      : col >= ctui_cal_view_columns(v) ? ctui_cal_view_columns(v) - 1
						: col;
	int row = y - 1 < 0 ? 0 : y - 1 >= rows ? rows - 1 : y - 1;
	int m = v->top + row * slot;
	*day = ctui_cal_view_day_of(v, col);
	*minute = m > last_slot_start(slot) ? last_slot_start(slot) : m;
	return true;
}

/* Remember the event under a press so that a drag can edit it. */
static void arm_drag(ctui_cal_view *v, int width, int height, int y, int x)
{
	long day = 0;
	int minute = 0, index = -1;
	if (!editable(v) || v->edit.grab != CTUI_CAL_GRAB_NONE ||
	    ctui_cal_view_hit(v, width, height, y, x, &day, &minute) !=
		CTUI_CAL_VIEW_SLOT)
		return;
	ctui_cal_edge part =
	    ctui_cal_view_event_part(v, width, height, y, x, &index, &day);
	ctui_cal_edit ed = {.mouse = true,
			    .grab_day = day,
			    .cursor_date = v->date,
			    .cursor_minute = v->minute,
			    .from_day = day,
			    .from_minute = minute,
			    .to_minute = minute};
	if (part == CTUI_CAL_EDGE_NONE) {
		ed.grab = CTUI_CAL_GRAB_CREATE;
	} else {
		ed.grab = part == CTUI_CAL_EDGE_BODY ? CTUI_CAL_GRAB_MOVE
						     : CTUI_CAL_GRAB_RESIZE;
		ed.top = part == CTUI_CAL_EDGE_TOP;
		ed.index = index;
		ed.saved = v->edit_events[index];
		ed.grab_offset = minute - v->edit_events[index].start_minute;
		if (v->edit_recurs)
			ed.saved_recur = v->edit_recurs[index];
		if (v->edit_spans)
			ed.saved_span = v->edit_spans[index];
	}
	v->edit = ed;
	v->hover_edge = CTUI_CAL_EDGE_NONE;
}

ctui_cal_view_event ctui_cal_view_drag(ctui_cal_view *v, int width, int height,
				       int y, int x)
{
	ctui_cal_edit *ed = &v->edit;
	long day;
	int m, slot = slot_of(v);
	if (v->sel.dragging) {
		if (!locate(v, width, height, y, x, &day, &m))
			return CTUI_CAL_VIEW_NONE;
		bool changed = ctui_cal_select_drag_to(&v->sel, day);
		ctui_date d = ctui_date_from_days(day);
		ctui_cal_view_event moved =
		    ctui_cal_view_goto(v, d.year, d.month, d.day, -1);
		return changed || moved != CTUI_CAL_VIEW_NONE
			   ? CTUI_CAL_VIEW_SELECTED
			   : CTUI_CAL_VIEW_NONE;
	}
	if (!ed->mouse || !locate(v, width, height, y, x, &day, &m))
		return CTUI_CAL_VIEW_NONE;
	ctui_cal_view_event e = CTUI_CAL_VIEW_NONE;
	if (ed->grab == CTUI_CAL_GRAB_MOVE) {
		e = move_grabbed(v, day, m - ed->grab_offset);
	} else if (ed->grab == CTUI_CAL_GRAB_RESIZE) {
		ctui_cal_event *ev = &v->edit_events[ed->index];
		if (v->edit_spans &&
		    ctui_cal_span_extra(&v->edit_spans[ed->index]) > 0) {
			/* several days: the edge follows the column under the
			 * pointer */
			ctui_cal_span *sp = &v->edit_spans[ed->index];
			if (ctui_cal_event_resize_range(
				ev, sp, ed->top,
				day * 1440L + (ed->top ? m : m + slot), slot,
				is_series(v, ed->index)))
				e = report(v, CTUI_CAL_VIEW_RESIZED, ed->index,
					   ed->grab_day);
			int edge =
			    ed->top ? ev->start_minute : ev->end_minute - 1;
			v->date = ctui_date_from_days(
			    ed->top ? ev->day
				    : ev->day + ctui_cal_span_extra(sp));
			v->minute = edge / slot * slot;
		} else {
			if (ctui_cal_event_resize(ev, ed->top,
						  ed->top ? m : m + slot, slot))
				e = report(v, CTUI_CAL_VIEW_RESIZED, ed->index,
					   ed->grab_day);
			int s, t;
			if (span(ev, &s, &t))
				v->minute = (ed->top ? s : t - 1) / slot * slot;
		}
	} else if (ed->grab == CTUI_CAL_GRAB_CREATE && m != ed->to_minute) {
		ed->to_minute = m;
		v->minute = m;
		e = CTUI_CAL_VIEW_MOVED;
	}
	if (e != CTUI_CAL_VIEW_NONE)
		ed->moved = true;
	ctui_cal_view_reveal(v, height);
	return e;
}

ctui_cal_view_event ctui_cal_view_release(ctui_cal_view *v, int width,
					  int height, int y, int x)
{
	if (v->sel.dragging) {
		ctui_cal_view_event e =
		    ctui_cal_view_drag(v, width, height, y, x);
		ctui_cal_select_end(&v->sel);
		ctui_cal_view_reveal(v, height);
		return e;
	}
	if (!v->edit.mouse || v->edit.grab == CTUI_CAL_GRAB_NONE) {
		v->edit.mouse = false;
		return CTUI_CAL_VIEW_NONE;
	}
	ctui_cal_view_drag(v, width, height, y, x);
	ctui_cal_edit ed = v->edit;
	if (ed.grab != CTUI_CAL_GRAB_CREATE) {
		if (ed.moved)
			return ctui_cal_view_drop(
			    v); /* may ask for the scope */
		v->edit.grab = CTUI_CAL_GRAB_NONE;
		v->edit.mouse = false;
		return CTUI_CAL_VIEW_NONE;
	}
	v->edit.grab = CTUI_CAL_GRAB_NONE;
	v->edit.mouse = false;
	int s, t;
	if (ed.from_minute == ed.to_minute ||
	    !ctui_cal_range(ed.from_minute, ed.to_minute, slot_of(v), &s, &t))
		return CTUI_CAL_VIEW_NONE;
	ctui_cal_view_event e = create_range(v, ed.from_day, s, t);
	ctui_cal_view_reveal(v, height);
	return e;
}

ctui_cal_view_event ctui_cal_view_hover(ctui_cal_view *v, int width, int height,
					int y, int x)
{
	int index = -1;
	ctui_cal_edge part =
	    editable(v) && v->edit.grab == CTUI_CAL_GRAB_NONE
		? ctui_cal_view_event_part(v, width, height, y, x, &index, NULL)
		: CTUI_CAL_EDGE_NONE;
	if (part != CTUI_CAL_EDGE_TOP && part != CTUI_CAL_EDGE_BOTTOM)
		part = CTUI_CAL_EDGE_NONE;
	if (part == v->hover_edge &&
	    (part == CTUI_CAL_EDGE_NONE || index == v->hover))
		return CTUI_CAL_VIEW_NONE;
	v->hover_edge = part;
	v->hover = part == CTUI_CAL_EDGE_NONE ? 0 : index;
	return CTUI_CAL_VIEW_HOVERED;
}

static ctui_cal_view_event edit_key(ctui_cal_view *v, int key, int height)
{
	int i;
	switch (key) {
	case 'a':
	case 'n':
		return ctui_cal_view_create(v, height);
	case 'x':
	case KEY_DC:
	case 'X':
		i = ctui_cal_view_event_under_cursor(v);
		if (i < 0)
			return CTUI_CAL_VIEW_NONE;
		v->selected = i;
		return report(v,
			      key == 'X' ? CTUI_CAL_VIEW_DELETE_SERIES_ASKED
					 : CTUI_CAL_VIEW_DELETE_ASKED,
			      i, cursor_day(v));
	case 'm':
	case 'r':
		return ctui_cal_view_grab(v, key == 'r');
	case '\n':
	case KEY_ENTER:
		return ctui_cal_view_grab(
		    v, false); /* NONE: the caller sees ACTIVATED */
	case KEY_SR:
	case KEY_SF:
		i = ctui_cal_view_event_under_cursor(v);
		return i < 0 ? CTUI_CAL_VIEW_NONE
			     : resize_step(v, i, false, key == KEY_SR ? -1 : 1);
	default:
		return CTUI_CAL_VIEW_NONE;
	}
}

/* Shift+Left / Shift+Right: the selected days become the range from the
 * anchor to the new cursor day. */
static ctui_cal_view_event extend_days(ctui_cal_view *v, long delta)
{
	long from = cursor_day(v);
	ctui_cal_select next = v->sel;
	if (!ctui_cal_select_extend(&next, from, from + delta) ||
	    step_days(v, delta) == CTUI_CAL_VIEW_NONE)
		return CTUI_CAL_VIEW_NONE;
	v->sel = next;
	return CTUI_CAL_VIEW_SELECTED;
}

ctui_cal_view_event ctui_cal_view_key(ctui_cal_view *v, int key, int height)
{
	ctui_cal_view_event e;
	if (v->edit.grab != CTUI_CAL_GRAB_NONE)
		return grabbed_key(v, key, height);
	if (editable(v)) {
		e = edit_key(v, key, height);
		if (e != CTUI_CAL_VIEW_NONE)
			return e;
	}
	switch (key) {
	case '\n':
	case KEY_ENTER:
		return CTUI_CAL_VIEW_ACTIVATED;
	case KEY_SLEFT:
	case 'H':
	case KEY_SRIGHT:
	case 'L':
		e = extend_days(v, key == KEY_SLEFT || key == 'H' ? -1 : 1);
		if (e != CTUI_CAL_VIEW_NONE)
			ctui_cal_view_reveal(v, height);
		return e;
	case ' ':
		return ctui_cal_select_toggle(&v->sel, cursor_day(v))
			   ? CTUI_CAL_VIEW_SELECTED
			   : CTUI_CAL_VIEW_NONE;
	case 27:
		if (v->sel.count == 0)
			return CTUI_CAL_VIEW_NONE;
		ctui_cal_select_clear(&v->sel);
		return CTUI_CAL_VIEW_SELECTED;
	case KEY_LEFT:
	case 'h':
		e = step_days(v, -1);
		break;
	case KEY_RIGHT:
	case 'l':
		e = step_days(v, 1);
		break;
	case KEY_UP:
	case 'k':
		e = step_slots(v, -1);
		break;
	case KEY_DOWN:
	case 'j':
		e = step_slots(v, 1);
		break;
	case KEY_PPAGE:
		e = step_days(v, v->mode == CTUI_CAL_VIEW_DAY ? -1 : -7);
		break;
	case KEY_NPAGE:
		e = step_days(v, v->mode == CTUI_CAL_VIEW_DAY ? 1 : 7);
		break;
	case KEY_HOME:
		if (!v->today.year)
			return CTUI_CAL_VIEW_NONE;
		e = ctui_cal_view_goto(v, v->today.year, v->today.month,
				       v->today.day, v->now_minute);
		break;
	default:
		return CTUI_CAL_VIEW_NONE;
	}
	if (e != CTUI_CAL_VIEW_NONE) {
		ctui_cal_select_move(&v->sel);
		ctui_cal_view_reveal(v, height);
	}
	return e;
}

ctui_cal_view_event ctui_cal_view_press(ctui_cal_view *v, int width, int height,
					int y, int x)
{
	long day = 0;
	int minute = -1;
	ctui_cal_view_part part =
	    ctui_cal_view_hit(v, width, height, y, x, &day, &minute);
	if (part == CTUI_CAL_VIEW_OUTSIDE)
		return CTUI_CAL_VIEW_NONE;
	int pick = part == CTUI_CAL_VIEW_SLOT
		       ? ctui_cal_view_event_at(v, width, height, y, x)
		       : -1;
	ctui_cal_view_event e;
	bool cleared = false;
	if (part == CTUI_CAL_VIEW_HEADER || part == CTUI_CAL_VIEW_SLOT)
		cleared = ctui_cal_select_begin(&v->sel, day);
	if (part != CTUI_CAL_VIEW_HEADER)
		ctui_cal_select_end(
		    &v->sel); /* only a header starts a selection drag */
	if (part == CTUI_CAL_VIEW_AXIS) {
		e = ctui_cal_view_goto(v, v->date.year, v->date.month,
				       v->date.day, minute);
	} else {
		ctui_date d = ctui_date_from_days(day);
		e = ctui_cal_view_goto(v, d.year, d.month, d.day,
				       part == CTUI_CAL_VIEW_HEADER ? -1
								    : minute);
	}
	if (pick >= 0) {
		v->selected = pick;
		e = CTUI_CAL_VIEW_PICKED;
	}
	if (e == CTUI_CAL_VIEW_NONE && cleared)
		e = CTUI_CAL_VIEW_SELECTED;
	if (e != CTUI_CAL_VIEW_NONE)
		ctui_cal_view_reveal(v, height);
	arm_drag(v, width, height, y, x);
	return e;
}

bool ctui_cal_view_dragging(const ctui_cal_view *v)
{
	return v->edit.mouse || v->sel.dragging;
}

ctui_cal_view_event ctui_cal_view_toggle_at(ctui_cal_view *v, int width,
					    int height, int y, int x)
{
	long day = 0;
	ctui_cal_view_part part =
	    ctui_cal_view_hit(v, width, height, y, x, &day, NULL);
	if (part != CTUI_CAL_VIEW_HEADER && part != CTUI_CAL_VIEW_SLOT)
		return CTUI_CAL_VIEW_NONE;
	ctui_cal_select_toggle(&v->sel, day);
	ctui_date d = ctui_date_from_days(day);
	ctui_cal_view_goto(v, d.year, d.month, d.day, -1);
	ctui_cal_view_reveal(v, height);
	return CTUI_CAL_VIEW_SELECTED;
}

ctui_cal_view_event ctui_cal_view_wheel(ctui_cal_view *v, int height, int rows)
{
	int slot = slot_of(v), old = v->top;
	long top = (long)v->top + (long)rows * slot;
	int hi = max_top(ctui_cal_view_rows(height), slot);
	v->top = top < 0 ? 0 : top > hi ? hi : (int)top;
	return v->top != old ? CTUI_CAL_VIEW_SCROLLED : CTUI_CAL_VIEW_NONE;
}

/* ---- drawing ---- */

static void draw_header(const ctui_cal_view *v, int y, int x, int inner,
			long day)
{
	ctui_date d = ctui_date_from_days(day);
	int wd = ctui_date_weekday(d.year, d.month, d.day);
	bool day_mode = v->mode == CTUI_CAL_VIEW_DAY;
	const char *name = wd < 0	      ? ""
			   : v->weekday_names ? v->weekday_names[wd]
			   : day_mode	      ? LONG_WEEKDAYS[wd]
					      : WEEKDAYS[wd];
	char buf[64];
	if (day_mode)
		snprintf(buf, sizeof buf, "%s %04d-%02d-%02d", name, d.year,
			 d.month, d.day);
	else
		snprintf(buf, sizeof buf, "%.3s %2d", name, d.day);
	bool is_today = v->today.year == d.year && v->today.month == d.month &&
			v->today.day == d.day;
	bool is_cursor =
	    day == ctui_date_to_days(v->date.year, v->date.month, v->date.day);
	int attr = is_cursor ? ctui_attr(CTUI_PAIR_SELECTION, true)
		   : ctui_cal_select_contains(&v->sel, day)
		       ? ctui_attr(CTUI_PAIR_CURSOR_SELECTED, true)
		   : is_today ? ctui_attr(CTUI_PAIR_CURSOR, true)
			      : ctui_attr(CTUI_PAIR_HEADING, true);
	ctui_fill(y, x, inner, attr);
	ctui_put(y, x, inner, buf, attr);
}

static void view_draw_impl(const ctui_cal_view *v, int y, int x, int width,
			   int height)
{
	int cw, inner;
	geometry(v, width, &cw, &inner);
	int rows = ctui_cal_view_rows(height), slot = slot_of(v),
	    n = ctui_cal_view_columns(v);
	int body = ctui_attr(CTUI_PAIR_BODY, false),
	    muted = ctui_attr(CTUI_PAIR_MUTED, false);
	if (rows < 1 || cw == 0)
		return;
	const ctui_glyphs *g = ctui_theme_glyphs();
	long first = ctui_cal_view_first_day(v);
	long cursor_day =
	    ctui_date_to_days(v->date.year, v->date.month, v->date.day);
	long today =
	    v->today.year
		? ctui_date_to_days(v->today.year, v->today.month, v->today.day)
		: 0;
	int cur = v->minute / slot * slot;

	ctui_fill(y, x, width, body);
	for (int r = 0; r < rows; r++)
		ctui_fill(y + 1 + r, x, width, body);
	for (int col = 0; col < n; col++)
		draw_header(v, y, x + AXIS + col * cw, inner, first + col);
	for (int r = 0; r < rows; r++) {
		int m = v->top + r * slot;
		if (m >= DAY_MINUTES)
			break;
		if (m % 60 < slot) {
			char buf[16];
			snprintf(buf, sizeof buf, "%02d:00", m / 60);
			ctui_put(y + 1 + r, x, AXIS - 1, buf, muted);
		}
		if (n > 1)
			for (int col = 0; col < n; col++)
				ctui_put(y + 1 + r, x + AXIS + col * cw + inner,
					 1, g->box_v, muted);
		if (m == cur && column_of(v, cursor_day) >= 0)
			ctui_fill(y + 1 + r,
				  x + AXIS + column_of(v, cursor_day) * cw,
				  inner, ctui_attr(CTUI_PAIR_SELECTION, false));
		if (v->now_minute >= 0 && v->today.year && v->now_minute >= m &&
		    v->now_minute < m + slot) {
			int col = column_of(v, today);
			if (col >= 0 && !(m == cur && today == cursor_day))
				ctui_fill(y + 1 + r, x + AXIS + col * cw, inner,
					  ctui_attr(CTUI_PAIR_ERROR, false));
		}
	}

	if (v->edit.grab == CTUI_CAL_GRAB_CREATE &&
	    v->edit.from_minute != v->edit.to_minute) {
		/* the range being dragged over */
		int a, b, col = column_of(v, v->edit.from_day);
		if (col >= 0 && ctui_cal_range(v->edit.from_minute,
					       v->edit.to_minute, slot, &a, &b))
			for (int r = 0; r < rows; r++) {
				int m = v->top + r * slot;
				if (m >= a && m < b)
					ctui_fill(y + 1 + r,
						  x + AXIS + col * cw, inner,
						  ctui_attr(CTUI_PAIR_SELECTION,
							    true));
			}
	}

	if (v->event_count < 1)
		return;
	int *buf = malloc(sizeof(int) * 2 * (size_t)v->event_count);
	if (!buf)
		return;
	int *lanes = buf + v->event_count;
	for (int col = 0; col < n; col++) {
		long day = first + col;
		ctui_cal_layout_span(v->events, v->recurs, v->spans,
				     v->event_count, day, buf, lanes);
		for (int i = 0; i < v->event_count; i++) {
			int ry, rx, rh, rw, s = 0, t = 0;
			long occ = day;
			if (buf[i] < 0 || !seg_of(v, i, day, &s, &t, &occ) ||
			    !rect_of(v, width, height, s, t, col, buf[i],
				     lanes[i], &ry, &rx, &rh, &rw))
				continue;
			const ctui_cal_event *e = &v->events[i];
			int extra = ctui_cal_span_extra(span_of(v, i));
			bool from_before = day > occ,
			     to_after = day < occ + extra;
			int color = v->colors ? v->colors[i].index : 0;
			int attr =
			    color ? ctui_cal_color_attr(color, i == v->selected)
				  : ctui_attr(e->pair ? (ctui_pair)e->pair
						      : CTUI_PAIR_DIALOG,
					      i == v->selected);
			if (i == v->selected)
				attr = (int)((unsigned)attr |
					     (unsigned)A_UNDERLINE);
			bool grabbed = v->edit.grab != CTUI_CAL_GRAB_NONE &&
				       v->edit.index == i;
			if (grabbed && v->edit.grab == CTUI_CAL_GRAB_MOVE)
				attr =
				    (int)((unsigned)attr | (unsigned)A_REVERSE |
					  (unsigned)A_BOLD);
			for (int r = 0; r < rh; r++)
				ctui_fill(y + ry + r, x + rx, rw, attr);
			bool starts_here = fdiv(s - v->top, slot) >= 0;
			bool hovered = v->hover_edge != CTUI_CAL_EDGE_NONE &&
				       v->hover == i &&
				       v->edit.grab == CTUI_CAL_GRAB_NONE;
			if ((grabbed && v->edit.grab == CTUI_CAL_GRAB_RESIZE) ||
			    hovered) {
				/* the active or hovered edge: a reversed row
				 * ending with a heavy bar */
				bool top =
				    hovered ? v->hover_edge == CTUI_CAL_EDGE_TOP
					    : v->edit.top;
				bool cut = top ? !starts_here || from_before
					       : fdiv(t - v->top + slot - 1,
						      slot) > rows ||
						     to_after;
				if (!cut) {
					int er = y + (top ? ry : ry + rh - 1);
					ctui_fill(er, x + rx, rw,
						  (int)((unsigned)attr |
							(unsigned)A_REVERSE |
							(unsigned)A_BOLD));
					ctui_put(er, x + rx + rw - 1, 1,
						 g->heavy,
						 (int)((unsigned)attr |
						       (unsigned)A_REVERSE |
						       (unsigned)A_BOLD));
				}
			}
			bool series =
			    recur_of(v, i) &&
			    recur_of(v, i)->repeat != CTUI_CAL_REPEAT_NONE;
			int mark = series && rw >= 4
				       ? 2
				       : 0; /* room for the recurrence marker */
			int lead = from_before && rw >= 3
				       ? 1
				       : 0; /* marks of the cut edges */
			int tail = to_after && rw >= 3 ? 1 : 0;
			ctui_put(y + ry, x + rx + lead, rw - mark - lead - tail,
				 starts_here || from_before
				     ? (e->title ? e->title : "")
				     : "...",
				 attr);
			if (mark)
				ctui_put(y + ry, x + rx + rw - 2 - tail, 1,
					 g->unicode ? "\xe2\x86\xbb" : "R",
					 attr);
			if (lead)
				ctui_put(y + ry, x + rx, 1,
					 g->unicode ? "\xe2\x97\x82" : "<",
					 attr);
			if (tail)
				ctui_put(y + ry, x + rx + rw - 1, 1,
					 g->unicode ? "\xe2\x96\xb8" : ">",
					 attr);
			if (rh > 1 && (starts_here || from_before) &&
			    rw >= 11) {
				char text[48] = "";
				if (extra == 0)
					snprintf(text, sizeof text,
						 "%02d:%02d-%02d:%02d", s / 60,
						 s % 60, t / 60, t % 60);
				else if (!from_before)
					snprintf(text, sizeof text,
						 "%02d:%02d-", s / 60, s % 60);
				else if (!to_after)
					snprintf(text, sizeof text,
						 "-%02d:%02d", t / 60, t % 60);
				ctui_put(y + ry + 1, x + rx, rw, text, attr);
			}
		}
	}
	free(buf);
}

void ctui_cal_view_draw(const ctui_cal_view *v, int y, int x, int width,
			int height)
{
	ctui_style_begin(v->style);
	view_draw_impl(v, y, x, width, height);
	ctui_style_end();
}
