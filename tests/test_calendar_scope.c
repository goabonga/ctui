/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * Occurrence versus series: detaching an occurrence, the exception list and
 * the pending scope of a drop.
 */
#include "check.h"

#include <curses.h>

#include <ctui/calendar_view.h>

#define W 62
#define H 11
#define DM 1440
#define CAP 6

static long day(int y, int m, int d)
{
	return ctui_date_to_days(y, m, d);
}

static ctui_cal_event g_ev[CAP];
static ctui_cal_recur g_rc[CAP];
static ctui_cal_span g_sp[CAP];

/* Weekly Wednesdays 10:00-11:00 from 2026-10-28, and a plain event. */
static void series_view(ctui_cal_view *v, int count, int cap)
{
	memset(g_ev, 0, sizeof g_ev);
	memset(g_rc, 0, sizeof g_rc);
	memset(g_sp, 0, sizeof g_sp);
	g_ev[0] = (ctui_cal_event){day(2026, 10, 28), 600, 660, "Weekly",
				   CTUI_PAIR_GAUGE_GREEN};
	g_rc[0] =
	    (ctui_cal_recur){.repeat = CTUI_CAL_REPEAT_WEEKLY, .interval = 1};
	g_ev[1] = (ctui_cal_event){day(2026, 11, 5), 900, 960, "Single", 0};
	ctui_cal_view_init(v, CTUI_CAL_VIEW_WEEK, 2026, 11, 4);
	v->top = 480;
	v->minute = 600;
	ctui_cal_view_set_editable(v, g_ev, g_rc, count, cap);
	ctui_cal_view_set_editable_spans(v, g_sp);
}

static void test_detach_pure(void)
{
	ctui_cal_view v;
	series_view(&v, 2, CAP);
	int count = 2, out = -1;
	long d11 = day(2026, 11, 11);
	/* the 11th moved to 14:00-15:30 the next day */
	CHECK(ctui_cal_detach(g_ev, g_rc, g_sp, &count, CAP, 0, d11,
			      (d11 + 1) * DM + 840, (d11 + 1) * DM + 930,
			      &out) == CTUI_CAL_DETACH_OK);
	CHECK(count == 3 && out == 2);
	CHECK(g_ev[2].day == d11 + 1 && g_ev[2].start_minute == 840 &&
	      g_ev[2].end_minute == 930);
	CHECK_STR(g_ev[2].title, "Weekly");
	CHECK(g_ev[2].pair == CTUI_PAIR_GAUGE_GREEN &&
	      g_rc[2].repeat == CTUI_CAL_REPEAT_NONE);
	CHECK(g_rc[0].exception_count == 1 && g_rc[0].exceptions[0] == d11);
	CHECK(!ctui_cal_event_occurs(&g_ev[0], &g_rc[0], d11));
	CHECK(ctui_cal_event_occurs(&g_ev[0], &g_rc[0], d11 + 7));
	CHECK(ctui_cal_event_occurs(&g_ev[2], &g_rc[2], d11 + 1));
	/* the same occurrence cannot be detached twice */
	CHECK(ctui_cal_detach(g_ev, g_rc, g_sp, &count, CAP, 0, d11,
			      d11 * DM + 600, d11 * DM + 660,
			      NULL) == CTUI_CAL_DETACH_INVALID);
	/* a day that is not an occurrence, a single event, a bad range, a bad
	 * index */
	CHECK(ctui_cal_detach(g_ev, g_rc, g_sp, &count, CAP, 0, d11 + 1, 0, 60,
			      NULL) == CTUI_CAL_DETACH_INVALID);
	CHECK(ctui_cal_detach(g_ev, g_rc, g_sp, &count, CAP, 1,
			      day(2026, 11, 5), 0, 60,
			      NULL) == CTUI_CAL_DETACH_INVALID);
	CHECK(ctui_cal_detach(g_ev, g_rc, g_sp, &count, CAP, 0, d11 + 7, 100,
			      100, NULL) == CTUI_CAL_DETACH_INVALID);
	CHECK(ctui_cal_detach(g_ev, g_rc, g_sp, &count, CAP, 9, d11 + 7, 0, 60,
			      NULL) == CTUI_CAL_DETACH_INVALID);
	CHECK(count == 3 && g_rc[0].exception_count == 1);
	/* a range of several days needs spans */
	CHECK(ctui_cal_detach(g_ev, g_rc, NULL, &count, CAP, 0, d11 + 7,
			      (d11 + 7) * DM, (d11 + 9) * DM,
			      NULL) == CTUI_CAL_DETACH_INVALID);
	CHECK(ctui_cal_detach(g_ev, g_rc, g_sp, &count, CAP, 0, d11 + 7,
			      (d11 + 7) * DM + 600, (d11 + 9) * DM,
			      &out) == CTUI_CAL_DETACH_OK);
	CHECK(g_sp[out].extra_days == 1 && g_ev[out].end_minute == DM);
}

