/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */
#include "check.h"

#include <ctui/calendar_view.h>

#define DM 1440

static long day(int y, int m, int d)
{
	return ctui_date_to_days(y, m, d);
}

static void test_range(void)
{
	ctui_cal_span sp = {0};
	ctui_cal_event e = {0, 0, 0, "x", 7};
	CHECK(ctui_cal_span_extra(NULL) == 0 && ctui_cal_span_extra(&sp) == 0);
	sp.extra_days = -3;
	CHECK(ctui_cal_span_extra(&sp) == 0);
	sp.extra_days = 100000;
	CHECK(ctui_cal_span_extra(&sp) == CTUI_CAL_SPAN_MAX);

	long d3 = day(2026, 11, 3);
	/* 3 to 7 November, all day: start 3rd 00:00, end 8th 00:00 */
	CHECK(ctui_cal_event_set_range(&e, &sp, d3 * DM, (d3 + 5) * DM));
	CHECK(e.day == d3 && e.start_minute == 0 && e.end_minute == DM &&
	      sp.extra_days == 4);
	CHECK(e.pair == 7 && e.title &&
	      ctui_cal_event_start_abs(&e) == d3 * DM &&
	      ctui_cal_event_end_abs(&e, &sp) == (d3 + 5) * DM);
	/* from 22:00 on the 3rd to 10:30 on the 5th */
	CHECK(ctui_cal_event_set_range(&e, &sp, d3 * DM + 1320,
				       (d3 + 2) * DM + 630));
	CHECK(e.day == d3 && e.start_minute == 1320 && e.end_minute == 630 &&
	      sp.extra_days == 2);
	/* inside one day: no extra day, NULL span accepted for that */
	CHECK(ctui_cal_event_set_range(&e, &sp, d3 * DM + 60, d3 * DM + 120));
	CHECK(sp.extra_days == 0 && e.start_minute == 60 &&
	      e.end_minute == 120);
	CHECK(ctui_cal_event_set_range(&e, NULL, d3 * DM + 60, d3 * DM + 90) &&
	      e.end_minute == 90);
	/* an end at midnight is the end of the day before */
	CHECK(ctui_cal_event_set_range(&e, &sp, d3 * DM + 1380, (d3 + 1) * DM));
	CHECK(sp.extra_days == 0 && e.end_minute == DM);
	/* the end must follow the start */
	ctui_cal_event keep = e;
	CHECK(!ctui_cal_event_set_range(&e, &sp, d3 * DM + 100, d3 * DM + 100));
	CHECK(!ctui_cal_event_set_range(&e, &sp, d3 * DM + 100, d3 * DM + 50));
	CHECK(e.day == keep.day && e.start_minute == keep.start_minute &&
	      sp.extra_days == 0);
	/* limits of the years and of the length */
	CHECK(!ctui_cal_event_set_range(&e, &sp, day(1, 1, 1) * DM - 1,
					day(1, 1, 1) * DM + 10));
	CHECK(ctui_cal_event_set_range(&e, &sp, day(1, 1, 1) * DM,
				       day(1, 1, 3) * DM) &&
	      sp.extra_days == 1);
	CHECK(ctui_cal_event_set_range(&e, &sp, day(9999, 12, 30) * DM,
				       (day(9999, 12, 31) + 1) * DM));
	CHECK(sp.extra_days == 1 && e.end_minute == DM);
	CHECK(!ctui_cal_event_set_range(&e, &sp, day(9999, 12, 30) * DM,
					(day(9999, 12, 31) + 1) * DM + 1));
	CHECK(ctui_cal_event_set_range(&e, &sp, d3 * DM,
				       (d3 + CTUI_CAL_SPAN_MAX + 1) * DM) &&
	      sp.extra_days == CTUI_CAL_SPAN_MAX);
	CHECK(!ctui_cal_event_set_range(&e, &sp, d3 * DM,
					(d3 + CTUI_CAL_SPAN_MAX + 2) * DM));
	/* across a leap day and a year end */
	CHECK(ctui_cal_event_set_range(&e, &sp, day(2028, 2, 28) * DM + 600,
				       day(2028, 3, 1) * DM + 600));
	CHECK(sp.extra_days == 2 && e.day == day(2028, 2, 28));
	CHECK(ctui_cal_event_set_range(&e, &sp, day(2026, 12, 31) * DM,
				       day(2027, 1, 2) * DM + 1));
	CHECK(sp.extra_days == 2 && e.end_minute == 1);
}

