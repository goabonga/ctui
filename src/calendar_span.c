/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * calendar_span.c - events that last several days (calendar_view.h).
 */
#include <stdbool.h>
#include <stddef.h>

#include <ctui/calendar_view.h>

#define DAY_MINUTES 1440

static long fdiv_l(long a, long b)
{
	long q = a / b;
	return a % b < 0 ? q - 1 : q;
}

static long min_day(void)
{
	return ctui_date_to_days(1, 1, 1);
}

static long max_day(void)
{
	return ctui_date_to_days(9999, 12, 31);
}

int ctui_cal_span_extra(const ctui_cal_span *sp)
{
	if (!sp || sp->extra_days < 1)
		return 0;
	return sp->extra_days > CTUI_CAL_SPAN_MAX ? CTUI_CAL_SPAN_MAX
						  : sp->extra_days;
}

long ctui_cal_event_start_abs(const ctui_cal_event *e)
{
	return e->day * DAY_MINUTES + e->start_minute;
}

long ctui_cal_event_end_abs(const ctui_cal_event *e, const ctui_cal_span *sp)
{
	return (e->day + ctui_cal_span_extra(sp)) * DAY_MINUTES + e->end_minute;
}

bool ctui_cal_event_set_range(ctui_cal_event *e, ctui_cal_span *sp, long start,
			      long end)
{
	if (end <= start)
		return false;
	long first = fdiv_l(start, DAY_MINUTES),
	     last = fdiv_l(end - 1, DAY_MINUTES);
	if (first < min_day() || last > max_day() ||
	    last - first > CTUI_CAL_SPAN_MAX)
		return false;
	e->day = first;
	e->start_minute = (int)(start - first * DAY_MINUTES);
	e->end_minute = (int)(end - last * DAY_MINUTES);
	if (sp)
		sp->extra_days = (int)(last - first);
	return true;
}

bool ctui_cal_event_segment(const ctui_cal_event *e, const ctui_cal_span *sp,
			    long occ, long day, int *start, int *end)
{
	int extra = ctui_cal_span_extra(sp), a = e->start_minute,
	    b = e->end_minute;
	if (day < occ || day > occ + extra)
		return false;
	if (extra == 0) {
		if (a < 0)
			a = 0;
		if (a >= DAY_MINUTES)
			return false;
		if (b <= a)
			b = a + 1;
		if (b > DAY_MINUTES)
			b = DAY_MINUTES;
	} else {
		a = day == occ ? (a < 0			? 0
				  : a > DAY_MINUTES - 1 ? DAY_MINUTES - 1
							: a)
			       : 0;
		b = day == occ + extra ? (b < 1		    ? 1
					  : b > DAY_MINUTES ? DAY_MINUTES
							    : b)
				       : DAY_MINUTES;
	}
	if (start)
		*start = a;
	if (end)
		*end = b;
	return true;
}

bool ctui_cal_event_covers(const ctui_cal_event *e, const ctui_cal_recur *r,
			   const ctui_cal_span *sp, long day, long *occ)
{
	int extra = ctui_cal_span_extra(sp);
	if (extra == 0) {
		if (!ctui_cal_event_occurs(e, r, day))
			return false;
		if (occ)
			*occ = day;
		return true;
	}
	long after = day - extra - 1, o, latest = 0;
	bool found = false;
	while (ctui_cal_event_next(e, r, after, &o) && o <= day) {
		found = true;
		latest = o;
		after = o;
	}
	if (found && occ)
		*occ = latest;
	return found;
}

int ctui_cal_event_expand_span(const ctui_cal_event *e, const ctui_cal_recur *r,
			       const ctui_cal_span *sp, long first, long last,
			       long *days, int max)
{
	return ctui_cal_event_expand(e, r, first - ctui_cal_span_extra(sp),
				     last, days, max);
}

uint32_t ctui_cal_span_month_mask(const ctui_cal_event *events,
				  const ctui_cal_recur *recurs,
				  const ctui_cal_span *spans, int count,
				  int year, int month)
{
	uint32_t mask = 0;
	int dim = ctui_date_days_in_month(year, month);
	if (!spans || !events)
		return 0;
	for (int i = 0; i < count; i++) {
		if (ctui_cal_span_extra(&spans[i]) == 0)
			continue;
		for (int d = 1; d <= dim; d++)
			if (ctui_cal_event_covers(
				&events[i], recurs ? &recurs[i] : NULL,
				&spans[i], ctui_date_to_days(year, month, d),
				NULL))
				mask |= 1U << (unsigned)(d - 1);
	}
	return mask;
}

static long clamp_l(long v, long lo, long hi)
{
	return v < lo ? lo : v > hi ? hi : v;
}

bool ctui_cal_event_resize_range(ctui_cal_event *e, ctui_cal_span *sp, bool top,
				 long at, int slot, bool keep_day)
{
	if (slot < 1)
		slot = 30;
	long a = ctui_cal_event_start_abs(e), b = ctui_cal_event_end_abs(e, sp);
	long day = fdiv_l(at, DAY_MINUTES);
	long tod = fdiv_l(at - day * DAY_MINUTES, slot) * slot;
	long snapped =
	    (top && keep_day ? fdiv_l(a, DAY_MINUTES) : day) * DAY_MINUTES +
	    tod;
	long na = a, nb = b;
	if (top) {
		long hi = b - slot,
		     floor_b = fdiv_l(b - 1, DAY_MINUTES) * DAY_MINUTES;
		if (hi < floor_b)
			hi = floor_b;
		long lo = keep_day ? fdiv_l(a, DAY_MINUTES) * DAY_MINUTES
				   : min_day() * DAY_MINUTES;
		na = clamp_l(snapped, lo, hi);
	} else {
		long lo = a + slot,
		     ceil_a = (fdiv_l(a, DAY_MINUTES) + 1) * DAY_MINUTES;
		if (lo > ceil_a)
			lo = ceil_a;
		nb = clamp_l(snapped, lo, (max_day() + 1) * DAY_MINUTES);
	}
	ctui_cal_event copy = *e;
	ctui_cal_span sc = {ctui_cal_span_extra(sp)};
	if (!ctui_cal_event_set_range(&copy, &sc, na, nb))
		return false;
	bool changed = na != a || nb != b;
	e->day = copy.day;
	e->start_minute = copy.start_minute;
	e->end_minute = copy.end_minute;
	if (sp)
		sp->extra_days = sc.extra_days;
	return changed;
}

void ctui_cal_move_clamp_span(long duration, long day, int start, int slot,
			      long first, long last, long *out_day,
			      int *out_start)
{
	if (slot < 1)
		slot = 30;
	if (duration < 1)
		duration = 1;
	long s = fdiv_l(start, slot) * slot,
	     last_start = ((long)DAY_MINUTES - 1) / slot * slot;
	s = clamp_l(s, 0, last_start);
	day = clamp_l(day, first, last);
	/* the end stays in year 9999 */
	long latest = (max_day() + 1) * DAY_MINUTES - duration;
	if (day * DAY_MINUTES + s > latest) {
		day = fdiv_l(latest - s, DAY_MINUTES);
		if (day * DAY_MINUTES + s > latest)
			day--;
	}
	if (out_day)
		*out_day = day;
	if (out_start)
		*out_start = (int)s;
}
