/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * calendar_recur.c - recurrence rules of calendar events (calendar_view.h).
 */
#include <stdbool.h>

#include <ctui/calendar_view.h>

#define MAX_INTERVAL 10000

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

static int interval_of(const ctui_cal_recur *r)
{
	int n = r->interval;
	return n >= 1 && n <= MAX_INTERVAL ? n : 1;
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

static unsigned weekdays_of(const ctui_cal_event *e, const ctui_cal_recur *r)
{
	unsigned m = r->weekdays & 0x7Fu;
	return m ? m : 1U << (unsigned)wd_of(e->day);
}

static int popcount_bits(unsigned m)
{
	int n = 0;
	for (; m; m >>= 1U)
		n += (int)(m & 1U);
	return n;
}

/* Does the day match the rule, ignoring the end and the exceptions? */
static bool raw_match(const ctui_cal_event *e, const ctui_cal_recur *r,
		      long day)
{
	int n = interval_of(r);
	if (day < e->day || day < min_day() || day > max_day() ||
	    e->day < min_day() || e->day > max_day())
		return false;
	ctui_date a = ctui_date_from_days(e->day), d = ctui_date_from_days(day);
	switch (r->repeat) {
	case CTUI_CAL_REPEAT_DAILY:
		return (day - e->day) % n == 0;
	case CTUI_CAL_REPEAT_WEEKLY:
		return (week_of(day) - week_of(e->day)) % n == 0 &&
		       (weekdays_of(e, r) >> (unsigned)wd_of(day) & 1U);
	case CTUI_CAL_REPEAT_MONTHLY:
		return d.day == a.day &&
		       ((long)(d.year - a.year) * 12 + (d.month - a.month)) %
			       n ==
			   0;
	case CTUI_CAL_REPEAT_YEARLY:
		return d.month == a.month && d.day == a.day &&
		       (d.year - a.year) % n == 0;
	default:
		return false;
	}
}

/* Raw matches in anchor..day, for a day that matches. */
static long count_upto(const ctui_cal_event *e, const ctui_cal_recur *r,
		       long day)
{
	int n = interval_of(r);
	ctui_date a = ctui_date_from_days(e->day), d = ctui_date_from_days(day);
	long c = 0;
	switch (r->repeat) {
	case CTUI_CAL_REPEAT_DAILY:
		return (day - e->day) / n + 1;
	case CTUI_CAL_REPEAT_WEEKLY: {
		unsigned mask = weekdays_of(e, r);
		long w = week_of(day) - week_of(e->day);
		unsigned from_anchor =
		    mask & ~((1U << (unsigned)wd_of(e->day)) - 1U);
		unsigned to_day = mask & ((2U << (unsigned)wd_of(day)) - 1U);
		if (w == 0)
			return popcount_bits(from_anchor & to_day);
		c = popcount_bits(from_anchor) +
		    (w - 1) / n * popcount_bits(mask);
		return c + popcount_bits(to_day);
	}
	case CTUI_CAL_REPEAT_MONTHLY: {
		long months =
		    (long)(d.year - a.year) * 12 + (d.month - a.month);
		for (long m = 0; m <= months; m += n) {
			long t = (long)a.year * 12 + (a.month - 1) + m;
			if (ctui_date_days_in_month((int)(t / 12),
						    (int)(t % 12) + 1) >= a.day)
				c++;
		}
		return c;
	}
	case CTUI_CAL_REPEAT_YEARLY:
		for (long y = a.year; y <= d.year; y += n)
			if (ctui_date_days_in_month((int)y, a.month) >= a.day)
				c++;
		return c;
	default:
		return 1;
	}
}

static bool excepted(const ctui_cal_recur *r, long day)
{
	int n = r->exception_count;
	if (n > CTUI_CAL_EXCEPTIONS)
		n = CTUI_CAL_EXCEPTIONS;
	for (int i = 0; i < n; i++)
		if (r->exceptions[i] == day)
			return true;
	return false;
}

/* Does a raw match fall inside the end of the series? */
static bool within_end(const ctui_cal_event *e, const ctui_cal_recur *r,
		       long day)
{
	if (r->end == CTUI_CAL_END_UNTIL)
		return day <= r->until;
	if (r->end == CTUI_CAL_END_COUNT)
		return count_upto(e, r, day) <= r->count;
	return true;
}

static const ctui_cal_recur NO_RULE;

bool ctui_cal_event_occurs(const ctui_cal_event *e, const ctui_cal_recur *r,
			   long day)
{
	if (!r)
		r = &NO_RULE;
	if (r->repeat == CTUI_CAL_REPEAT_NONE)
		return day == e->day;
	return raw_match(e, r, day) && within_end(e, r, day) &&
	       !excepted(r, day);
}

/* The first raw match >= from, or false. */
static bool raw_next(const ctui_cal_event *e, const ctui_cal_recur *r,
		     long from, long *out)
{
	int n = interval_of(r);
	if (from < e->day)
		from = e->day;
	if (from < min_day())
		from = min_day();
	if (from > max_day() || e->day < min_day() || e->day > max_day())
		return false;
	ctui_date a = ctui_date_from_days(e->day),
		  f = ctui_date_from_days(from);
	switch (r->repeat) {
	case CTUI_CAL_REPEAT_DAILY: {
		long k = (from - e->day + n - 1) / n;
		*out = e->day + k * n;
		return *out <= max_day();
	}
	case CTUI_CAL_REPEAT_WEEKLY:
		for (long d = from; d <= max_day() && d <= from + 7L * n + 7;
		     d++)
			if (raw_match(e, r, d)) {
				*out = d;
				return true;
			}
		return false;
	case CTUI_CAL_REPEAT_MONTHLY: {
		long months =
		    (long)(f.year - a.year) * 12 + (f.month - a.month);
		for (long m = (months + n - 1) / n * n;; m += n) {
			long t = (long)a.year * 12 + (a.month - 1) + m;
			if (t / 12 > 9999)
				return false;
			int y = (int)(t / 12), mo = (int)(t % 12) + 1;
			if (ctui_date_days_in_month(y, mo) < a.day)
				continue;
			long d = ctui_date_to_days(y, mo, a.day);
			if (d >= from) {
				*out = d;
				return d <= max_day();
			}
		}
	}
	case CTUI_CAL_REPEAT_YEARLY:
		for (long y =
			 a.year + ((long)(f.year - a.year) + n - 1) / n * n;
		     y <= 9999; y += n) {
			if (ctui_date_days_in_month((int)y, a.month) < a.day)
				continue;
			long d = ctui_date_to_days((int)y, a.month, a.day);
			if (d >= from) {
				*out = d;
				return true;
			}
		}
		return false;
	default:
		return false;
	}
}

bool ctui_cal_event_next(const ctui_cal_event *e, const ctui_cal_recur *r,
			 long after, long *next)
{
	if (!r)
		r = &NO_RULE;
	if (r->repeat == CTUI_CAL_REPEAT_NONE) {
		if (e->day <= after)
			return false;
		*next = e->day;
		return true;
	}
	long from = after + 1;
	for (int guard = 0; guard <= CTUI_CAL_EXCEPTIONS + 1; guard++) {
		long d;
		if (!raw_next(e, r, from, &d) || !within_end(e, r, d))
			return false;
		if (!excepted(r, d)) {
			*next = d;
			return true;
		}
		from = d + 1;
	}
	return false;
}

int ctui_cal_event_expand(const ctui_cal_event *e, const ctui_cal_recur *r,
			  long first, long last, long *days, int max)
{
	int found = 0;
	long after = first - 1, d;
	while (after < last && ctui_cal_event_next(e, r, after, &d) &&
	       d <= last) {
		if (days && found < max)
			days[found] = d;
		found++;
		after = d;
	}
	return found;
}