static void test_segment(void)
{
	long o = day(2026, 11, 3);
	ctui_cal_event e = {o, 600, 660, "x", 0};
	int s, t;
	CHECK(ctui_cal_event_segment(&e, NULL, o, o, &s, &t) && s == 600 &&
	      t == 660);
	CHECK(!ctui_cal_event_segment(&e, NULL, o, o + 1, &s, &t) &&
	      !ctui_cal_event_segment(&e, NULL, o, o - 1, &s, &t));
	ctui_cal_event odd = {o, 700, 700, "x", 0}; /* lasts a minute */
	CHECK(ctui_cal_event_segment(&odd, NULL, o, o, &s, &t) && s == 700 &&
	      t == 701);
	ctui_cal_event late = {o, 1440, 1500, "x", 0};
	CHECK(!ctui_cal_event_segment(&late, NULL, o, o, &s, &t));
	ctui_cal_event cut = {o, 1400, 2000, "x", 0};
	CHECK(ctui_cal_event_segment(&cut, NULL, o, o, &s, &t) && s == 1400 &&
	      t == DM);

	/* 22:00 on the 3rd to 10:30 on the 6th */
	ctui_cal_span sp = {3};
	e = (ctui_cal_event){o, 1320, 630, "x", 0};
	CHECK(ctui_cal_event_segment(&e, &sp, o, o, &s, &t) && s == 1320 &&
	      t == DM);
	CHECK(ctui_cal_event_segment(&e, &sp, o, o + 1, &s, &t) && s == 0 &&
	      t == DM);
	CHECK(ctui_cal_event_segment(&e, &sp, o, o + 2, &s, &t) && s == 0 &&
	      t == DM);
	CHECK(ctui_cal_event_segment(&e, &sp, o, o + 3, &s, &t) && s == 0 &&
	      t == 630);
	CHECK(!ctui_cal_event_segment(&e, &sp, o, o + 4, &s, &t) &&
	      !ctui_cal_event_segment(&e, &sp, o, o - 1, &s, &t));
	/* a later occurrence of a series */
	CHECK(ctui_cal_event_segment(&e, &sp, o + 7, o + 10, &s, &t) &&
	      t == 630);
	CHECK(ctui_cal_event_segment(&e, &sp, o, o, NULL, NULL));
}

