/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */
#include "check.h"

#include <ctui/calendar_view.h>

static long day(int y, int m, int d)
{
	return ctui_date_to_days(y, m, d);
}

/* An event with a rule: the rule is held in the global g_r, as the tests
 * use one at a time. */
static ctui_cal_recur g_r;

static ctui_cal_event ev(long d, ctui_cal_repeat r, int interval)
{
	g_r = (ctui_cal_recur){.repeat = r, .interval = interval};
	return (ctui_cal_event){
	    .day = d, .start_minute = 540, .end_minute = 600, .title = "x"};
}

static void test_single(void)
{
	ctui_cal_event e = {day(2026, 11, 3), 540, 600, "x", 0};
	g_r = (ctui_cal_recur){0};
	long n;
	CHECK(ctui_cal_event_occurs(&e, &g_r, day(2026, 11, 3)));
	CHECK(!ctui_cal_event_occurs(&e, &g_r, day(2026, 11, 4)));
	CHECK(!ctui_cal_event_next(&e, &g_r, day(2026, 11, 3), &n));
	CHECK(ctui_cal_event_next(&e, &g_r, day(2026, 11, 2), &n) &&
	      n == day(2026, 11, 3));
	CHECK(ctui_cal_event_expand(&e, &g_r, day(2026, 11, 1),
				    day(2026, 11, 30), NULL, 0) == 1);
}

static void test_daily(void)
{
	ctui_cal_event e = ev(day(2026, 11, 3), CTUI_CAL_REPEAT_DAILY, 1);
	CHECK(!ctui_cal_event_occurs(&e, &g_r, day(2026, 11, 2)));
	CHECK(ctui_cal_event_occurs(&e, &g_r, day(2026, 11, 3)) &&
	      ctui_cal_event_occurs(&e, &g_r, day(2027, 1, 9)));
	g_r.interval = 3;
	CHECK(ctui_cal_event_occurs(&e, &g_r, day(2026, 11, 6)) &&
	      !ctui_cal_event_occurs(&e, &g_r, day(2026, 11, 7)));
	long days[8];
	CHECK(ctui_cal_event_expand(&e, &g_r, day(2026, 11, 1),
				    day(2026, 11, 13), days, 8) == 4);
	CHECK(days[0] == day(2026, 11, 3) && days[3] == day(2026, 11, 12));
	/* max limits what is written, not what is counted */
	long two[2] = {0, 0};
	CHECK(ctui_cal_event_expand(&e, &g_r, day(2026, 11, 1),
				    day(2026, 11, 13), two, 2) == 4);
	CHECK(two[1] == day(2026, 11, 6));
	g_r.interval = 0; /* invalid: every day */
	CHECK(ctui_cal_event_occurs(&e, &g_r, day(2026, 11, 4)));
	g_r.interval = 99999;
	CHECK(ctui_cal_event_occurs(&e, &g_r, day(2026, 11, 4)));
}

static void test_weekly(void)
{
	/* Tuesday 2026-11-03; no mask: every Tuesday */
	ctui_cal_event e = ev(day(2026, 11, 3), CTUI_CAL_REPEAT_WEEKLY, 1);
	CHECK(ctui_cal_event_occurs(&e, &g_r, day(2026, 11, 10)) &&
	      !ctui_cal_event_occurs(&e, &g_r, day(2026, 11, 11)));
	g_r.weekdays = 1U << 0U | 1U << 2U | 1U << 4U; /* Mon Wed Fri */
	long days[8];
	int n = ctui_cal_event_expand(&e, &g_r, day(2026, 11, 1),
				      day(2026, 11, 14), days, 8);
	/* the anchor Tuesday does not match the mask, and nothing before it
	 * occurs */
	CHECK(n == 5 && days[0] == day(2026, 11, 4) &&
	      days[1] == day(2026, 11, 6) && days[2] == day(2026, 11, 9) &&
	      days[4] == day(2026, 11, 13));
	g_r.interval = 2; /* every other week: weeks of 11-03 and 11-17 */
	n = ctui_cal_event_expand(&e, &g_r, day(2026, 11, 1), day(2026, 11, 30),
				  days, 8);
	CHECK(n == 6 && days[2] == day(2026, 11, 16) &&
	      days[3] == day(2026, 11, 18) && days[5] == day(2026, 11, 30));
	CHECK(!ctui_cal_event_occurs(&e, &g_r, day(2026, 11, 9)));
	/* a Sunday anchor: its week is the one that ends on it */
	ctui_cal_event s = ev(day(2026, 11, 1), CTUI_CAL_REPEAT_WEEKLY, 2);
	g_r.weekdays = 1U << 6U | 1U << 0U;
	CHECK(ctui_cal_event_occurs(&s, &g_r, day(2026, 11, 1)) &&
	      !ctui_cal_event_occurs(&s, &g_r, day(2026, 11, 2)));
	CHECK(!ctui_cal_event_occurs(&s, &g_r, day(2026, 11, 8)) &&
	      ctui_cal_event_occurs(&s, &g_r, day(2026, 11, 9)));
	CHECK(ctui_cal_event_occurs(&s, &g_r, day(2026, 11, 15)));
}

