/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * calendar_select.c - the set of selected days (calendar_select.h).
 */
#include <ctui/calendar_select.h>

#include <string.h>

#include <ctui/calendar.h>

static bool in_years(long day)
{
	return day >= ctui_date_to_days(1, 1, 1) &&
	       day <= ctui_date_to_days(9999, 12, 31);
}

void ctui_cal_select_clear(ctui_cal_select *s)
{
	*s = (ctui_cal_select){0};
}

/* Index of the first day >= `day`, in 0..count. */
static int lower_bound(const ctui_cal_select *s, long day)
{
	int lo = 0, hi = s->count;
	while (lo < hi) {
		int mid = (lo + hi) / 2;
		if (s->days[mid] < day)
			lo = mid + 1;
		else
			hi = mid;
	}
	return lo;
}

bool ctui_cal_select_contains(const ctui_cal_select *s, long day)
{
	int i = lower_bound(s, day);
	return i < s->count && s->days[i] == day;
}

bool ctui_cal_select_add(ctui_cal_select *s, long day)
{
	if (!in_years(day))
		return false;
	int i = lower_bound(s, day);
	if (i < s->count && s->days[i] == day)
		return true;
	if (s->count >= CTUI_CAL_SELECT_MAX)
		return false;
	memmove(&s->days[i + 1], &s->days[i],
		sizeof(long) * (size_t)(s->count - i));
	s->days[i] = day;
	s->count++;
	return true;
}

bool ctui_cal_select_toggle(ctui_cal_select *s, long day)
{
	if (!in_years(day))
		return false;
	int i = lower_bound(s, day);
	if (i < s->count && s->days[i] == day) {
		memmove(&s->days[i], &s->days[i + 1],
			sizeof(long) * (size_t)(s->count - i - 1));
		s->count--;
	} else if (!ctui_cal_select_add(s, day)) {
		return false;
	}
	s->anchor = day;
	s->has_anchor = true;
	return true;
}

bool ctui_cal_select_set_range(ctui_cal_select *s, long a, long b)
{
	long lo = a < b ? a : b, hi = a < b ? b : a;
	if (!in_years(lo) || !in_years(hi) || hi - lo + 1 > CTUI_CAL_SELECT_MAX)
		return false;
	s->count = 0;
	for (long d = lo; d <= hi; d++)
		s->days[s->count++] = d;
	s->anchor = a;
	s->has_anchor = true;
	return true;
}

bool ctui_cal_select_extend(ctui_cal_select *s, long from, long to)
{
	long anchor = s->has_anchor ? s->anchor : from;
	return ctui_cal_select_set_range(s, anchor, to);
}

void ctui_cal_select_move(ctui_cal_select *s)
{
	s->has_anchor = false;
}

bool ctui_cal_select_begin(ctui_cal_select *s, long day)
{
	bool changed = s->count > 0;
	ctui_cal_select_clear(s);
	if (!in_years(day))
		return changed;
	s->anchor = day;
	s->has_anchor = true;
	s->dragging = true;
	return changed;
}

bool ctui_cal_select_drag_to(ctui_cal_select *s, long day)
{
	if (!s->dragging)
		return false;
	if (day == s->anchor) {
		bool changed = s->count > 0;
		s->count = 0;
		return changed;
	}
	ctui_cal_select old = *s;
	if (!ctui_cal_select_set_range(s, s->anchor, day))
		return false;
	return s->count != old.count ||
	       memcmp(s->days, old.days, sizeof(long) * (size_t)s->count) != 0;
}

void ctui_cal_select_end(ctui_cal_select *s)
{
	s->dragging = false;
}

int ctui_cal_select_count(const ctui_cal_select *s)
{
	return s->count;
}

long ctui_cal_select_first(const ctui_cal_select *s)
{
	return s->count ? s->days[0] : 0;
}

long ctui_cal_select_last(const ctui_cal_select *s)
{
	return s->count ? s->days[s->count - 1] : 0;
}

long ctui_cal_select_at(const ctui_cal_select *s, int i)
{
	return i >= 0 && i < s->count ? s->days[i] : 0;
}

bool ctui_cal_select_contiguous(const ctui_cal_select *s)
{
	return s->count > 0 &&
	       s->days[s->count - 1] - s->days[0] == s->count - 1;
}
