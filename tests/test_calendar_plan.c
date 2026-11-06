/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */
#include "check.h"

#include <ctui/calendar_view.h>

static long day(int y, int m, int d)
{
	return ctui_date_to_days(y, m, d);
}

/* A selection of days of November 2026 (Monday the 2nd). */
static ctui_cal_select pick(int n, const int *d)
{
	ctui_cal_select s = {0};
	for (int i = 0; i < n; i++)
		ctui_cal_select_add(&s, day(2026, 11, d[i]));
	return s;
}

#define SEL(...)                                                               \
	pick((int)(sizeof((int[]){__VA_ARGS__}) / sizeof(int)),                \
	     (int[]){__VA_ARGS__})

/* Expand the rule of a plan from its first day: the days it gives up to `last`.
 */
static int occurrences(const ctui_cal_place *p, long last, long *out, int max)
{
	ctui_cal_event e = {p->first, 600, 660, "x", 0};
	return ctui_cal_event_expand(&e, &p->rule, p->first, last, out, max);
}

static void test_classify(void)
{
	ctui_cal_select s = SEL(2);
	ctui_cal_select none = {0};
	CHECK(ctui_cal_select_classify(&none) == CTUI_CAL_SEL_NONE);
	CHECK(ctui_cal_select_classify(&s) == CTUI_CAL_SEL_SINGLE);
	s = SEL(3, 4, 5);
	CHECK(ctui_cal_select_classify(&s) == CTUI_CAL_SEL_RANGE);
	s = SEL(2, 5); /* Monday and Thursday */
	CHECK(ctui_cal_select_classify(&s) == CTUI_CAL_SEL_WEEKLY);
	s = SEL(2, 5, 9, 12);
	CHECK(ctui_cal_select_classify(&s) == CTUI_CAL_SEL_WEEKLY);
	s = SEL(2, 3, 11); /* Mon, Tue, then a Wednesday a week later */
	CHECK(ctui_cal_select_classify(&s) == CTUI_CAL_SEL_ARBITRARY);
	s = SEL(2, 9, 30); /* Mondays one and three weeks apart */
	CHECK(ctui_cal_select_classify(&s) ==
	      CTUI_CAL_SEL_ARBITRARY); /* every week would add the 16th */
}