static void test_covers(void)
{
	long mon = day(2026, 11, 2), o;
	ctui_cal_event e = {mon, 0, DM, "Holiday", 0};
	ctui_cal_span sp = {2}; /* three days */
	CHECK(!ctui_cal_event_covers(&e, NULL, &sp, mon - 1, &o));
	CHECK(ctui_cal_event_covers(&e, NULL, &sp, mon, &o) && o == mon);
	CHECK(ctui_cal_event_covers(&e, NULL, &sp, mon + 2, &o) && o == mon);
	CHECK(!ctui_cal_event_covers(&e, NULL, &sp, mon + 3, &o));
	CHECK(ctui_cal_event_covers(&e, NULL, &sp, mon + 1, NULL));
	/* single-day events work as before, with or without a span */
	ctui_cal_span none = {0};
	CHECK(ctui_cal_event_covers(&e, NULL, NULL, mon, &o) && o == mon &&
	      !ctui_cal_event_covers(&e, NULL, &none, mon + 1, &o));

	/* every Monday, three days long: the length is kept in every occurrence
	 */
	ctui_cal_recur weekly = {.repeat = CTUI_CAL_REPEAT_WEEKLY,
				 .interval = 1};
	CHECK(ctui_cal_event_covers(&e, &weekly, &sp, mon + 7, &o) &&
	      o == mon + 7);
	CHECK(ctui_cal_event_covers(&e, &weekly, &sp, mon + 21 + 2, &o) &&
	      o == mon + 21);
	CHECK(!ctui_cal_event_covers(&e, &weekly, &sp, mon + 21 + 3, &o));
	CHECK(!ctui_cal_event_covers(&e, &weekly, &sp, mon - 1,
				     &o)); /* nothing before the anchor */
	/* every other Monday */
	weekly.interval = 2;
	CHECK(!ctui_cal_event_covers(&e, &weekly, &sp, mon + 8, &o));
	CHECK(ctui_cal_event_covers(&e, &weekly, &sp, mon + 15, &o) &&
	      o == mon + 14);
	/* a day left out of the series: the occurrence disappears with its days
	 */
	weekly.interval = 1;
	weekly.exception_count = 1;
	weekly.exceptions[0] = mon + 7;
	CHECK(!ctui_cal_event_covers(&e, &weekly, &sp, mon + 8, &o) &&
	      ctui_cal_event_covers(&e, &weekly, &sp, mon + 14, &o));
	/* two occurrences only */
	weekly.exception_count = 0;
	weekly.end = CTUI_CAL_END_COUNT;
	weekly.count = 2;
	CHECK(ctui_cal_event_covers(&e, &weekly, &sp, mon + 9, &o) &&
	      o == mon + 7);
	CHECK(!ctui_cal_event_covers(&e, &weekly, &sp, mon + 14, &o));
	/* overlapping occurrences: the latest one counts */
	ctui_cal_recur daily = {.repeat = CTUI_CAL_REPEAT_DAILY, .interval = 1};
	CHECK(ctui_cal_event_covers(&e, &daily, &sp, mon + 5, &o) &&
	      o == mon + 5);

	/* across a leap day: 28 February 2028 for three days covers 29 February
	 * and 1 March */
	long feb28 = day(2028, 2, 28);
	ctui_cal_event leap = {feb28, 600, 700, "x", 0};
	ctui_cal_span two = {2};
	CHECK(ctui_cal_event_covers(&leap, NULL, &two, day(2028, 3, 1), &o) &&
	      o == feb28);
	CHECK(!ctui_cal_event_covers(&leap, NULL, &two, day(2028, 3, 2), &o));
	/* yearly on 29 February with a length: only leap years */
	ctui_cal_event y29 = {day(2024, 2, 29), 0, DM, "x", 0};
	ctui_cal_recur yearly = {.repeat = CTUI_CAL_REPEAT_YEARLY,
				 .interval = 1};
	CHECK(ctui_cal_event_covers(&y29, &yearly, &two, day(2028, 3, 1), &o) &&
	      o == day(2028, 2, 29));
	CHECK(!ctui_cal_event_covers(&y29, &yearly, &two, day(2027, 3, 1), &o));
	/* the end of year 9999 and the start of year 1 */
	ctui_cal_event last = {day(9999, 12, 30), 0, DM, "x", 0};
	CHECK(ctui_cal_event_covers(&last, NULL, &two, day(9999, 12, 31), &o) &&
	      o == day(9999, 12, 30));
	ctui_cal_event first = {day(1, 1, 1), 0, DM, "x", 0};
	CHECK(ctui_cal_event_covers(&first, &daily, &two, day(1, 1, 2), &o));
	CHECK(
	    !ctui_cal_event_covers(&first, &daily, &two, day(1, 1, 1) - 1, &o));
}