static void test_detach_full(void)
{
	ctui_cal_view v;
	series_view(&v, 2, CAP);
	int count = CAP, out = -1;
	long d11 = day(2026, 11, 11);
	/* no room in the events: nothing changes */
	CHECK(ctui_cal_detach(g_ev, g_rc, g_sp, &count, CAP, 0, d11,
			      d11 * DM + 600, d11 * DM + 660,
			      &out) == CTUI_CAL_DETACH_NO_ROOM);
	CHECK(count == CAP && out == -1 && g_rc[0].exception_count == 0);
	/* the exception list full: nothing is dropped silently */
	count = 2;
	g_rc[0].exception_count = CTUI_CAL_EXCEPTIONS;
	CHECK(ctui_cal_detach(g_ev, g_rc, g_sp, &count, CAP, 0, d11,
			      d11 * DM + 600, d11 * DM + 660,
			      &out) == CTUI_CAL_DETACH_NO_EXCEPTIONS);
	CHECK(count == 2 && g_rc[0].exception_count == CTUI_CAL_EXCEPTIONS);
	/* one place left in the list is used */
	g_rc[0].exception_count = CTUI_CAL_EXCEPTIONS - 1;
	CHECK(ctui_cal_detach(g_ev, g_rc, g_sp, &count, CAP, 0, d11,
			      d11 * DM + 600, d11 * DM + 660,
			      &out) == CTUI_CAL_DETACH_OK);
	CHECK(g_rc[0].exception_count == CTUI_CAL_EXCEPTIONS &&
	      g_rc[0].exceptions[CTUI_CAL_EXCEPTIONS - 1] == d11);
	CHECK(CTUI_CAL_EXCEPTIONS >= 8);
}

static void test_policy_default(void)
{
	ctui_cal_view v;
	series_view(&v, 2, CAP);
	v.date = (ctui_date){2026, 11, 11};
	/* without ask_scope the series moves as before */
	CHECK(!v.ask_scope);
	CHECK(ctui_cal_view_key(&v, '\n', H) == CTUI_CAL_VIEW_GRABBED);
	CHECK(ctui_cal_view_key(&v, KEY_RIGHT, H) == CTUI_CAL_VIEW_EVENT_MOVED);
	CHECK(ctui_cal_view_key(&v, '\n', H) == CTUI_CAL_VIEW_DROPPED &&
	      !v.scope_pending);
	CHECK(g_ev[0].day == day(2026, 10, 29) && v.event_count == 2);
	CHECK(ctui_cal_view_resolve_scope(&v, CTUI_CAL_SCOPE_OCCURRENCE, H) ==
	      CTUI_CAL_VIEW_NONE);
}

