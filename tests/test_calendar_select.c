/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */
#include "check.h"

#include <ctui/calendar.h>
#include <ctui/calendar_select.h>

static long day(int y, int m, int d)
{
	return ctui_date_to_days(y, m, d);
}

static void test_add_toggle(void)
{
	ctui_cal_select s = {0};
	CHECK(ctui_cal_select_count(&s) == 0 &&
	      ctui_cal_select_first(&s) == 0 && ctui_cal_select_last(&s) == 0 &&
	      !ctui_cal_select_contiguous(&s));
	CHECK(ctui_cal_select_add(&s, day(2026, 11, 5)) &&
	      ctui_cal_select_add(&s, day(2026, 11, 3)));
	CHECK(ctui_cal_select_add(&s, day(2026, 11, 3))); /* already there */
	CHECK(ctui_cal_select_count(&s) == 2 &&
	      ctui_cal_select_at(&s, 0) == day(2026, 11, 3) &&
	      ctui_cal_select_at(&s, 1) == day(2026, 11, 5));
	CHECK(ctui_cal_select_at(&s, 2) == 0 &&
	      ctui_cal_select_at(&s, -1) == 0);
	CHECK(ctui_cal_select_first(&s) == day(2026, 11, 3) &&
	      ctui_cal_select_last(&s) == day(2026, 11, 5));
	CHECK(ctui_cal_select_contains(&s, day(2026, 11, 5)) &&
	      !ctui_cal_select_contains(&s, day(2026, 11, 4)));
	CHECK(!ctui_cal_select_contiguous(&s));

	CHECK(ctui_cal_select_toggle(
	    &s, day(2026, 11, 4))); /* added, becomes the anchor */
	CHECK(s.has_anchor && s.anchor == day(2026, 11, 4) &&
	      ctui_cal_select_contiguous(&s));
	CHECK(ctui_cal_select_toggle(&s, day(2026, 11, 4)) &&
	      ctui_cal_select_count(&s) == 2);
	CHECK(!ctui_cal_select_contains(&s, day(2026, 11, 4)));
	ctui_cal_select_clear(&s);
	CHECK(ctui_cal_select_count(&s) == 0 && !s.has_anchor && !s.dragging);
	CHECK(ctui_cal_select_toggle(&s, day(2026, 1, 1)) &&
	      ctui_cal_select_contiguous(&s));
}

static void test_limits(void)
{
	ctui_cal_select s = {0};
	/* outside years 1..9999 */
	CHECK(!ctui_cal_select_add(&s, day(1, 1, 1) - 1) &&
	      !ctui_cal_select_add(&s, day(9999, 12, 31) + 1));
	CHECK(!ctui_cal_select_toggle(&s, day(10000, 1, 1)) &&
	      ctui_cal_select_count(&s) == 0);
	CHECK(ctui_cal_select_add(&s, day(1, 1, 1)) &&
	      ctui_cal_select_add(&s, day(9999, 12, 31)));
	CHECK(!ctui_cal_select_set_range(&s, day(9999, 12, 30),
					 day(10000, 1, 2)) &&
	      ctui_cal_select_count(&s) == 2); /* unchanged */
	ctui_cal_select_clear(&s);

	/* the cap */
	for (int i = 0; i < CTUI_CAL_SELECT_MAX; i++)
		CHECK(ctui_cal_select_add(&s, day(2026, 1, 1) + 2L * i));
	CHECK(!ctui_cal_select_add(&s, day(2026, 12, 31)) &&
	      !ctui_cal_select_toggle(&s, day(2026, 12, 31)));
	CHECK(ctui_cal_select_count(&s) == CTUI_CAL_SELECT_MAX);
	CHECK(ctui_cal_select_toggle(
	    &s, day(2026, 1, 1))); /* removing still works when full */
	CHECK(ctui_cal_select_count(&s) == CTUI_CAL_SELECT_MAX - 1);
	bool sorted = true;
	for (int i = 1; i < s.count; i++)
		sorted = sorted && s.days[i - 1] < s.days[i];
	CHECK(sorted);
	CHECK(ctui_cal_select_set_range(
	    &s, day(2026, 3, 1), day(2026, 3, 1) + CTUI_CAL_SELECT_MAX - 1));
	CHECK(ctui_cal_select_count(&s) == CTUI_CAL_SELECT_MAX);
	CHECK(!ctui_cal_select_set_range(
	    &s, day(2026, 3, 1), day(2026, 3, 1) + CTUI_CAL_SELECT_MAX));
	CHECK(ctui_cal_select_count(&s) == CTUI_CAL_SELECT_MAX &&
	      ctui_cal_select_first(&s) == day(2026, 3, 1));
}