static void test_monthly(void)
{
	ctui_cal_event e = ev(day(2026, 1, 31), CTUI_CAL_REPEAT_MONTHLY, 1);
	CHECK(ctui_cal_event_occurs(&e, &g_r, day(2026, 3, 31)) &&
	      !ctui_cal_event_occurs(&e, &g_r, day(2026, 2, 28)));
	long n;
	CHECK(ctui_cal_event_next(&e, &g_r, day(2026, 3, 31), &n) &&
	      n == day(2026, 5, 31)); /* April has 30 */
	CHECK(ctui_cal_event_next(&e, &g_r, day(2026, 1, 31), &n) &&
	      n == day(2026, 3, 31));
	e = ev(day(2026, 11, 15), CTUI_CAL_REPEAT_MONTHLY, 3);
	CHECK(ctui_cal_event_occurs(&e, &g_r, day(2027, 2, 15)) &&
	      !ctui_cal_event_occurs(&e, &g_r, day(2026, 12, 15)));
	CHECK(ctui_cal_event_next(&e, &g_r, day(2026, 11, 15), &n) &&
	      n == day(2027, 2, 15));
	CHECK(ctui_cal_event_next(&e, &g_r, day(2026, 12, 1), &n) &&
	      n == day(2027, 2, 15));
	/* the 29th: leap Februaries only; interval 12 from a leap February */
	e = ev(day(2024, 2, 29), CTUI_CAL_REPEAT_MONTHLY, 12);
	CHECK(ctui_cal_event_next(&e, &g_r, day(2024, 2, 29), &n) &&
	      n == day(2028, 2, 29));
	CHECK(!ctui_cal_event_occurs(&e, &g_r, day(2025, 2, 28)));
}

static void test_yearly(void)
{
	ctui_cal_event e = ev(day(2026, 11, 3), CTUI_CAL_REPEAT_YEARLY, 1);
	CHECK(ctui_cal_event_occurs(&e, &g_r, day(2030, 11, 3)) &&
	      !ctui_cal_event_occurs(&e, &g_r, day(2030, 11, 4)));
	CHECK(!ctui_cal_event_occurs(&e, &g_r, day(2025, 11, 3)));
	g_r.interval = 2;
	CHECK(!ctui_cal_event_occurs(&e, &g_r, day(2027, 11, 3)) &&
	      ctui_cal_event_occurs(&e, &g_r, day(2028, 11, 3)));
	/* leap day */
	e = ev(day(2024, 2, 29), CTUI_CAL_REPEAT_YEARLY, 1);
	long n;
	CHECK(ctui_cal_event_next(&e, &g_r, day(2024, 2, 29), &n) &&
	      n == day(2028, 2, 29));
	CHECK(!ctui_cal_event_occurs(&e, &g_r, day(2025, 2, 28)) &&
	      !ctui_cal_event_occurs(&e, &g_r, day(2025, 3, 1)));
	CHECK(ctui_cal_event_next(&e, &g_r, day(2096, 2, 29), &n) &&
	      n == day(2104, 2, 29)); /* 2100 is not leap */
}

static void test_end(void)
{
	ctui_cal_event e = ev(day(2026, 11, 1), CTUI_CAL_REPEAT_DAILY, 1);
	g_r.end = CTUI_CAL_END_UNTIL;
	g_r.until = day(2026, 11, 5);
	CHECK(ctui_cal_event_occurs(&e, &g_r, day(2026, 11, 5)) &&
	      !ctui_cal_event_occurs(&e, &g_r, day(2026, 11, 6)));
	CHECK(ctui_cal_event_expand(&e, &g_r, day(2026, 1, 1), day(2027, 1, 1),
				    NULL, 0) == 5);
	long n;
	CHECK(!ctui_cal_event_next(&e, &g_r, day(2026, 11, 5), &n));

	g_r.end = CTUI_CAL_END_COUNT;
	g_r.count = 4;
	CHECK(ctui_cal_event_occurs(&e, &g_r, day(2026, 11, 4)) &&
	      !ctui_cal_event_occurs(&e, &g_r, day(2026, 11, 5)));
	g_r.repeat = CTUI_CAL_REPEAT_WEEKLY;
	g_r.weekdays =
	    1U << 6U | 1U << 0U | 1U << 2U; /* 2026-11-01 is a Sunday */
	/* Sun 1, Mon 2, Wed 4, Sun 8: the fourth is the 8th */
	CHECK(ctui_cal_event_occurs(&e, &g_r, day(2026, 11, 8)) &&
	      !ctui_cal_event_occurs(&e, &g_r, day(2026, 11, 9)));
	CHECK(ctui_cal_event_expand(&e, &g_r, day(2026, 1, 1), day(2030, 1, 1),
				    NULL, 0) == 4);
	g_r.interval = 2;
	g_r.count = 5; /* Sun 1; week of 11-09 is skipped.. no: 11-02 (Mon) is
			  in the next week */
	int total = ctui_cal_event_expand(&e, &g_r, day(2026, 1, 1),
					  day(2030, 1, 1), NULL, 0);
	CHECK(total == 5);
	g_r.repeat = CTUI_CAL_REPEAT_MONTHLY;
	g_r.interval = 1;
	g_r.count = 3;
	CHECK(ctui_cal_event_expand(&e, &g_r, day(2026, 1, 1), day(2030, 1, 1),
				    NULL, 0) == 3);
	CHECK(ctui_cal_event_occurs(&e, &g_r, day(2027, 1, 1)) &&
	      !ctui_cal_event_occurs(&e, &g_r, day(2027, 2, 1)));
	g_r.repeat = CTUI_CAL_REPEAT_YEARLY;
	CHECK(ctui_cal_event_expand(&e, &g_r, day(2026, 1, 1), day(2060, 1, 1),
				    NULL, 0) == 3);
}