static void test_ask_series(void)
{
	ctui_cal_view v;
	series_view(&v, 2, CAP);
	v.ask_scope = true;
	v.date = (ctui_date){2026, 11, 11};
	ctui_cal_view_key(&v, '\n', H);
	ctui_cal_view_key(&v, KEY_RIGHT, H);
	CHECK(ctui_cal_view_key(&v, '\n', H) == CTUI_CAL_VIEW_ASK_SCOPE &&
	      v.scope_pending);
	CHECK(v.changed == 0 && v.changed_day == day(2026, 11, 12));
	/* the edit waits: keys do nothing, the series is still moved */
	CHECK(ctui_cal_view_key(&v, KEY_RIGHT, H) == CTUI_CAL_VIEW_NONE);
	CHECK(ctui_cal_view_key(&v, 'x', H) == CTUI_CAL_VIEW_NONE);
	CHECK(ctui_cal_view_key(&v, '\n', H) == CTUI_CAL_VIEW_ASK_SCOPE);
	CHECK(g_ev[0].day == day(2026, 10, 29));
	CHECK(ctui_cal_view_resolve_scope(&v, CTUI_CAL_SCOPE_SERIES, H) ==
	      CTUI_CAL_VIEW_DROPPED);
	CHECK(!v.scope_pending && v.edit.grab == CTUI_CAL_GRAB_NONE &&
	      v.event_count == 2);
	CHECK(g_ev[0].day == day(2026, 10, 29) && g_rc[0].exception_count == 0);
}

static void test_ask_occurrence_move(void)
{
	ctui_cal_view v;
	series_view(&v, 2, CAP);
	v.ask_scope = true;
	v.date = (ctui_date){2026, 11, 11};
	ctui_cal_view_key(&v, '\n', H);
	ctui_cal_view_key(&v, KEY_RIGHT, H);
	ctui_cal_view_key(&v, KEY_DOWN, H);
	CHECK(ctui_cal_view_key(&v, '\n', H) == CTUI_CAL_VIEW_ASK_SCOPE);
	CHECK(ctui_cal_view_resolve_scope(&v, CTUI_CAL_SCOPE_OCCURRENCE, H) ==
	      CTUI_CAL_VIEW_DETACHED);
	CHECK(v.event_count == 3 && v.changed == 2 && v.changed_series == 0 &&
	      v.selected == 2);
	CHECK(v.changed_day == day(2026, 11, 12));
	/* the series is as it was, minus the 11th */
	CHECK(g_ev[0].day == day(2026, 10, 28) && g_ev[0].start_minute == 600 &&
	      g_ev[0].end_minute == 660);
	CHECK(g_rc[0].repeat == CTUI_CAL_REPEAT_WEEKLY &&
	      g_rc[0].exception_count == 1 &&
	      g_rc[0].exceptions[0] == day(2026, 11, 11));
	/* the new ordinary event is the moved occurrence */
	CHECK(g_ev[2].day == day(2026, 11, 12) && g_ev[2].start_minute == 630 &&
	      g_ev[2].end_minute == 690);
	CHECK_STR(g_ev[2].title, "Weekly");
	CHECK(g_ev[2].pair == CTUI_PAIR_GAUGE_GREEN &&
	      g_rc[2].repeat == CTUI_CAL_REPEAT_NONE);
	CHECK(v.edit.grab == CTUI_CAL_GRAB_NONE && !v.scope_pending);
	CHECK(!ctui_cal_event_occurs(&g_ev[0], &g_rc[0], day(2026, 11, 11)));
	CHECK(ctui_cal_event_occurs(&g_ev[0], &g_rc[0], day(2026, 11, 18)));
}