static void test_repeat_rules(void)
{
	ctui_cal_place p;
	ctui_cal_select none = {0}, s = SEL(5);
	p = ctui_cal_plan_repeat(&none);
	CHECK(p.kind == CTUI_CAL_PLACE_NONE && p.days == 0 && p.events == 0);
	p = ctui_cal_plan_repeat(&s);
	CHECK(p.kind == CTUI_CAL_PLACE_SINGLE && p.events == 1 &&
	      p.first == day(2026, 11, 5));

	/* Monday and Thursday: one weekly rule on both weekdays, from Monday */
	s = SEL(2, 5);
	p = ctui_cal_plan_repeat(&s);
	CHECK(p.kind == CTUI_CAL_PLACE_SERIES && p.events == 1 && p.days == 2 &&
	      p.first == day(2026, 11, 2));
	CHECK(p.rule.repeat == CTUI_CAL_REPEAT_WEEKLY && p.rule.interval == 1 &&
	      p.rule.weekdays == (1U << 0U | 1U << 3U) &&
	      p.rule.end == CTUI_CAL_END_NEVER);
	long got[6];
	CHECK(occurrences(&p, day(2026, 11, 20), got, 6) == 6);
	CHECK(got[0] == day(2026, 11, 2) && got[1] == day(2026, 11, 5) &&
	      got[2] == day(2026, 11, 9) && got[5] == day(2026, 11, 19));

	/* a start in mid-week: Thursday 5, then Monday 9 and Thursday 12 */
	s = SEL(5, 9, 12);
	p = ctui_cal_plan_repeat(&s);
	CHECK(p.kind == CTUI_CAL_PLACE_SERIES &&
	      p.rule.weekdays == (1U << 0U | 1U << 3U) && p.rule.interval == 1);
	CHECK(p.first == day(2026, 11, 5));

	/* every second week: Monday 2 and 16 */
	s = SEL(2, 16);
	p = ctui_cal_plan_repeat(&s);
	CHECK(p.kind == CTUI_CAL_PLACE_SERIES && p.rule.interval == 2 &&
	      p.rule.weekdays == 1U);
	CHECK(occurrences(&p, day(2026, 12, 14), got, 4) == 4 &&
	      got[2] == day(2026, 11, 30));
	/* three Mondays two weeks apart, and Tue+Fri in a week two weeks later
	 */
	s = SEL(2, 16, 30);
	p = ctui_cal_plan_repeat(&s);
	CHECK(p.kind == CTUI_CAL_PLACE_SERIES && p.rule.interval == 2);
	s = SEL(3, 6, 17, 20);
	p = ctui_cal_plan_repeat(&s);
	CHECK(p.kind == CTUI_CAL_PLACE_SERIES && p.rule.interval == 2 &&
	      p.rule.weekdays == (1U << 1U | 1U << 4U));

	/* consecutive days are a series on those weekdays too */
	s = SEL(3, 4, 5);
	p = ctui_cal_plan_repeat(&s);
	CHECK(p.kind == CTUI_CAL_PLACE_SERIES &&
	      p.rule.weekdays == (1U << 1U | 1U << 2U | 1U << 3U));
	/* a range over a week end: Saturday 7 to Monday 9 */
	s = SEL(7, 8, 9);
	p = ctui_cal_plan_repeat(&s);
	CHECK(p.kind == CTUI_CAL_PLACE_SERIES &&
	      p.rule.weekdays == (1U << 5U | 1U << 6U | 1U << 0U));
	CHECK(occurrences(&p, day(2026, 11, 9), got, 6) == 3);
	/* ten consecutive days still map: every weekday, every week */
	ctui_cal_select ten = {0};
	ctui_cal_select_set_range(&ten, day(2026, 11, 2), day(2026, 11, 11));
	p = ctui_cal_plan_repeat(&ten);
	CHECK(p.kind == CTUI_CAL_PLACE_SERIES && p.rule.weekdays == 0x7Fu &&
	      p.days == 10);
}

static void test_repeat_fallback(void)
{
	ctui_cal_place p;
	/* three dates in different weeks with uneven gaps: one event per day */
	ctui_cal_select s = SEL(2, 11, 30);
	p = ctui_cal_plan_repeat(&s);
	CHECK(p.kind == CTUI_CAL_PLACE_EACH && p.events == 3 && p.days == 3 &&
	      p.first == day(2026, 11, 2));
	CHECK(p.rule.repeat == CTUI_CAL_REPEAT_NONE);
	/* a weekday that is not in the first week again */
	s = SEL(2, 4, 11);
	p = ctui_cal_plan_repeat(&s);
	CHECK(p.kind == CTUI_CAL_PLACE_EACH && p.events == 3);
	/* a missing occurrence between two selected ones */
	s = SEL(2, 16, 23);
	p = ctui_cal_plan_repeat(&s);
	CHECK(p.kind == CTUI_CAL_PLACE_EACH);
	/* two days whose weeks are further apart than a rule can say */
	ctui_cal_select far = {0};
	ctui_cal_select_add(&far, day(1, 1, 1));
	ctui_cal_select_add(&far, day(9999, 12, 31));
	p = ctui_cal_plan_repeat(&far);
	CHECK(p.kind == CTUI_CAL_PLACE_EACH && p.events == 2);
	/* a full selection of scattered days */
	ctui_cal_select many = {0};
	for (int i = 0; i < CTUI_CAL_SELECT_MAX; i++)
		ctui_cal_select_add(&many, day(2026, 1, 1) + (long)i * i);
	p = ctui_cal_plan_repeat(&many);
	CHECK(many.count == CTUI_CAL_SELECT_MAX &&
	      p.kind == CTUI_CAL_PLACE_EACH && p.events == CTUI_CAL_SELECT_MAX);
}