static void test_exceptions(void)
{
	ctui_cal_event e = ev(day(2026, 11, 1), CTUI_CAL_REPEAT_DAILY, 1);
	g_r.exceptions[0] = day(2026, 11, 3);
	g_r.exceptions[1] = day(2026, 11, 4);
	g_r.exception_count = 2;
	long n;
	CHECK(!ctui_cal_event_occurs(&e, &g_r, day(2026, 11, 3)) &&
	      ctui_cal_event_occurs(&e, &g_r, day(2026, 11, 5)));
	CHECK(ctui_cal_event_next(&e, &g_r, day(2026, 11, 2), &n) &&
	      n == day(2026, 11, 5));
	CHECK(ctui_cal_event_expand(&e, &g_r, day(2026, 11, 1),
				    day(2026, 11, 7), NULL, 0) == 5);
	/* an exception still counts toward the count */
	g_r.end = CTUI_CAL_END_COUNT;
	g_r.count = 5;
	CHECK(ctui_cal_event_expand(&e, &g_r, day(2026, 11, 1),
				    day(2026, 12, 7), NULL, 0) == 3);
	/* a corrupt count does not read past the table */
	g_r.exception_count = 99;
	CHECK(ctui_cal_event_occurs(&e, &g_r, day(2026, 11, 1)));
}

static void test_range_and_limits(void)
{
	ctui_cal_event e = ev(day(2026, 11, 2), CTUI_CAL_REPEAT_WEEKLY, 1);
	long days[4];
	CHECK(ctui_cal_event_expand(&e, &g_r, day(2026, 11, 2),
				    day(2026, 11, 2), days, 4) == 1);
	CHECK(ctui_cal_event_expand(&e, &g_r, day(2026, 11, 3),
				    day(2026, 11, 8), days, 4) == 0);
	CHECK(ctui_cal_event_expand(&e, &g_r, day(2026, 11, 9),
				    day(2026, 11, 1), days, 4) == 0);
	CHECK(ctui_cal_event_expand(&e, &g_r, day(2026, 1, 1), day(2026, 11, 1),
				    days, 4) == 0);

	/* year 1 and year 9999 */
	e = ev(day(1, 1, 1), CTUI_CAL_REPEAT_YEARLY, 1);
	long n;
	CHECK(ctui_cal_event_occurs(&e, &g_r, day(1, 1, 1)) &&
	      ctui_cal_event_next(&e, &g_r, day(1, 1, 1), &n) &&
	      n == day(2, 1, 1));
	e = ev(day(9998, 12, 31), CTUI_CAL_REPEAT_YEARLY, 1);
	CHECK(ctui_cal_event_next(&e, &g_r, day(9998, 12, 31), &n) &&
	      n == day(9999, 12, 31));
	CHECK(!ctui_cal_event_next(&e, &g_r, day(9999, 12, 31), &n));
	e = ev(day(9999, 12, 30), CTUI_CAL_REPEAT_DAILY, 1);
	CHECK(ctui_cal_event_expand(&e, &g_r, day(9999, 12, 29),
				    day(10000, 1, 10), NULL, 0) == 2);
	e = ev(day(9999, 12, 1), CTUI_CAL_REPEAT_MONTHLY, 1);
	CHECK(!ctui_cal_event_next(&e, &g_r, day(9999, 12, 1), &n));
	e = ev(day(9999, 12, 1), CTUI_CAL_REPEAT_WEEKLY, 1);
	CHECK(ctui_cal_event_next(&e, &g_r, day(9999, 12, 24), &n) &&
	      n == day(9999, 12, 29) &&
	      !ctui_cal_event_next(&e, &g_r, day(9999, 12, 29), &n));
	/* an anchor before year 1: nothing occurs */
	e = ev(day(1, 1, 1) - 10, CTUI_CAL_REPEAT_DAILY, 1);
	CHECK(!ctui_cal_event_occurs(&e, &g_r, day(1, 1, 1) - 5));
	CHECK(
	    !ctui_cal_event_next(&e, &g_r, day(1, 1, 1) - 20,
				 &n)); /* an anchor outside 1..9999 has none */
}

int main(void)
{
	RUN(test_single);
	RUN(test_daily);
	RUN(test_weekly);
	RUN(test_monthly);
	RUN(test_yearly);
	RUN(test_end);
	RUN(test_exceptions);
	RUN(test_range_and_limits);
	return CHECK_DONE();
}