static void test_ask_occurrence_resize(void)
{
	ctui_cal_view v;
	series_view(&v, 2, CAP);
	v.ask_scope = true;
	v.date = (ctui_date){2026, 11, 18};
	CHECK(ctui_cal_view_key(&v, 'r', H) == CTUI_CAL_VIEW_GRABBED);
	CHECK(ctui_cal_view_key(&v, KEY_DOWN, H) == CTUI_CAL_VIEW_RESIZED);
	CHECK(ctui_cal_view_key(&v, '\n', H) == CTUI_CAL_VIEW_ASK_SCOPE);
	CHECK(ctui_cal_view_resolve_scope(&v, CTUI_CAL_SCOPE_OCCURRENCE, H) ==
	      CTUI_CAL_VIEW_DETACHED);
	CHECK(g_ev[0].end_minute == 660 &&
	      g_rc[0].exceptions[0] == day(2026, 11, 18));
	CHECK(g_ev[2].day == day(2026, 11, 18) && g_ev[2].start_minute == 600 &&
	      g_ev[2].end_minute == 690);
}

static void test_ask_cancel_and_nothing(void)
{
	ctui_cal_view v;
	series_view(&v, 2, CAP);
	v.ask_scope = true;
	v.date = (ctui_date){2026, 11, 11};
	/* a drop that changed nothing does not ask */
	ctui_cal_view_key(&v, '\n', H);
	CHECK(ctui_cal_view_key(&v, '\n', H) == CTUI_CAL_VIEW_DROPPED);
	/* a plain event never asks */
	v.date = (ctui_date){2026, 11, 5};
	v.minute = 900;
	ctui_cal_view_key(&v, '\n', H);
	ctui_cal_view_key(&v, KEY_DOWN, H);
	CHECK(ctui_cal_view_key(&v, '\n', H) == CTUI_CAL_VIEW_DROPPED &&
	      g_ev[1].start_minute == 930);
	/* Esc while pending restores the series */
	v.date = (ctui_date){2026, 11, 11};
	v.minute = 600;
	ctui_cal_view_key(&v, '\n', H);
	ctui_cal_view_key(&v, KEY_LEFT, H);
	CHECK(ctui_cal_view_key(&v, '\n', H) == CTUI_CAL_VIEW_ASK_SCOPE);
	CHECK(ctui_cal_view_key(&v, 27, H) == CTUI_CAL_VIEW_CANCELLED);
	CHECK(!v.scope_pending && g_ev[0].day == day(2026, 10, 28) &&
	      v.event_count == 2);
	CHECK(g_rc[0].exception_count == 0);
}

static void test_ask_full(void)
{
	ctui_cal_view v;
	series_view(&v, 2, 2); /* no room for one more event */
	v.ask_scope = true;
	v.date = (ctui_date){2026, 11, 11};
	ctui_cal_view_key(&v, '\n', H);
	ctui_cal_view_key(&v, KEY_RIGHT, H);
	ctui_cal_view_key(&v, '\n', H);
	CHECK(ctui_cal_view_resolve_scope(&v, CTUI_CAL_SCOPE_OCCURRENCE, H) ==
	      CTUI_CAL_VIEW_FULL);
	/* nothing changed: still pending, the moved series is as it was */
	CHECK(v.scope_pending && v.event_count == 2 &&
	      g_rc[0].exception_count == 0);
	CHECK(g_ev[0].day == day(2026, 10, 29));
	CHECK(ctui_cal_view_resolve_scope(&v, CTUI_CAL_SCOPE_SERIES, H) ==
	      CTUI_CAL_VIEW_DROPPED);

	/* the exception list full */
	series_view(&v, 2, CAP);
	v.ask_scope = true;
	g_rc[0].exception_count = CTUI_CAL_EXCEPTIONS;
	for (int i = 0; i < CTUI_CAL_EXCEPTIONS; i++)
		g_rc[0].exceptions[i] =
		    day(2026, 1, 7) + 7L * i; /* Wednesdays long before */
	v.date = (ctui_date){2026, 11, 11};
	ctui_cal_view_key(&v, '\n', H);
	ctui_cal_view_key(&v, KEY_DOWN, H);
	ctui_cal_view_key(&v, '\n', H);
	CHECK(v.scope_pending);
	CHECK(ctui_cal_view_resolve_scope(&v, CTUI_CAL_SCOPE_OCCURRENCE, H) ==
	      CTUI_CAL_VIEW_FULL);
	CHECK(v.event_count == 2 &&
	      g_rc[0].exception_count == CTUI_CAL_EXCEPTIONS);
	CHECK(ctui_cal_view_cancel(&v, H) == CTUI_CAL_VIEW_CANCELLED &&
	      !v.scope_pending);
	CHECK(g_ev[0].start_minute == 600);
}