static void test_year_ends_and_leap(void)
{
	ctui_cal_place p;
	/* Thursday 31 Dec 2026 and Monday 4 Jan 2027, across the year end */
	ctui_cal_select s = {0};
	ctui_cal_select_add(&s, day(2026, 12, 31));
	ctui_cal_select_add(&s, day(2027, 1, 4));
	p = ctui_cal_plan_repeat(&s);
	CHECK(p.kind == CTUI_CAL_PLACE_SERIES &&
	      p.rule.weekdays == (1U << 3U | 1U << 0U));
	long got[3];
	CHECK(occurrences(&p, day(2027, 1, 8), got, 3) == 3 &&
	      got[2] == day(2027, 1, 7));
	/* the same weekday on both sides of 29 February 2028 (four weeks apart)
	 */
	ctui_cal_select_clear(&s);
	ctui_cal_select_add(&s, day(2028, 2, 21));
	ctui_cal_select_add(&s, day(2028, 3, 20));
	p = ctui_cal_plan_repeat(&s);
	CHECK(p.kind == CTUI_CAL_PLACE_SERIES && p.rule.interval == 4 &&
	      p.rule.weekdays == 1U);
	CHECK(occurrences(&p, day(2028, 4, 20), got, 3) == 3 &&
	      got[1] == day(2028, 3, 20));
	/* the limits of the years */
	ctui_cal_select_clear(&s);
	ctui_cal_select_add(&s, day(9999, 12, 30));
	ctui_cal_select_add(&s, day(9999, 12, 31));
	p = ctui_cal_plan_repeat(&s);
	CHECK(p.kind == CTUI_CAL_PLACE_SERIES && p.days == 2);
	ctui_cal_select_clear(&s);
	ctui_cal_select_add(&s, day(1, 1, 1));
	ctui_cal_select_add(&s, day(1, 1, 2));
	p = ctui_cal_plan_repeat(&s);
	CHECK(p.kind == CTUI_CAL_PLACE_SERIES && p.first == day(1, 1, 1));
}

static void test_span_plan(void)
{
	ctui_cal_place p;
	ctui_cal_select none = {0}, s = SEL(5);
	CHECK(ctui_cal_plan_span(&none).kind == CTUI_CAL_PLACE_NONE);
	p = ctui_cal_plan_span(&s);
	CHECK(p.kind == CTUI_CAL_PLACE_SINGLE && p.days == 1);
	s = SEL(3, 4, 5, 6, 7);
	p = ctui_cal_plan_span(&s);
	CHECK(p.kind == CTUI_CAL_PLACE_SPAN && p.days == 5 && p.events == 1 &&
	      p.first == day(2026, 11, 3));
	s = SEL(3, 5); /* not consecutive: no single event */
	p = ctui_cal_plan_span(&s);
	CHECK(p.kind == CTUI_CAL_PLACE_NONE && p.days == 2);
	/* across the end of a year and a leap day */
	ctui_cal_select r = {0};
	ctui_cal_select_set_range(&r, day(2026, 12, 30), day(2027, 1, 2));
	p = ctui_cal_plan_span(&r);
	CHECK(p.kind == CTUI_CAL_PLACE_SPAN && p.days == 4 &&
	      p.first == day(2026, 12, 30));
	ctui_cal_select_set_range(&r, day(2028, 2, 28), day(2028, 3, 1));
	p = ctui_cal_plan_span(&r);
	CHECK(p.kind == CTUI_CAL_PLACE_SPAN && p.days == 3);
}

int main(void)
{
	RUN(test_classify);
	RUN(test_repeat_rules);
	RUN(test_repeat_fallback);
	RUN(test_year_ends_and_leap);
	RUN(test_span_plan);
	return CHECK_DONE();
}
