/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * calendar_plan.c - classify a selection of days and build the recurrence
 * rule or the span of an event placed on it (calendar_view.h).
 */
#include <stdbool.h>
#include <stddef.h>

#include <ctui/calendar_view.h>

static long fdiv_l(long a, long b)
{
	long q = a / b;
	return a % b < 0 ? q - 1 : q;
}

/* Monday-based week number and weekday (1970-01-01 was a Thursday). */
static long week_of(long day)
{
	return fdiv_l(day + 3, 7);
}

static int wd_of(long day)
{
	return (int)(day + 3 - 7 * week_of(day));
}

static long gcd_l(long a, long b)
{
	while (b) {
		long t = a % b;
		a = b;
		b = t;
	}
	return a;
}

/* The weekly rule whose first occurrences, from the first selected day to
 * the last, are exactly the selected days; false when there is none. */
static bool weekly_rule(const ctui_cal_select *s, ctui_cal_recur *rule)
{
	long first = s->days[0];
	long step = 0;
	unsigned mask = 0;
	for (int i = 0; i < s->count; i++) {
		mask |= 1U << (unsigned)wd_of(s->days[i]);
		step = gcd_l(step, week_of(s->days[i]) - week_of(first));
	}
	ctui_cal_recur r = {.repeat = CTUI_CAL_REPEAT_WEEKLY,
			    .interval =
				step >= 1 && step <= 10000 ? (int)step : 1,
			    .weekdays = mask};
	if (step > 10000)
		return false;
	/* walk the occurrences of the rule and compare them with the selection
	 */
	ctui_cal_event anchor = {first, 0, 60, NULL, 0};
	long d = first - 1;
	for (int i = 0; i < s->count; i++) {
		if (!ctui_cal_event_next(&anchor, &r, d, &d) || d != s->days[i])
			return false;
	}
	*rule = r;
	return true;
}

ctui_cal_sel_kind ctui_cal_select_classify(const ctui_cal_select *s)
{
	ctui_cal_recur rule;
	if (s->count == 0)
		return CTUI_CAL_SEL_NONE;
	if (s->count == 1)
		return CTUI_CAL_SEL_SINGLE;
	if (ctui_cal_select_contiguous(s))
		return CTUI_CAL_SEL_RANGE;
	return weekly_rule(s, &rule) ? CTUI_CAL_SEL_WEEKLY
				     : CTUI_CAL_SEL_ARBITRARY;
}

ctui_cal_place ctui_cal_plan_repeat(const ctui_cal_select *s)
{
	ctui_cal_place p = {.kind = CTUI_CAL_PLACE_NONE,
			    .first = ctui_cal_select_first(s),
			    .days = s->count};
	if (s->count == 0)
		return p;
	p.events = 1;
	if (s->count == 1) {
		p.kind = CTUI_CAL_PLACE_SINGLE;
	} else if (weekly_rule(s, &p.rule)) {
		p.kind = CTUI_CAL_PLACE_SERIES;
	} else {
		p.kind = CTUI_CAL_PLACE_EACH;
		p.events = s->count;
	}
	return p;
}

ctui_cal_place ctui_cal_plan_each(const ctui_cal_select *s)
{
	ctui_cal_place p = {.kind = CTUI_CAL_PLACE_NONE,
			    .first = ctui_cal_select_first(s),
			    .days = s->count};
	if (s->count == 0)
		return p;
	p.events = s->count;
	p.kind = s->count == 1 ? CTUI_CAL_PLACE_SINGLE : CTUI_CAL_PLACE_EACH;
	if (s->count == 1)
		p.events = 1;
	return p;
}

ctui_cal_place ctui_cal_plan_span(const ctui_cal_select *s)
{
	ctui_cal_place p = {.kind = CTUI_CAL_PLACE_NONE,
			    .first = ctui_cal_select_first(s),
			    .days = s->count};
	if (s->count == 0 || !ctui_cal_select_contiguous(s))
		return p;
	p.events = 1;
	p.kind = s->count == 1 ? CTUI_CAL_PLACE_SINGLE : CTUI_CAL_PLACE_SPAN;
	return p;
}