static void test_ask_mouse(void)
{
	ctui_cal_view v;
	series_view(&v, 2, CAP);
	v.ask_scope = true;
	/* Wednesday's column is x 22..28; row y holds minute 480 + 30 * (y -
	 * 1): 10:00 is row 5 */
	CHECK(ctui_cal_view_press(&v, W, H, 5, 24) == CTUI_CAL_VIEW_PICKED);
	CHECK(ctui_cal_view_drag(&v, W, H, 7, 24) == CTUI_CAL_VIEW_EVENT_MOVED);
	CHECK(ctui_cal_view_release(&v, W, H, 7, 24) ==
	      CTUI_CAL_VIEW_ASK_SCOPE);
	CHECK(v.scope_pending && !ctui_cal_view_dragging(&v));
	CHECK(ctui_cal_view_drag(&v, W, H, 8, 24) == CTUI_CAL_VIEW_NONE);
	CHECK(ctui_cal_view_resolve_scope(&v, CTUI_CAL_SCOPE_OCCURRENCE, H) ==
	      CTUI_CAL_VIEW_DETACHED);
	CHECK(g_ev[2].day == day(2026, 11, 4) && g_ev[2].start_minute == 660);
	CHECK(g_rc[0].exceptions[0] == day(2026, 11, 4) &&
	      g_ev[0].start_minute == 600);
	/* a click without a drag never asks */
	series_view(&v, 2, CAP);
	v.ask_scope = true;
	ctui_cal_view_press(&v, W, H, 5, 24);
	CHECK(ctui_cal_view_release(&v, W, H, 5, 24) == CTUI_CAL_VIEW_NONE &&
	      !v.scope_pending);
}

static void test_multi_day_occurrence(void)
{
	ctui_cal_view v;
	series_view(&v, 2, CAP);
	/* a 2-day weekly event: Wednesday 10:00 to Thursday 11:00 */
	g_ev[0].end_minute = 660;
	g_sp[0].extra_days = 1;
	v.ask_scope = true;
	v.date = (ctui_date){2026, 11, 11};
	ctui_cal_view_key(&v, '\n', H);
	ctui_cal_view_key(&v, KEY_RIGHT, H);
	CHECK(ctui_cal_view_key(&v, '\n', H) == CTUI_CAL_VIEW_ASK_SCOPE);
	CHECK(ctui_cal_view_resolve_scope(&v, CTUI_CAL_SCOPE_OCCURRENCE, H) ==
	      CTUI_CAL_VIEW_DETACHED);
	CHECK(g_ev[2].day == day(2026, 11, 12) && g_ev[2].start_minute == 600 &&
	      g_ev[2].end_minute == 660 && g_sp[2].extra_days == 1 &&
	      g_rc[2].repeat == 0);
	CHECK(g_sp[0].extra_days == 1 &&
	      g_rc[0].exceptions[0] == day(2026, 11, 11));
}

int main(void)
{
	RUN(test_detach_pure);
	RUN(test_detach_full);
	RUN(test_policy_default);
	RUN(test_ask_series);
	RUN(test_ask_occurrence_move);
	RUN(test_ask_occurrence_resize);
	RUN(test_ask_cancel_and_nothing);
	RUN(test_ask_full);
	RUN(test_ask_mouse);
	RUN(test_multi_day_occurrence);
	return CHECK_DONE();
}