static void test_expand_and_mask(void)
{
	long mon = day(2026, 11, 2);
	ctui_cal_event e = {mon, 0, DM, "x", 0};
	ctui_cal_span sp = {2};
	ctui_cal_recur weekly = {.repeat = CTUI_CAL_REPEAT_WEEKLY,
				 .interval = 1};
	long days[8];
	/* the week of the 9th is touched by the occurrence of the 9th only (the
	 * 2nd ends on the 4th) */
	CHECK(ctui_cal_event_expand_span(&e, &weekly, &sp, mon + 7, mon + 13,
					 days, 8) == 1 &&
	      days[0] == mon + 7);
	/* a window that starts inside an occurrence sees it */
	CHECK(ctui_cal_event_expand_span(&e, &weekly, &sp, mon + 8, mon + 8,
					 days, 8) == 1 &&
	      days[0] == mon + 7);
	CHECK(ctui_cal_event_expand_span(&e, &weekly, &sp, mon + 10, mon + 12,
					 days, 8) == 0);
	CHECK(ctui_cal_event_expand_span(&e, &weekly, &sp, mon, mon + 20, days,
					 2) == 3 &&
	      days[1] == mon + 7);
	CHECK(ctui_cal_event_expand_span(&e, NULL, NULL, mon - 3, mon + 3, NULL,
					 0) == 1);

	/* November 2026: the 2nd to 4th every week, a holiday 20th to 22nd, a
	 * plain event on the 10th */
	ctui_cal_event ev[3] = {e,
				{day(2026, 11, 20), 0, DM, "h", 0},
				{day(2026, 11, 10), 0, 60, "p", 0}};
	ctui_cal_span spans[3] = {{2}, {2}, {0}};
	ctui_cal_recur rec[3] = {weekly, {0}, {0}};
	uint32_t m = ctui_cal_span_month_mask(ev, rec, spans, 3, 2026, 11);
	uint32_t want = 0;
	for (int d = 1; d <= 30; d++)
		if ((d >= 2 && d <= 4) || (d >= 9 && d <= 11) ||
		    (d >= 16 && d <= 18) || (d >= 20 && d <= 25) || d >= 30)
			want |= 1U << (unsigned)(d - 1);
	CHECK(m == want);
	CHECK((m & (1U << 9U)) != 0); /* the 10th is inside the 9th-11th
					occurrence, not the plain event */
	CHECK(ctui_cal_span_month_mask(ev, rec, NULL, 3, 2026, 11) == 0);
	/* an event of the previous month spilling into this one */
	ctui_cal_event spill = {day(2026, 10, 30), 0, DM, "s", 0};
	ctui_cal_span three = {3};
	CHECK(ctui_cal_span_month_mask(&spill, NULL, &three, 1, 2026, 11) ==
	      3U);
	CHECK(ctui_cal_span_month_mask(&spill, NULL, &three, 1, 2026, 12) == 0);
}