static void test_range_extend(void)
{
	ctui_cal_select s = {0};
	long a = day(2026, 12, 30);
	/* from 30 Dec to 2 Jan, across the year end; either order */
	CHECK(ctui_cal_select_set_range(&s, day(2027, 1, 2), a));
	CHECK(ctui_cal_select_count(&s) == 4 &&
	      ctui_cal_select_first(&s) == a &&
	      ctui_cal_select_last(&s) == day(2027, 1, 2) &&
	      ctui_cal_select_contiguous(&s));
	CHECK(s.anchor == day(2027, 1, 2));

	ctui_cal_select_clear(&s);
	/* Shift+Right from the 5th: the anchor is the cursor before the move */
	CHECK(ctui_cal_select_extend(&s, day(2026, 11, 5), day(2026, 11, 6)));
	CHECK(ctui_cal_select_count(&s) == 2 && s.anchor == day(2026, 11, 5));
	CHECK(ctui_cal_select_extend(&s, day(2026, 11, 6), day(2026, 11, 7)) &&
	      ctui_cal_select_count(&s) == 3);
	/* back past the anchor: the range flips to the other side */
	CHECK(ctui_cal_select_extend(&s, day(2026, 11, 7), day(2026, 11, 4)));
	CHECK(ctui_cal_select_count(&s) == 2 &&
	      ctui_cal_select_first(&s) == day(2026, 11, 4) &&
	      ctui_cal_select_last(&s) == day(2026, 11, 5));
	/* a plain move forgets the anchor and keeps the days */
	ctui_cal_select_move(&s);
	CHECK(!s.has_anchor && ctui_cal_select_count(&s) == 2);
	CHECK(ctui_cal_select_extend(&s, day(2026, 11, 10), day(2026, 11, 11)));
	CHECK(ctui_cal_select_count(&s) == 2 &&
	      ctui_cal_select_first(&s) == day(2026, 11, 10));
	/* too long: refused, unchanged */
	CHECK(!ctui_cal_select_extend(&s, day(2026, 11, 11),
				      day(2026, 11, 10) + 100));
	CHECK(ctui_cal_select_count(&s) == 2);
	/* a leap day inside the range */
	CHECK(
	    ctui_cal_select_set_range(&s, day(2024, 2, 28), day(2024, 3, 1)) &&
	    ctui_cal_select_count(&s) == 3);
	CHECK(ctui_cal_select_contains(&s, day(2024, 2, 29)));
}

static void test_drag(void)
{
	ctui_cal_select s = {0};
	ctui_cal_select_toggle(&s, day(2026, 11, 20));
	CHECK(ctui_cal_select_begin(
	    &s, day(2026, 11, 3))); /* emptied the old set */
	CHECK(ctui_cal_select_count(&s) == 0 && s.dragging &&
	      s.anchor == day(2026, 11, 3));
	CHECK(!ctui_cal_select_drag_to(
	    &s, day(2026, 11, 3))); /* a click on one day selects none */
	CHECK(ctui_cal_select_drag_to(&s, day(2026, 11, 5)) &&
	      ctui_cal_select_count(&s) == 3);
	CHECK(!ctui_cal_select_drag_to(
	    &s, day(2026, 11, 5))); /* same place: no change */
	CHECK(ctui_cal_select_drag_to(&s, day(2026, 11, 1)) &&
	      ctui_cal_select_count(&s) == 3 &&
	      ctui_cal_select_first(&s) == day(2026, 11, 1));
	CHECK(ctui_cal_select_drag_to(&s, day(2026, 11, 3)) &&
	      ctui_cal_select_count(&s) == 0);
	CHECK(ctui_cal_select_drag_to(&s, day(2026, 11, 4)) &&
	      ctui_cal_select_count(&s) == 2);
	CHECK(!ctui_cal_select_drag_to(&s, day(2026, 11, 3) + 200) &&
	      ctui_cal_select_count(&s) == 2);
	ctui_cal_select_end(&s);
	CHECK(!s.dragging && !ctui_cal_select_drag_to(&s, day(2026, 11, 9)) &&
	      ctui_cal_select_count(&s) == 2);
	CHECK(ctui_cal_select_begin(
	    &s, day(10000, 1, 1))); /* emptied, nothing armed */
	CHECK(!s.dragging && ctui_cal_select_count(&s) == 0);
}

int main(void)
{
	RUN(test_add_toggle);
	RUN(test_limits);
	RUN(test_range_extend);
	RUN(test_drag);
	return CHECK_DONE();
}