static void test_resize_range(void)
{
	long d3 = day(2026, 11, 3);
	ctui_cal_event e = {d3, 600, 660, "x", 0};
	ctui_cal_span sp = {2}; /* 10:00 on the 3rd to 11:00 on the 5th */
	CHECK(ctui_cal_event_end_abs(&e, &sp) == (d3 + 2) * DM + 660);
	/* the end moves to the 7th at 17:15, snapped down to the half hour */
	CHECK(ctui_cal_event_resize_range(&e, &sp, false, (d3 + 4) * DM + 1035,
					  30, false));
	CHECK(sp.extra_days == 4 && e.end_minute == 1020 &&
	      e.start_minute == 600 && e.day == d3);
	/* the end back to the first day, before the start: one slot after it at
	 * least */
	CHECK(ctui_cal_event_resize_range(&e, &sp, false, d3 * DM + 100, 30,
					  false));
	CHECK(sp.extra_days == 0 && e.start_minute == 600 &&
	      e.end_minute == 630);
	/* the start moves to an earlier day */
	sp.extra_days = 2;
	e.end_minute = 660;
	CHECK(ctui_cal_event_resize_range(&e, &sp, true, (d3 - 2) * DM + 495,
					  30, false));
	CHECK(e.day == d3 - 2 && e.start_minute == 480 && sp.extra_days == 4 &&
	      e.end_minute == 660);
	/* ... but stays on its day for a repeating event */
	e = (ctui_cal_event){d3, 600, 660, "x", 0};
	sp.extra_days = 2;
	CHECK(ctui_cal_event_resize_range(&e, &sp, true, (d3 - 2) * DM + 495,
					  30, true));
	CHECK(e.day == d3 && e.start_minute == 480 && sp.extra_days == 2);
	/* the start cannot pass the end: it stops a slot before it */
	CHECK(ctui_cal_event_resize_range(&e, &sp, true, (d3 + 9) * DM, 30,
					  false));
	CHECK(e.day == d3 + 2 && e.start_minute == 630 && e.end_minute == 660 &&
	      sp.extra_days == 0);
	/* no change is reported as such */
	e = (ctui_cal_event){d3, 600, 660, "x", 0};
	sp.extra_days = 2;
	CHECK(!ctui_cal_event_resize_range(&e, &sp, false, (d3 + 2) * DM + 660,
					   30, false));
	/* the end of a day: pushing the end past it goes on in the next day */
	e = (ctui_cal_event){d3, 600, DM, "x", 0};
	sp.extra_days = 0;
	CHECK(ctui_cal_event_resize_range(&e, &sp, false, (d3 + 1) * DM + 30,
					  30, false));
	CHECK(sp.extra_days == 1 && e.end_minute == 30);
	CHECK(ctui_cal_event_resize_range(&e, &sp, false, d3 * DM + 1380, 30,
					  false));
	CHECK(sp.extra_days == 0 && e.end_minute == 1380);
	/* never ends at or before its start, whatever is asked */
	for (long at = (d3 - 3) * DM; at < (d3 + 4) * DM; at += 17) {
		e = (ctui_cal_event){d3, 600, 660, "x", 0};
		sp.extra_days = 1;
		ctui_cal_event_resize_range(&e, &sp, at % 2 == 0, at, 30,
					    false);
		CHECK(ctui_cal_event_end_abs(&e, &sp) >
		      ctui_cal_event_start_abs(&e));
	}
	/* the limits of the years */
	e = (ctui_cal_event){day(9999, 12, 30), 0, DM, "x", 0};
	sp.extra_days = 1;
	CHECK(!ctui_cal_event_resize_range(
	    &e, &sp, false, (day(9999, 12, 31) + 5) * DM, 30, false));
	CHECK(sp.extra_days == 1 && e.day == day(9999, 12, 30));
	e = (ctui_cal_event){day(1, 1, 2), 0, DM, "x", 0};
	sp.extra_days = 1;
	CHECK(ctui_cal_event_resize_range(&e, &sp, true,
					  (day(1, 1, 1) - 5) * DM, 30, false));
	CHECK(e.day == day(1, 1, 1) && e.start_minute == 0);
}

static void test_move_clamp_span(void)
{
	long d, first = day(2026, 11, 2), last = first + 6;
	int s;
	/* a 2 day 3 hour event keeps any start time of the day */
	ctui_cal_move_clamp_span(2 * DM + 180, first + 1, 1410, 30, first, last,
				 &d, &s);
	CHECK(d == first + 1 && s == 1410);
	ctui_cal_move_clamp_span(2 * DM + 180, first + 1, 1425, 30, first, last,
				 &d, &s);
	CHECK(s == 1410);
	ctui_cal_move_clamp_span(100, first - 3, -40, 30, first, last, &d, &s);
	CHECK(d == first && s == 0);
	ctui_cal_move_clamp_span(100, last + 3, 5000, 30, first, last, &d, &s);
	CHECK(d == last && s == 1410);
	/* end of year 9999: the day moves back so that the event still ends in
	 * it */
	long end = day(9999, 12, 31);
	ctui_cal_move_clamp_span(3L * DM, end, 600, 30, end - 10, end, &d, &s);
	CHECK(d == end - 3 && s == 600);
	ctui_cal_move_clamp_span(3L * DM, end, 0, 30, end - 10, end, &d, &s);
	CHECK(d == end - 2 && s == 0);
	ctui_cal_move_clamp_span(0, first, 600, 0, first, last, NULL, NULL);
}

int main(void)
{
	RUN(test_range);
	RUN(test_segment);
	RUN(test_covers);
	RUN(test_expand_and_mask);
	RUN(test_resize_range);
	RUN(test_move_clamp_span);
	return CHECK_DONE();
}
