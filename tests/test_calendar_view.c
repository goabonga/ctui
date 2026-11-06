/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */
#include "check.h"
#include "screen.h"

#include <curses.h>

#include <ctui/calendar_view.h>
#include <ctui/theme.h>

#define W 62 /* axis 6 + seven columns of 8 */
#define H 11 /* header + ten rows */

static long day(int y, int m, int d)
{
	return ctui_date_to_days(y, m, d);
}

/* Tuesday 2026-11-03: A and B overlap, B and C overlap, D is alone. */
static ctui_cal_event g_events[5];

static void fill_events(void)
{
	long tue = day(2026, 11, 3);
	g_events[0] = (ctui_cal_event){tue, 540, 600, "Standup", 0};
	g_events[1] = (ctui_cal_event){tue, 570, 630, "Planning", 0};
	g_events[2] = (ctui_cal_event){tue, 600, 660, "Sync", 0};
	g_events[3] =
	    (ctui_cal_event){tue, 720, 780, "Lunch", CTUI_PAIR_GAUGE_GREEN};
	g_events[4] = (ctui_cal_event){day(2026, 11, 5), 840, 900, "Demo", 0};
}

static void week_view(ctui_cal_view *v)
{
	ctui_cal_view_init(v, CTUI_CAL_VIEW_WEEK, 2026, 11,
			   4); /* a Wednesday */
	v->top = 480;
	ctui_cal_view_set_today(v, 2026, 11, 2, 615);
	fill_events();
	ctui_cal_view_set_events(v, g_events, 5);
}

static void test_layout(void)
{
	fill_events();
	int lane[5], lanes[5];
	CHECK(ctui_cal_layout(g_events, 5, day(2026, 11, 3), lane, lanes) == 4);
	CHECK(lane[0] == 0 && lane[1] == 1 && lane[2] == 0 && lane[3] == 0);
	CHECK(lanes[0] == 2 && lanes[1] == 2 && lanes[2] == 2 && lanes[3] == 1);
	CHECK(lane[4] == -1 && lanes[4] == -1);
	CHECK(ctui_cal_layout(g_events, 5, day(2026, 11, 5), lane, lanes) ==
		  1 &&
	      lane[4] == 0 && lanes[4] == 1 && lane[0] == -1);
	CHECK(ctui_cal_layout(g_events, 5, day(2026, 11, 4), lane, lanes) == 0);

	/* three events at once take three lanes; touching ones do not overlap
	 */
	ctui_cal_event e[4] = {{7, 600, 660, "a", 0},
			       {7, 600, 660, "b", 0},
			       {7, 600, 700, "c", 0},
			       {7, 660, 720, "d", 0}};
	CHECK(ctui_cal_layout(e, 4, 7, lane, lanes) == 4);
	CHECK(lane[0] == 0 && lane[1] == 1 && lane[2] == 2 && lanes[0] == 3 &&
	      lanes[2] == 3);
	CHECK(lane[3] == 0 && lanes[3] == 3); /* c still runs until 11:40 */
	/* an event with no length lasts a minute, one past midnight is cut or
	 * ignored */
	ctui_cal_event odd[3] = {{7, 600, 600, "z", 0},
				 {7, 1400, 2000, "x", 0},
				 {7, 1440, 1500, "n", 0}};
	CHECK(ctui_cal_layout(odd, 3, 7, lane, lanes) == 2 && lane[2] == -1);
}

static void test_geometry(void)
{
	ctui_cal_view v;
	week_view(&v);
	CHECK(ctui_cal_view_columns(&v) == 7 && ctui_cal_view_rows(H) == 10 &&
	      ctui_cal_view_rows(1) == 0);
	CHECK(ctui_cal_view_first_day(&v) == day(2026, 11, 2)); /* Monday */
	v.week_start = 6;
	CHECK(ctui_cal_view_first_day(&v) == day(2026, 11, 1)); /* Sunday */
	CHECK(ctui_cal_view_day_of(&v, 3) == day(2026, 11, 4));
	v.week_start = 2; /* Wednesday: the date itself */
	CHECK(ctui_cal_view_first_day(&v) == day(2026, 11, 4));
	v.week_start = 0;
	v.mode = CTUI_CAL_VIEW_DAY;
	CHECK(ctui_cal_view_columns(&v) == 1 &&
	      ctui_cal_view_first_day(&v) == day(2026, 11, 4));

	week_view(&v);
	int y, x, h, w;
	/* Standup 09:00-10:00 in the left lane of Tuesday's column (x 14..21)
	 */
	CHECK(ctui_cal_view_event_rect(&v, W, H, 0, &y, &x, &h, &w) && y == 3 &&
	      x == 14 && h == 2 && w == 3);
	CHECK(ctui_cal_view_event_rect(&v, W, H, 1, &y, &x, &h, &w) && y == 4 &&
	      x == 17 && h == 2 && w == 4);
	CHECK(ctui_cal_view_event_rect(&v, W, H, 3, &y, &x, &h, &w) && y == 9 &&
	      x == 14 && h == 2 && w == 7);
	CHECK(!ctui_cal_view_event_rect(&v, W, H, 4, &y, &x, &h,
					&w)); /* below the shown hours */
	CHECK(!ctui_cal_view_event_rect(&v, W, H, 5, &y, &x, &h, &w) &&
	      !ctui_cal_view_event_rect(&v, W, H, -1, NULL, NULL, NULL, NULL));
	/* scrolled: Standup starts above the top and is cut */
	v.top = 555;
	v.slot_minutes = 15;
	CHECK(ctui_cal_view_event_rect(&v, W, H, 0, &y, &x, &h, &w) && y == 1 &&
	      h == 3);
	v.slot_minutes = 30;
	v.top = 600; /* Standup ended at 10:00 */
	CHECK(!ctui_cal_view_event_rect(&v, W, H, 0, &y, &x, &h, &w));
	/* a day outside the shown week */
	v.top = 480;
	v.date = (ctui_date){2026, 11, 12};
	CHECK(!ctui_cal_view_event_rect(&v, W, H, 0, &y, &x, &h, &w));
	/* too narrow */
	week_view(&v);
	CHECK(!ctui_cal_view_event_rect(&v, 10, H, 0, &y, &x, &h, &w));
}

static void test_hit(void)
{
	ctui_cal_view v;
	week_view(&v);
	long d = 0;
	int m = -1;
	CHECK(ctui_cal_view_hit(&v, W, H, 0, 6, &d, NULL) ==
		  CTUI_CAL_VIEW_HEADER &&
	      d == day(2026, 11, 2));
	CHECK(ctui_cal_view_hit(&v, W, H, 0, 61, &d, NULL) ==
		  CTUI_CAL_VIEW_HEADER &&
	      d == day(2026, 11, 8));
	CHECK(ctui_cal_view_hit(&v, W, H, 0, 2, NULL, NULL) ==
	      CTUI_CAL_VIEW_OUTSIDE);
	CHECK(ctui_cal_view_hit(&v, W, H, 3, 2, NULL, &m) ==
		  CTUI_CAL_VIEW_AXIS &&
	      m == 540);
	CHECK(ctui_cal_view_hit(&v, W, H, 1, 0, NULL, &m) ==
		  CTUI_CAL_VIEW_AXIS &&
	      m == 480);
	CHECK(ctui_cal_view_hit(&v, W, H, 3, 14, &d, &m) ==
		  CTUI_CAL_VIEW_SLOT &&
	      d == day(2026, 11, 3) && m == 540);
	CHECK(ctui_cal_view_hit(&v, W, H, 10, 13, &d, &m) ==
		  CTUI_CAL_VIEW_SLOT &&
	      d == day(2026, 11, 2) && m == 750);
	CHECK(ctui_cal_view_hit(&v, W, H, 11, 14, NULL, NULL) ==
	      CTUI_CAL_VIEW_OUTSIDE);
	CHECK(ctui_cal_view_hit(&v, W, H, 3, W, NULL, NULL) ==
	      CTUI_CAL_VIEW_OUTSIDE);
	CHECK(ctui_cal_view_hit(&v, W, H, -1, 14, NULL, NULL) ==
	      CTUI_CAL_VIEW_OUTSIDE);
	CHECK(ctui_cal_view_hit(&v, 10, H, 3, 7, NULL, NULL) ==
	      CTUI_CAL_VIEW_OUTSIDE);
	/* the last visible row may lie past midnight */
	v.top = 1140;
	CHECK(ctui_cal_view_hit(&v, W, H, 10, 14, NULL, &m) ==
		  CTUI_CAL_VIEW_SLOT &&
	      m == 1410);
	v.top = 1200;
	CHECK(ctui_cal_view_hit(&v, W, H, 10, 14, NULL, NULL) ==
	      CTUI_CAL_VIEW_OUTSIDE);
	week_view(&v);

	CHECK(ctui_cal_view_event_at(&v, W, H, 3, 14) == 0);
	CHECK(ctui_cal_view_event_at(&v, W, H, 3, 16) == 0);
	CHECK(ctui_cal_view_event_at(&v, W, H, 3, 17) == -1);
	CHECK(ctui_cal_view_event_at(&v, W, H, 4, 17) == 1);
	CHECK(ctui_cal_view_event_at(&v, W, H, 5, 14) == 2);
	CHECK(ctui_cal_view_event_at(&v, W, H, 7, 14) ==
	      -1); /* 11:00 is free */
	CHECK(ctui_cal_view_event_at(&v, W, H, 9, 20) == 3);
	CHECK(ctui_cal_view_event_at(&v, W, H, 9, 21) ==
	      -1); /* the separator */
	CHECK(ctui_cal_view_event_at(&v, W, H, 2, 14) == -1);
	CHECK(ctui_cal_view_event_at(&v, W, H, 0, 14) == -1);
	CHECK(ctui_cal_view_event_at(&v, W, H, 3, 6) ==
	      -1); /* Monday has none */

	/* day view: one wide column */
	v.mode = CTUI_CAL_VIEW_DAY;
	v.date = (ctui_date){2026, 11, 3};
	CHECK(ctui_cal_view_hit(&v, 40, H, 0, 39, &d, NULL) ==
		  CTUI_CAL_VIEW_HEADER &&
	      d == day(2026, 11, 3));
	CHECK(ctui_cal_view_hit(&v, 40, H, 3, 39, &d, &m) ==
		  CTUI_CAL_VIEW_SLOT &&
	      m == 540);
	CHECK(ctui_cal_view_event_at(&v, 40, H, 3, 6) == 0 &&
	      ctui_cal_view_event_at(&v, 40, H, 4, 23) == 1);
	CHECK(ctui_cal_view_event_at(&v, 40, H, 4, 22) == 0 &&
	      ctui_cal_view_event_at(&v, 40, H, 5, 6) == 2);
}

static void test_keys(void)
{
	ctui_cal_view v;
	week_view(&v);
	ctui_cal_view_reveal(&v, H);
	CHECK(v.date.day == 4 && v.minute == 480 && v.top == 480);
	CHECK(ctui_cal_view_key(&v, KEY_RIGHT, H) == CTUI_CAL_VIEW_DATE &&
	      v.date.day == 5);
	CHECK(ctui_cal_view_key(&v, 'h', H) == CTUI_CAL_VIEW_DATE &&
	      v.date.day == 4);
	CHECK(ctui_cal_view_key(&v, KEY_DOWN, H) == CTUI_CAL_VIEW_MOVED &&
	      v.minute == 510);
	CHECK(ctui_cal_view_key(&v, 'k', H) == CTUI_CAL_VIEW_MOVED &&
	      v.minute == 480);
	CHECK(ctui_cal_view_key(&v, KEY_UP, H) == CTUI_CAL_VIEW_MOVED &&
	      v.minute == 450 && v.top == 450);
	for (int i = 0; i < 20; i++)
		ctui_cal_view_key(&v, KEY_DOWN, H);
	CHECK(v.minute == 1050 &&
	      v.top == 1050 - 9 * 30); /* the cursor is the last row */
	CHECK(ctui_cal_view_key(&v, KEY_NPAGE, H) == CTUI_CAL_VIEW_DATE &&
	      v.date.day == 11 && v.minute == 1050);
	CHECK(ctui_cal_view_key(&v, KEY_PPAGE, H) == CTUI_CAL_VIEW_DATE &&
	      v.date.day == 4);
	CHECK(ctui_cal_view_key(&v, '\n', H) == CTUI_CAL_VIEW_ACTIVATED);
	CHECK(ctui_cal_view_key(&v, KEY_ENTER, H) == CTUI_CAL_VIEW_ACTIVATED);
	CHECK(ctui_cal_view_key(&v, 'x', H) == CTUI_CAL_VIEW_NONE);
	/* Home goes to today and to the time of day */
	CHECK(ctui_cal_view_key(&v, KEY_HOME, H) == CTUI_CAL_VIEW_DATE &&
	      v.date.day == 2 && v.minute == 600);
	CHECK(v.top <= 600 && v.top + 300 > 600);
	CHECK(ctui_cal_view_key(&v, KEY_HOME, H) == CTUI_CAL_VIEW_NONE);
	ctui_cal_view_set_today(&v, 0, 0, 0, -1);
	CHECK(v.today.year == 0 && v.now_minute == -1 &&
	      ctui_cal_view_key(&v, KEY_HOME, H) == CTUI_CAL_VIEW_NONE);
	/* the day ends at midnight and begins at 00:00 */
	v.minute = 1410;
	CHECK(ctui_cal_view_key(&v, KEY_DOWN, H) == CTUI_CAL_VIEW_NONE &&
	      v.minute == 1410);
	v.minute = 0;
	CHECK(ctui_cal_view_key(&v, KEY_UP, H) == CTUI_CAL_VIEW_NONE);
	/* in the day view PgUp / PgDn move one day */
	v.mode = CTUI_CAL_VIEW_DAY;
	CHECK(ctui_cal_view_key(&v, KEY_NPAGE, H) == CTUI_CAL_VIEW_DATE &&
	      v.date.day == 3);
	CHECK(ctui_cal_view_key(&v, KEY_PPAGE, H) == CTUI_CAL_VIEW_DATE &&
	      v.date.day == 2);
	/* a day crosses months and years */
	ctui_cal_view_init(&v, CTUI_CAL_VIEW_DAY, 2026, 12, 31);
	CHECK(ctui_cal_view_key(&v, KEY_RIGHT, H) == CTUI_CAL_VIEW_DATE &&
	      v.date.year == 2027 && v.date.month == 1 && v.date.day == 1);
	ctui_cal_view_init(&v, CTUI_CAL_VIEW_DAY, 1, 1, 1);
	CHECK(ctui_cal_view_key(&v, KEY_LEFT, H) == CTUI_CAL_VIEW_NONE &&
	      v.date.year == 1);
	ctui_cal_view_init(&v, CTUI_CAL_VIEW_DAY, 9999, 12, 31);
	CHECK(ctui_cal_view_key(&v, KEY_RIGHT, H) == CTUI_CAL_VIEW_NONE &&
	      v.date.year == 9999);
}

static void test_state(void)
{
	ctui_cal_view v;
	ctui_cal_view_init(&v, CTUI_CAL_VIEW_WEEK, 2026, 2, 30);
	CHECK(v.date.year == 1970 && v.slot_minutes == 30 && v.selected == -1 &&
	      v.now_minute == -1);
	ctui_cal_view_init(&v, CTUI_CAL_VIEW_WEEK, 2026, 11, 4);
	CHECK(v.date.year == 2026 && v.minute == 480 && v.event_count == 0);
	CHECK(ctui_cal_view_goto(&v, 2026, 11, 4, -1) == CTUI_CAL_VIEW_NONE);
	CHECK(ctui_cal_view_goto(&v, 2026, 11, 4, 520) == CTUI_CAL_VIEW_MOVED &&
	      v.minute == 510);
	CHECK(ctui_cal_view_goto(&v, 2026, 11, 31, 0) == CTUI_CAL_VIEW_NONE &&
	      v.date.day == 4);
	CHECK(ctui_cal_view_goto(&v, 2026, 11, 9, 5000) == CTUI_CAL_VIEW_DATE &&
	      v.minute == 1410);
	ctui_cal_view_set_today(&v, 2026, 2, 30, 100);
	CHECK(v.today.year == 0);
	ctui_cal_view_set_today(&v, 2026, 11, 2, 1440);
	CHECK(v.today.year == 2026 && v.now_minute == -1);
	ctui_cal_view_set_events(&v, NULL, 4);
	CHECK(v.event_count == 0);
	/* reveal rounds to the slot and keeps the cursor on screen */
	v.slot_minutes = 60;
	v.minute = 545;
	v.top = 130;
	ctui_cal_view_reveal(&v, H);
	CHECK(v.minute == 540 && v.top == 120);
	v.slot_minutes = 0; /* falls back to 30 */
	v.minute = 1439;
	ctui_cal_view_reveal(&v, H);
	CHECK(v.minute == 1410 && v.top == 1140);
	ctui_cal_view_reveal(&v, 0);
	CHECK(v.top >= 0 && v.top <= 1140);
}

static void test_mouse(void)
{
	ctui_cal_view v;
	week_view(&v);
	CHECK(ctui_cal_view_press(&v, W, H, 3, 14) == CTUI_CAL_VIEW_PICKED &&
	      v.selected == 0);
	CHECK(v.date.day == 3 && v.minute == 540);
	CHECK(ctui_cal_view_press(&v, W, H, 4, 18) == CTUI_CAL_VIEW_PICKED &&
	      v.selected == 1 && v.minute == 570);
	/* an empty cell moves the cursor and clears the pick */
	CHECK(ctui_cal_view_press(&v, W, H, 7, 14) == CTUI_CAL_VIEW_MOVED &&
	      v.selected == -1 && v.minute == 660);
	CHECK(ctui_cal_view_press(&v, W, H, 7, 30) == CTUI_CAL_VIEW_DATE &&
	      v.date.day == 5);
	CHECK(ctui_cal_view_press(&v, W, H, 7, 30) == CTUI_CAL_VIEW_NONE);
	/* a header changes the day only; an hour label changes the time only */
	CHECK(ctui_cal_view_press(&v, W, H, 0, 6) == CTUI_CAL_VIEW_DATE &&
	      v.date.day == 2 && v.minute == 660);
	CHECK(ctui_cal_view_press(&v, W, H, 2, 1) == CTUI_CAL_VIEW_MOVED &&
	      v.minute == 510 && v.date.day == 2);
	CHECK(ctui_cal_view_press(&v, W, H, 0, 1) == CTUI_CAL_VIEW_NONE);
	CHECK(ctui_cal_view_press(&v, W, H, 20, 20) == CTUI_CAL_VIEW_NONE);
	/* a click on the last visible row never scrolls beyond what is shown */
	CHECK(ctui_cal_view_press(&v, W, H, 10, 7) == CTUI_CAL_VIEW_MOVED &&
	      v.top == 480);

	/* the wheel scrolls the hours and keeps the cursor */
	int minute = v.minute;
	CHECK(ctui_cal_view_wheel(&v, H, 2) == CTUI_CAL_VIEW_SCROLLED &&
	      v.top == 540 && v.minute == minute);
	CHECK(ctui_cal_view_wheel(&v, H, -1) == CTUI_CAL_VIEW_SCROLLED &&
	      v.top == 510);
	CHECK(ctui_cal_view_wheel(&v, H, -100) == CTUI_CAL_VIEW_SCROLLED &&
	      v.top == 0);
	CHECK(ctui_cal_view_wheel(&v, H, -1) == CTUI_CAL_VIEW_NONE);
	CHECK(ctui_cal_view_wheel(&v, H, 1000) == CTUI_CAL_VIEW_SCROLLED &&
	      v.top == 1140);
	CHECK(ctui_cal_view_wheel(&v, H, 1) == CTUI_CAL_VIEW_NONE);
}

static void test_edit_helpers(void)
{
	/* parts of a block */
	CHECK(ctui_cal_edge_of(0, 1, false, false) == CTUI_CAL_EDGE_BODY);
	CHECK(ctui_cal_edge_of(0, 2, false, false) == CTUI_CAL_EDGE_BODY);
	CHECK(ctui_cal_edge_of(1, 2, false, false) == CTUI_CAL_EDGE_BOTTOM);
	CHECK(ctui_cal_edge_of(0, 4, false, false) == CTUI_CAL_EDGE_TOP);
	CHECK(ctui_cal_edge_of(1, 4, false, false) == CTUI_CAL_EDGE_BODY);
	CHECK(ctui_cal_edge_of(3, 4, false, false) == CTUI_CAL_EDGE_BOTTOM);
	CHECK(ctui_cal_edge_of(0, 4, true, false) == CTUI_CAL_EDGE_BODY);
	CHECK(ctui_cal_edge_of(3, 4, false, true) == CTUI_CAL_EDGE_BODY);
	CHECK(ctui_cal_edge_of(4, 4, false, false) == CTUI_CAL_EDGE_NONE);
	CHECK(ctui_cal_edge_of(-1, 4, false, false) == CTUI_CAL_EDGE_NONE);

	/* hit testing on the view: a lone two-hour event on Wednesday
	 * 09:00-11:00 */
	ctui_cal_view v;
	ctui_cal_event e[1] = {{day(2026, 11, 4), 540, 660, "Long", 0}};
	ctui_cal_view_init(&v, CTUI_CAL_VIEW_WEEK, 2026, 11, 4);
	v.top = 480;
	ctui_cal_view_set_events(&v, e, 1);
	int idx = -1;
	long d = 0;
	/* Wednesday is column 2: x 22..28; rows 3..6 */
	CHECK(ctui_cal_view_event_part(&v, W, H, 3, 22, &idx, &d) ==
		  CTUI_CAL_EDGE_TOP &&
	      idx == 0 && d == day(2026, 11, 4));
	CHECK(ctui_cal_view_event_part(&v, W, H, 4, 25, NULL, NULL) ==
	      CTUI_CAL_EDGE_BODY);
	CHECK(ctui_cal_view_event_part(&v, W, H, 5, 25, NULL, NULL) ==
	      CTUI_CAL_EDGE_BODY);
	CHECK(ctui_cal_view_event_part(&v, W, H, 6, 28, NULL, NULL) ==
	      CTUI_CAL_EDGE_BOTTOM);
	CHECK(ctui_cal_view_event_part(&v, W, H, 7, 25, NULL, NULL) ==
	      CTUI_CAL_EDGE_NONE);
	CHECK(ctui_cal_view_event_part(&v, W, H, 3, 21, NULL, NULL) ==
	      CTUI_CAL_EDGE_NONE);
	CHECK(ctui_cal_view_event_part(&v, W, H, 0, 25, NULL, NULL) ==
	      CTUI_CAL_EDGE_NONE);
	/* scrolled so that the start is above the top: the top row is body */
	v.top = 570;
	CHECK(ctui_cal_view_event_part(&v, W, H, 1, 25, NULL, NULL) ==
	      CTUI_CAL_EDGE_BODY);
	CHECK(ctui_cal_view_event_part(&v, W, H, 2, 25, NULL, NULL) ==
	      CTUI_CAL_EDGE_BODY);
	CHECK(ctui_cal_view_event_part(&v, W, H, 3, 25, NULL, NULL) ==
	      CTUI_CAL_EDGE_BOTTOM);

	/* move: snapped down, duration kept, inside the day and the days */
	long nd;
	int ns;
	ctui_cal_move_clamp(60, 100, 605, 30, 98, 104, &nd, &ns);
	CHECK(nd == 100 && ns == 600);
	ctui_cal_move_clamp(60, 90, -50, 30, 98, 104, &nd, &ns);
	CHECK(nd == 98 && ns == 0);
	ctui_cal_move_clamp(60, 120, 1439, 30, 98, 104, &nd, &ns);
	CHECK(nd == 104 && ns == 1380); /* ends at midnight, not past it */
	ctui_cal_move_clamp(0, 100, 1439, 30, 98, 104, &nd, &ns);
	CHECK(ns ==
	      1410); /* a duration below one minute is one minute: snapped */
	ctui_cal_move_clamp(2000, 100, 500, 30, 98, 104, &nd, &ns);
	CHECK(ns == 0);
	ctui_cal_move_clamp(60, 100, 600, 30, 98, 104, NULL, NULL);

	/* resize */
	ctui_cal_event r = {1, 540, 660, "r", 0};
	CHECK(ctui_cal_event_resize(&r, false, 700, 30) &&
	      r.start_minute == 540 && r.end_minute == 690);
	CHECK(!ctui_cal_event_resize(&r, false, 690, 30));
	CHECK(ctui_cal_event_resize(&r, false, 500, 30) &&
	      r.end_minute == 570); /* one slot at least */
	CHECK(ctui_cal_event_resize(&r, false, 5000, 30) &&
	      r.end_minute == 1440);
	CHECK(ctui_cal_event_resize(&r, true, 100, 30) &&
	      r.start_minute == 90 && r.end_minute == 1440);
	CHECK(ctui_cal_event_resize(&r, true, 1430, 30) &&
	      r.start_minute == 1410);
	CHECK(ctui_cal_event_resize(&r, true, -40, 30) && r.start_minute == 0);
	r = (ctui_cal_event){1, 1425, 1440, "r",
			     0}; /* shorter than a slot at midnight */
	CHECK(!ctui_cal_event_resize(&r, false, 1440, 30) &&
	      r.end_minute == 1440);
	CHECK(ctui_cal_event_resize(&r, true, 1425, 30) &&
	      r.start_minute == 1410);
	r = (ctui_cal_event){1, 10, 20, "r",
			     0}; /* shorter than a slot at the top */
	CHECK(ctui_cal_event_resize(&r, true, 15, 30) && r.start_minute == 0 &&
	      r.end_minute == 20);
	r = (ctui_cal_event){1, 600, 600, "r",
			     0}; /* no length: one minute, then a slot */
	CHECK(ctui_cal_event_resize(&r, false, 600, 30) && r.end_minute == 630);
	CHECK(!ctui_cal_event_resize(&(ctui_cal_event){1, 1500, 1600, "r", 0},
				     false, 600, 30));

	/* a dragged range covers whole slots, either way */
	int s, t;
	CHECK(ctui_cal_range(600, 660, 30, &s, &t) && s == 600 && t == 690);
	CHECK(ctui_cal_range(675, 600, 30, &s, &t) && s == 600 && t == 690);
	CHECK(ctui_cal_range(610, 620, 30, &s, &t) && s == 600 && t == 630);
	CHECK(ctui_cal_range(1439, 1400, 30, &s, &t) && s == 1380 && t == 1440);
	CHECK(!ctui_cal_range(-1, 10, 30, &s, &t) &&
	      !ctui_cal_range(10, 1440, 30, &s, &t));
}

/* An editable week view of 2026-11-04 (Wednesday) with room for `cap` events.
 */
static ctui_cal_event g_ed[6];
static ctui_cal_recur g_edr[6];

static void editable_view(ctui_cal_view *v, int count, int cap)
{
	ctui_cal_view_init(v, CTUI_CAL_VIEW_WEEK, 2026, 11, 4);
	v->top = 480;
	v->minute = 600;
	ctui_cal_view_set_editable(v, g_ed, g_edr, count, cap);
}

static void test_edit_create_delete(void)
{
	ctui_cal_view v;
	g_ed[0] = (ctui_cal_event){day(2026, 11, 4), 540, 570, "A", 0};
	g_ed[1] = (ctui_cal_event){day(2026, 11, 4), 600, 660, "B", 0};
	g_ed[2] = (ctui_cal_event){day(2026, 11, 5), 600, 660, "C", 0};
	for (int i = 0; i < 6; i++)
		g_edr[i] = (ctui_cal_recur){0};
	editable_view(&v, 3, 4);
	CHECK(ctui_cal_view_event_under_cursor(&v) == 1);

	/* a new event: one slot at the cursor; the caller names it */
	v.minute = 720;
	CHECK(ctui_cal_view_key(&v, 'a', H) == CTUI_CAL_VIEW_CREATED &&
	      v.event_count == 4);
	CHECK(v.changed == 3 && v.changed_day == day(2026, 11, 4) &&
	      v.selected == 3);
	CHECK(g_ed[3].day == day(2026, 11, 4) && g_ed[3].start_minute == 720 &&
	      g_ed[3].end_minute == 750 && g_ed[3].title == NULL);
	CHECK(ctui_cal_view_key(&v, 'n', H) == CTUI_CAL_VIEW_FULL &&
	      v.event_count == 4);

	/* the last slot of the day is one slot, not past midnight */
	v.event_capacity = 6;
	v.minute = 1410;
	CHECK(ctui_cal_view_key(&v, 'n', H) == CTUI_CAL_VIEW_CREATED &&
	      g_ed[4].end_minute == 1440);

	/* delete asks first, then removes and shifts the others down */
	v.minute = 600;
	CHECK(ctui_cal_view_key(&v, 'x', H) == CTUI_CAL_VIEW_DELETE_ASKED &&
	      v.event_count == 5);
	CHECK(v.selected == 1 && v.changed == 1);
	CHECK(ctui_cal_view_delete(&v, false) == CTUI_CAL_VIEW_DELETED &&
	      v.event_count == 4);
	CHECK(v.changed == 1 && g_ed[1].title && g_ed[1].title[0] == 'C' &&
	      v.selected == -1);
	CHECK(ctui_cal_view_key(&v, KEY_DC, H) ==
	      CTUI_CAL_VIEW_NONE); /* nothing under the cursor */
	CHECK(ctui_cal_view_delete(&v, false) == CTUI_CAL_VIEW_NONE);
	v.minute = 540;
	CHECK(ctui_cal_view_key(&v, KEY_DC, H) == CTUI_CAL_VIEW_DELETE_ASKED);
	CHECK(ctui_cal_view_key(&v, 'X', H) ==
	      CTUI_CAL_VIEW_DELETE_SERIES_ASKED);

	/* a view that is not editable ignores the editing keys; Enter activates
	 */
	ctui_cal_view p;
	ctui_cal_view_init(&p, CTUI_CAL_VIEW_WEEK, 2026, 11, 4);
	ctui_cal_view_set_events(&p, g_ed, 3);
	CHECK(ctui_cal_view_key(&p, 'a', H) == CTUI_CAL_VIEW_NONE &&
	      p.event_count == 3);
	CHECK(ctui_cal_view_key(&p, '\n', H) == CTUI_CAL_VIEW_ACTIVATED);
	CHECK(ctui_cal_view_create(&p, H) == CTUI_CAL_VIEW_NONE);
	editable_view(&v, 3, 4);
	v.minute = 900; /* nothing here: Enter still activates */
	CHECK(ctui_cal_view_key(&v, '\n', H) == CTUI_CAL_VIEW_ACTIVATED);
	ctui_cal_view_set_events(&v, g_ed, 3); /* plain events end editing */
	CHECK(ctui_cal_view_key(&v, 'a', H) == CTUI_CAL_VIEW_NONE);
}

static void test_edit_move(void)
{
	ctui_cal_view v;
	g_ed[0] = (ctui_cal_event){day(2026, 11, 4), 600, 660, "B", 0};
	for (int i = 0; i < 6; i++)
		g_edr[i] = (ctui_cal_recur){0};
	editable_view(&v, 1, 2);
	CHECK(ctui_cal_view_key(&v, '\n', H) == CTUI_CAL_VIEW_GRABBED &&
	      v.edit.grab == CTUI_CAL_GRAB_MOVE);
	CHECK(ctui_cal_view_key(&v, 'a', H) == CTUI_CAL_VIEW_NONE &&
	      v.event_count == 1); /* ignored */
	CHECK(ctui_cal_view_key(&v, KEY_DOWN, H) == CTUI_CAL_VIEW_EVENT_MOVED);
	CHECK(g_ed[0].start_minute == 630 && g_ed[0].end_minute == 690 &&
	      v.minute == 630);
	CHECK(ctui_cal_view_key(&v, 'k', H) == CTUI_CAL_VIEW_EVENT_MOVED &&
	      g_ed[0].start_minute == 600);
	CHECK(ctui_cal_view_key(&v, KEY_RIGHT, H) == CTUI_CAL_VIEW_EVENT_MOVED);
	CHECK(g_ed[0].day == day(2026, 11, 5) && v.date.day == 5 &&
	      v.changed_day == day(2026, 11, 5));
	CHECK(ctui_cal_view_key(&v, 'l', H) == CTUI_CAL_VIEW_EVENT_MOVED);
	CHECK(ctui_cal_view_key(&v, 'l', H) == CTUI_CAL_VIEW_EVENT_MOVED);
	CHECK(ctui_cal_view_key(&v, 'l', H) ==
	      CTUI_CAL_VIEW_EVENT_MOVED); /* Sunday */
	CHECK(ctui_cal_view_key(&v, 'l', H) ==
	      CTUI_CAL_VIEW_NONE); /* stays in the week */
	CHECK(g_ed[0].day == day(2026, 11, 8));
	CHECK(ctui_cal_view_key(&v, 27, H) == CTUI_CAL_VIEW_CANCELLED &&
	      v.edit.grab == CTUI_CAL_GRAB_NONE);
	CHECK(g_ed[0].day == day(2026, 11, 4) && g_ed[0].start_minute == 600 &&
	      g_ed[0].end_minute == 660);
	CHECK(v.date.day == 4 && v.minute == 600);

	/* drop keeps it */
	ctui_cal_view_key(&v, 'm', H);
	ctui_cal_view_key(&v, KEY_LEFT, H);
	CHECK(ctui_cal_view_key(&v, '\n', H) == CTUI_CAL_VIEW_DROPPED &&
	      g_ed[0].day == day(2026, 11, 3));
	CHECK(ctui_cal_view_cancel(&v, H) == CTUI_CAL_VIEW_NONE &&
	      ctui_cal_view_drop(&v) == CTUI_CAL_VIEW_NONE);

	/* never past midnight, never before it, an odd start snaps */
	g_ed[0] = (ctui_cal_event){day(2026, 11, 4), 1380, 1440, "L", 0};
	v.date = (ctui_date){2026, 11, 4};
	v.minute = 1380;
	ctui_cal_view_key(&v, 'm', H);
	CHECK(ctui_cal_view_key(&v, KEY_DOWN, H) == CTUI_CAL_VIEW_NONE &&
	      g_ed[0].end_minute == 1440);
	CHECK(ctui_cal_view_key(&v, KEY_UP, H) == CTUI_CAL_VIEW_EVENT_MOVED &&
	      g_ed[0].start_minute == 1350);
	ctui_cal_view_key(&v, 27, H);
	g_ed[0] = (ctui_cal_event){day(2026, 11, 4), 20, 80, "S", 0};
	v.minute = 0;
	ctui_cal_view_key(&v, 'm', H);
	CHECK(ctui_cal_view_key(&v, KEY_UP, H) == CTUI_CAL_VIEW_EVENT_MOVED &&
	      g_ed[0].start_minute == 0 && g_ed[0].end_minute == 60);
	CHECK(ctui_cal_view_key(&v, KEY_UP, H) == CTUI_CAL_VIEW_NONE);
	ctui_cal_view_key(&v, 27, H);
	CHECK(g_ed[0].start_minute == 20 && g_ed[0].end_minute == 80);

	/* the day view has one column: no day moves */
	editable_view(&v, 1, 2);
	g_ed[0] = (ctui_cal_event){day(2026, 11, 4), 600, 660, "B", 0};
	v.mode = CTUI_CAL_VIEW_DAY;
	ctui_cal_view_key(&v, '\n', H);
	CHECK(ctui_cal_view_key(&v, KEY_RIGHT, H) == CTUI_CAL_VIEW_NONE &&
	      g_ed[0].day == day(2026, 11, 4));
}

static void test_edit_resize(void)
{
	ctui_cal_view v;
	g_ed[0] = (ctui_cal_event){day(2026, 11, 4), 600, 660, "B", 0};
	for (int i = 0; i < 6; i++)
		g_edr[i] = (ctui_cal_recur){0};
	editable_view(&v, 1, 2);
	CHECK(ctui_cal_view_key(&v, 'r', H) == CTUI_CAL_VIEW_GRABBED &&
	      v.edit.grab == CTUI_CAL_GRAB_RESIZE);
	CHECK(ctui_cal_view_key(&v, KEY_DOWN, H) == CTUI_CAL_VIEW_RESIZED &&
	      g_ed[0].end_minute == 690);
	CHECK(ctui_cal_view_key(&v, KEY_UP, H) == CTUI_CAL_VIEW_RESIZED &&
	      g_ed[0].end_minute == 660);
	CHECK(ctui_cal_view_key(&v, 'k', H) == CTUI_CAL_VIEW_RESIZED &&
	      g_ed[0].end_minute == 630);
	CHECK(ctui_cal_view_key(&v, 'k', H) == CTUI_CAL_VIEW_NONE &&
	      g_ed[0].end_minute == 630); /* 1 slot */
	CHECK(ctui_cal_view_key(&v, '\t', H) == CTUI_CAL_VIEW_GRABBED &&
	      v.edit.top);
	CHECK(ctui_cal_view_key(&v, KEY_UP, H) == CTUI_CAL_VIEW_RESIZED &&
	      g_ed[0].start_minute == 570 && g_ed[0].end_minute == 630 &&
	      v.minute == 570);
	CHECK(ctui_cal_view_key(&v, KEY_DOWN, H) == CTUI_CAL_VIEW_RESIZED &&
	      g_ed[0].start_minute == 600);
	CHECK(ctui_cal_view_key(&v, KEY_DOWN, H) == CTUI_CAL_VIEW_NONE);
	CHECK(ctui_cal_view_key(&v, 27, H) == CTUI_CAL_VIEW_CANCELLED);
	CHECK(g_ed[0].start_minute == 600 && g_ed[0].end_minute == 660 &&
	      v.minute == 600);

	ctui_cal_view_key(&v, 'r', H);
	ctui_cal_view_key(&v, KEY_DOWN, H);
	CHECK(ctui_cal_view_key(&v, 'r', H) == CTUI_CAL_VIEW_DROPPED &&
	      g_ed[0].end_minute == 690);

	/* Shift+Up / Shift+Down move the end with no mode; an odd end snaps */
	CHECK(ctui_cal_view_key(&v, KEY_SF, H) == CTUI_CAL_VIEW_RESIZED &&
	      g_ed[0].end_minute == 720);
	CHECK(ctui_cal_view_key(&v, KEY_SR, H) == CTUI_CAL_VIEW_RESIZED &&
	      g_ed[0].end_minute == 690);
	g_ed[0].end_minute = 675;
	CHECK(ctui_cal_view_key(&v, KEY_SR, H) == CTUI_CAL_VIEW_RESIZED &&
	      g_ed[0].end_minute == 660);
	g_ed[0].end_minute = 675;
	CHECK(ctui_cal_view_key(&v, KEY_SF, H) == CTUI_CAL_VIEW_RESIZED &&
	      g_ed[0].end_minute == 690);
	g_ed[0].end_minute = 1440;
	CHECK(ctui_cal_view_key(&v, KEY_SF, H) == CTUI_CAL_VIEW_NONE);
	v.minute = 480; /* before the event */
	CHECK(ctui_cal_view_key(&v, KEY_SF, H) == CTUI_CAL_VIEW_NONE);
	CHECK(ctui_cal_view_key(&v, 'r', H) == CTUI_CAL_VIEW_NONE);
}

static void test_edit_series(void)
{
	ctui_cal_view v;
	/* weekly Wednesdays from 2026-10-28, until 11-25, with an exception on
	 * 11-18 */
	g_ed[0] = (ctui_cal_event){day(2026, 10, 28), 600, 660, "W", 0};
	g_edr[0] = (ctui_cal_recur){.repeat = CTUI_CAL_REPEAT_WEEKLY,
				    .interval = 1,
				    .end = CTUI_CAL_END_UNTIL,
				    .until = day(2026, 11, 25),
				    .exceptions = {day(2026, 11, 18)},
				    .exception_count = 1};
	editable_view(&v, 1, 3);
	/* delete on an occurrence: only that day is left out */
	CHECK(ctui_cal_view_key(&v, 'x', H) == CTUI_CAL_VIEW_DELETE_ASKED);
	CHECK(ctui_cal_view_delete(&v, false) == CTUI_CAL_VIEW_EXCEPTED &&
	      v.event_count == 1);
	CHECK(g_edr[0].exception_count == 2 &&
	      g_edr[0].exceptions[1] == day(2026, 11, 4));
	CHECK(!ctui_cal_event_occurs(&g_ed[0], &g_edr[0], day(2026, 11, 4)));
	CHECK(ctui_cal_event_occurs(&g_ed[0], &g_edr[0], day(2026, 11, 11)));
	CHECK(ctui_cal_view_event_under_cursor(&v) == -1);
	/* a full exception table reports it */
	g_edr[0].exception_count = CTUI_CAL_EXCEPTIONS;
	v.date = (ctui_date){2026, 11, 11};
	CHECK(ctui_cal_view_delete(&v, false) == CTUI_CAL_VIEW_FULL);
	g_edr[0].exception_count = 2;

	/* moving an occurrence moves the series: the days, the end and the
	 * exceptions */
	v.date = (ctui_date){2026, 11, 11};
	CHECK(ctui_cal_view_key(&v, '\n', H) == CTUI_CAL_VIEW_GRABBED);
	CHECK(ctui_cal_view_key(&v, KEY_RIGHT, H) == CTUI_CAL_VIEW_EVENT_MOVED);
	CHECK(g_ed[0].day == day(2026, 10, 29) &&
	      g_edr[0].until == day(2026, 11, 26) &&
	      g_edr[0].exceptions[1] == day(2026, 11, 5));
	CHECK(ctui_cal_event_occurs(&g_ed[0], &g_edr[0], day(2026, 11, 12)));
	CHECK(ctui_cal_view_key(&v, KEY_DOWN, H) == CTUI_CAL_VIEW_EVENT_MOVED);
	CHECK(g_ed[0].start_minute == 630);
	CHECK(ctui_cal_view_key(&v, 27, H) == CTUI_CAL_VIEW_CANCELLED);
	CHECK(g_ed[0].day == day(2026, 10, 28) &&
	      g_edr[0].until == day(2026, 11, 25) &&
	      g_edr[0].exceptions[1] == day(2026, 11, 4) &&
	      g_ed[0].start_minute == 600);

	/* resizing changes every occurrence */
	ctui_cal_view_key(&v, 'r', H);
	CHECK(ctui_cal_view_key(&v, KEY_DOWN, H) == CTUI_CAL_VIEW_RESIZED);
	ctui_cal_view_key(&v, '\n', H);
	CHECK(g_ed[0].end_minute == 690);

	/* X removes the whole series */
	CHECK(ctui_cal_view_key(&v, 'X', H) ==
	      CTUI_CAL_VIEW_DELETE_SERIES_ASKED);
	CHECK(ctui_cal_view_delete(&v, true) == CTUI_CAL_VIEW_DELETED &&
	      v.event_count == 0);
}

/* Wednesday's column is x 22..28; row y holds minute 480 + 30 * (y - 1). */
static void mouse_view(ctui_cal_view *v, int count, int cap)
{
	g_ed[0] = (ctui_cal_event){day(2026, 11, 4), 600, 720, "B",
				   0}; /* rows 5..8 */
	for (int i = 0; i < 6; i++)
		g_edr[i] = (ctui_cal_recur){0};
	editable_view(v, count, cap);
}

static void test_edit_mouse(void)
{
	ctui_cal_view v;
	mouse_view(&v, 1, 3);

	/* press the body, drag a row down and a column right, release */
	CHECK(ctui_cal_view_press(&v, W, H, 6, 24) == CTUI_CAL_VIEW_PICKED &&
	      v.selected == 0);
	CHECK(v.edit.grab == CTUI_CAL_GRAB_MOVE && v.edit.mouse);
	CHECK(ctui_cal_view_drag(&v, W, H, 8, 32) == CTUI_CAL_VIEW_EVENT_MOVED);
	CHECK(g_ed[0].day == day(2026, 11, 5) && g_ed[0].start_minute == 660 &&
	      g_ed[0].end_minute ==
		  780); /* the grab kept its row: 30 minutes into the event */
	CHECK(ctui_cal_view_drag(&v, W, H, 8, 32) == CTUI_CAL_VIEW_NONE);
	CHECK(ctui_cal_view_key(&v, 'a', H) ==
	      CTUI_CAL_VIEW_NONE); /* keys are ignored meanwhile */
	CHECK(ctui_cal_view_release(&v, W, H, 8, 32) == CTUI_CAL_VIEW_DROPPED);
	CHECK(v.edit.grab == CTUI_CAL_GRAB_NONE && g_ed[0].start_minute == 660);
	CHECK(ctui_cal_view_release(&v, W, H, 8, 32) == CTUI_CAL_VIEW_NONE);

	/* a click without a drag changes nothing */
	mouse_view(&v, 1, 3);
	ctui_cal_view_press(&v, W, H, 6, 24);
	CHECK(ctui_cal_view_release(&v, W, H, 6, 24) == CTUI_CAL_VIEW_NONE &&
	      g_ed[0].start_minute == 600);

	/* a move stops at the edges of the week and of the day; outside the box
	 * is clamped */
	ctui_cal_view_press(&v, W, H, 6, 24);
	ctui_cal_view_drag(&v, W, H, 500, 500);
	CHECK(g_ed[0].day == day(2026, 11, 8) && g_ed[0].end_minute <= 1440);
	ctui_cal_view_drag(&v, W, H, -9, -9);
	CHECK(g_ed[0].day == day(2026, 11, 2) && g_ed[0].start_minute >= 0);
	/* right button or Esc cancels */
	CHECK(ctui_cal_view_cancel(&v, H) == CTUI_CAL_VIEW_CANCELLED);
	CHECK(g_ed[0].day == day(2026, 11, 4) && g_ed[0].start_minute == 600 &&
	      v.date.day == 4);
	CHECK(ctui_cal_view_release(&v, W, H, 9, 30) == CTUI_CAL_VIEW_NONE &&
	      g_ed[0].start_minute == 600);
	ctui_cal_view_press(&v, W, H, 6, 24);
	ctui_cal_view_drag(&v, W, H, 7, 24);
	CHECK(ctui_cal_view_key(&v, 27, H) == CTUI_CAL_VIEW_CANCELLED &&
	      g_ed[0].start_minute == 600);

	/* the bottom edge changes the end, in the day of the event, one slot at
	 * least */
	mouse_view(&v, 1, 3);
	ctui_cal_view_press(&v, W, H, 8, 24);
	CHECK(v.edit.grab == CTUI_CAL_GRAB_RESIZE && !v.edit.top);
	CHECK(ctui_cal_view_drag(&v, W, H, 10, 40) == CTUI_CAL_VIEW_RESIZED &&
	      g_ed[0].end_minute == 780);
	CHECK(g_ed[0].day == day(2026, 11, 4) && g_ed[0].start_minute == 600);
	CHECK(ctui_cal_view_drag(&v, W, H, 1, 24) == CTUI_CAL_VIEW_RESIZED &&
	      g_ed[0].end_minute == 630);
	CHECK(ctui_cal_view_release(&v, W, H, 1, 24) == CTUI_CAL_VIEW_DROPPED);

	/* the top edge changes the start */
	mouse_view(&v, 1, 3);
	ctui_cal_view_press(&v, W, H, 5, 24);
	CHECK(v.edit.grab == CTUI_CAL_GRAB_RESIZE && v.edit.top);
	CHECK(ctui_cal_view_drag(&v, W, H, 3, 24) == CTUI_CAL_VIEW_RESIZED &&
	      g_ed[0].start_minute == 540 && g_ed[0].end_minute == 720);
	CHECK(ctui_cal_view_drag(&v, W, H, 10, 24) == CTUI_CAL_VIEW_RESIZED &&
	      g_ed[0].start_minute == 690);
	CHECK(ctui_cal_view_cancel(&v, H) == CTUI_CAL_VIEW_CANCELLED &&
	      g_ed[0].start_minute == 600);

	/* dragging over empty slots creates an event over the range */
	mouse_view(&v, 1, 3);
	ctui_cal_view_press(&v, W, H, 2, 40); /* Friday 08:30 */
	CHECK(v.edit.grab == CTUI_CAL_GRAB_CREATE);
	CHECK(ctui_cal_view_drag(&v, W, H, 4, 24) ==
	      CTUI_CAL_VIEW_MOVED); /* the day stays Friday */
	CHECK(v.event_count == 1);
	CHECK(ctui_cal_view_release(&v, W, H, 4, 24) == CTUI_CAL_VIEW_CREATED &&
	      v.event_count == 2);
	CHECK(g_ed[1].day == day(2026, 11, 6) && g_ed[1].start_minute == 510 &&
	      g_ed[1].end_minute == 600 && g_ed[1].title == NULL &&
	      v.selected == 1 && v.changed == 1);
	/* upwards, and into the last slot of the capacity */
	ctui_cal_view_press(&v, W, H, 5, 40);
	ctui_cal_view_drag(&v, W, H, 3, 40);
	CHECK(ctui_cal_view_release(&v, W, H, 3, 40) == CTUI_CAL_VIEW_CREATED &&
	      g_ed[2].start_minute == 540 && g_ed[2].end_minute == 630);
	ctui_cal_view_press(&v, W, H, 9, 40);
	ctui_cal_view_drag(&v, W, H, 10, 40);
	CHECK(ctui_cal_view_release(&v, W, H, 10, 40) == CTUI_CAL_VIEW_FULL &&
	      v.event_count == 3);
	/* a click, or a cancel, creates nothing */
	ctui_cal_view_press(&v, W, H, 9, 40);
	CHECK(ctui_cal_view_release(&v, W, H, 9, 40) == CTUI_CAL_VIEW_NONE);
	ctui_cal_view_press(&v, W, H, 9, 40);
	ctui_cal_view_drag(&v, W, H, 10, 40);
	CHECK(ctui_cal_view_cancel(&v, H) == CTUI_CAL_VIEW_CANCELLED);
	CHECK(ctui_cal_view_release(&v, W, H, 10, 40) == CTUI_CAL_VIEW_NONE &&
	      v.event_count == 3);

	/* a view that is not editable only picks and moves the cursor */
	ctui_cal_view p;
	ctui_cal_view_init(&p, CTUI_CAL_VIEW_WEEK, 2026, 11, 4);
	p.top = 480;
	ctui_cal_view_set_events(&p, g_ed, 1);
	CHECK(ctui_cal_view_press(&p, W, H, 6, 24) == CTUI_CAL_VIEW_PICKED &&
	      p.edit.grab == CTUI_CAL_GRAB_NONE);
	CHECK(ctui_cal_view_drag(&p, W, H, 8, 32) == CTUI_CAL_VIEW_NONE);
	CHECK(ctui_cal_view_release(&p, W, H, 8, 32) == CTUI_CAL_VIEW_NONE);
	CHECK(ctui_cal_view_hover(&p, W, H, 5, 24) == CTUI_CAL_VIEW_NONE);

	/* hovering an edge is reported once */
	mouse_view(&v, 1, 3);
	CHECK(ctui_cal_view_hover(&v, W, H, 5, 24) == CTUI_CAL_VIEW_HOVERED &&
	      v.hover_edge == CTUI_CAL_EDGE_TOP);
	CHECK(ctui_cal_view_hover(&v, W, H, 5, 25) == CTUI_CAL_VIEW_NONE);
	CHECK(ctui_cal_view_hover(&v, W, H, 8, 25) == CTUI_CAL_VIEW_HOVERED &&
	      v.hover_edge == CTUI_CAL_EDGE_BOTTOM);
	CHECK(ctui_cal_view_hover(&v, W, H, 6, 25) == CTUI_CAL_VIEW_HOVERED &&
	      v.hover_edge == CTUI_CAL_EDGE_NONE);
	CHECK(ctui_cal_view_hover(&v, W, H, 3, 25) == CTUI_CAL_VIEW_NONE);

	/* dragging an occurrence moves its series */
	mouse_view(&v, 1, 3);
	g_ed[0].day = day(2026, 10, 28);
	g_edr[0] =
	    (ctui_cal_recur){.repeat = CTUI_CAL_REPEAT_WEEKLY, .interval = 1};
	v.date = (ctui_date){2026, 11, 4};
	ctui_cal_view_press(&v, W, H, 6, 24);
	CHECK(v.selected == 0 && v.edit.grab == CTUI_CAL_GRAB_MOVE);
	ctui_cal_view_drag(&v, W, H, 6, 32);
	ctui_cal_view_release(&v, W, H, 6, 32);
	CHECK(g_ed[0].day == day(2026, 10, 29) &&
	      ctui_cal_event_occurs(&g_ed[0], &g_edr[0], day(2026, 11, 5)));
}

/* A weekly series (Tuesdays 09:00-10:00 from 2026-10-27) next to a single. */
static ctui_cal_event g_rec_events[2];
static ctui_cal_recur g_rec[2];

static void recurring_view(ctui_cal_view *v)
{
	g_rec_events[0] =
	    (ctui_cal_event){day(2026, 10, 27), 540, 600, "Weekly", 0};
	g_rec_events[1] =
	    (ctui_cal_event){day(2026, 11, 3), 570, 630, "Single", 0};
	g_rec[0] =
	    (ctui_cal_recur){.repeat = CTUI_CAL_REPEAT_WEEKLY, .interval = 1};
	g_rec[1] = (ctui_cal_recur){0};
	ctui_cal_view_init(v, CTUI_CAL_VIEW_WEEK, 2026, 11, 4);
	v->top = 480;
	ctui_cal_view_set_events(v, g_rec_events, 2);
	ctui_cal_view_set_recurs(v, g_rec);
}

static void test_recurring(void)
{
	ctui_cal_view v;
	recurring_view(&v);
	int lane[2], lanes[2], y, x, h, w;
	/* the anchor is in the previous week: this week's Tuesday is an
	 * occurrence */
	CHECK(ctui_cal_layout_recur(g_rec_events, g_rec, 2, day(2026, 11, 3),
				    lane, lanes) == 2);
	CHECK(lane[0] == 0 && lane[1] == 1 && lanes[0] == 2);
	CHECK(ctui_cal_layout_recur(g_rec_events, g_rec, 2, day(2026, 11, 4),
				    lane, lanes) == 0);
	CHECK(ctui_cal_layout_recur(g_rec_events, g_rec, 2, day(2026, 11, 10),
				    lane, lanes) == 1);
	CHECK(ctui_cal_layout(g_rec_events, 2, day(2026, 11, 3), lane, lanes) ==
	      1); /* no rules */
	CHECK(ctui_cal_view_event_rect(&v, W, H, 0, &y, &x, &h, &w) &&
	      x == 14 && y == 3);
	CHECK(ctui_cal_view_event_at(&v, W, H, 3, 14) == 0);
	CHECK(ctui_cal_view_press(&v, W, H, 3, 14) == CTUI_CAL_VIEW_PICKED &&
	      v.selected == 0);
	v.mode = CTUI_CAL_VIEW_DAY;
	v.date = (ctui_date){2026, 11, 10}; /* a week later: only the series */
	CHECK(ctui_cal_view_event_at(&v, 40, H, 3, 8) == 0);
	CHECK(ctui_cal_view_event_at(&v, 40, H, 4, 8) == 0);
	CHECK(ctui_cal_view_event_at(&v, 40, H, 5, 8) == -1);
	v.date =
	    (ctui_date){2026, 10, 20}; /* before the anchor nothing occurs */
	CHECK(ctui_cal_view_event_at(&v, 40, H, 3, 8) == -1);
}

static void test_draw(void)
{
	screen_open(24, 80);
	screen_clear();
	ctui_cal_view v;
	week_view(&v);
	ctui_cal_view_draw(&v, 1, 2, W, H);
	CHECK(screen_has_text("Mon  2") && screen_has_text("Sun  8"));
	CHECK(screen_has_text("08:00") && screen_has_text("12:00"));
	CHECK(screen_has_text("Lunch"));
	CHECK(!screen_has_text("Demo")); /* 14:00 is not shown */
	CHECK(screen_has_color(ctui_attr(CTUI_PAIR_GAUGE_GREEN, false)));
	CHECK(screen_has_color(
	    ctui_attr(CTUI_PAIR_CURSOR, true))); /* today's header */
	CHECK(screen_has_color(
	    ctui_attr(CTUI_PAIR_SELECTION, true))); /* cursor day header */
	CHECK(screen_has_color(
	    ctui_attr(CTUI_PAIR_ERROR, false))); /* the current time */
	char row[16] = {0};
	mvinnstr(1, 2, row, 5);
	CHECK(row[0] == 'M' || row[0] == ' ');

	v.week_start = 6;
	screen_clear();
	ctui_cal_view_draw(&v, 0, 0, W, H);
	CHECK(screen_has_text("Sun  1"));

	/* the day view: full date and room for the title and the times */
	v.mode = CTUI_CAL_VIEW_DAY;
	v.date = (ctui_date){2026, 11, 3};
	screen_clear();
	ctui_cal_view_draw(&v, 0, 0, 40, H);
	CHECK(screen_has_text("Tuesday 2026-11-03"));
	CHECK(screen_has_text("Standup") && screen_has_text("Planning"));
	CHECK(screen_has_text("09:00-10:00") && screen_has_text("09:30-10:30"));
	CHECK(!screen_has_text("Mon"));

	/* the picked event is underlined, names are per instance */
	static const char *const NAMES[7] = {"Lu", "Ma", "Me", "Je",
					     "Ve", "Sa", "Di"};
	v.selected = 0;
	v.weekday_names = NAMES;
	v.mode = CTUI_CAL_VIEW_WEEK;
	screen_clear();
	ctui_cal_view_draw(&v, 0, 0, W, H);
	CHECK(screen_has_text("Ma  3"));
	CHECK((mvinch(3, 22) & A_UNDERLINE) != 0);

	/* a style recolours only this instance */
	ctui_style st = {0};
	st.attr[CTUI_PAIR_GAUGE_GREEN] = ctui_attr(CTUI_PAIR_ERROR, true);
	v.style = &st;
	screen_clear();
	ctui_cal_view_draw(&v, 0, 0, W, H);
	CHECK(screen_has_color(ctui_attr(CTUI_PAIR_ERROR, true)));

	/* occurrences carry the recurrence marker */
	recurring_view(&v);
	screen_clear();
	ctui_cal_view_draw(&v, 0, 0, W, H);
	CHECK(screen_has_text("Wee") &&
	      screen_has_text("Sin")); /* two lanes of 3 columns */
	v.mode = CTUI_CAL_VIEW_DAY;
	v.date = (ctui_date){2026, 10, 27};
	screen_clear();
	ctui_cal_view_draw(&v, 0, 0, 40, H);
	CHECK(screen_has_text("Weekly") && screen_has_text("\xe2\x86\xbb"));
	v.date = (ctui_date){2026, 10, 20};
	screen_clear();
	ctui_cal_view_draw(&v, 0, 0, 40, H);
	CHECK(!screen_has_text("Weekly"));

	/* a grabbed event is reversed; the edge being resized and a hovered
	 * edge too */
	mouse_view(&v, 1, 3);
	CHECK((mvinch(6, 24) & A_REVERSE) == 0);
	ctui_cal_view_key(&v, 'm', H);
	screen_clear();
	ctui_cal_view_draw(&v, 0, 0, W, H);
	CHECK((mvinch(6, 24) & A_REVERSE) != 0);
	ctui_cal_view_key(&v, 27, H);
	ctui_cal_view_key(&v, 'r', H);
	screen_clear();
	ctui_cal_view_draw(&v, 0, 0, W, H);
	CHECK((mvinch(8, 24) & A_REVERSE) != 0 &&
	      (mvinch(6, 24) & A_REVERSE) == 0);
	ctui_cal_view_key(&v, 27, H);
	ctui_cal_view_hover(&v, W, H, 5, 24);
	screen_clear();
	ctui_cal_view_draw(&v, 0, 0, W, H);
	CHECK((mvinch(5, 24) & A_REVERSE) != 0 &&
	      (mvinch(8, 24) & A_REVERSE) == 0);
	/* the range dragged to create an event */
	ctui_cal_view_press(&v, W, H, 2, 40);
	ctui_cal_view_drag(&v, W, H, 3, 40);
	screen_clear();
	ctui_cal_view_draw(&v, 0, 0, W, H);
	CHECK(PAIR_NUMBER(mvinch(3, 40) & A_COLOR) ==
	      PAIR_NUMBER((chtype)ctui_attr(CTUI_PAIR_SELECTION, true)));
	ctui_cal_view_cancel(&v, H);

	/* tiny boxes draw nothing and do not crash */
	screen_clear();
	ctui_cal_view_draw(&v, 0, 0, 10, H);
	ctui_cal_view_draw(&v, 0, 0, W, 1);
	ctui_cal_view_draw(&v, 0, 0, W, 0);
	endwin();
}

static void test_select(void)
{
	ctui_cal_view v;
	week_view(&v); /* Wednesday 2026-11-04, the week of Monday the 2nd */
	CHECK(ctui_cal_view_key(&v, KEY_SRIGHT, H) == CTUI_CAL_VIEW_SELECTED &&
	      v.date.day == 5);
	CHECK(v.sel.count == 2 &&
	      ctui_cal_select_contains(&v.sel, day(2026, 11, 4)));
	CHECK(ctui_cal_view_key(&v, 'L', H) == CTUI_CAL_VIEW_SELECTED &&
	      v.date.day == 6 && v.sel.count == 3);
	CHECK(ctui_cal_view_key(&v, KEY_SLEFT, H) == CTUI_CAL_VIEW_SELECTED &&
	      v.sel.count == 2);
	CHECK(ctui_cal_view_key(&v, 'H', H) == CTUI_CAL_VIEW_SELECTED &&
	      v.sel.count == 1);
	CHECK(ctui_cal_view_key(&v, 'H', H) == CTUI_CAL_VIEW_SELECTED &&
	      v.sel.count == 2 && v.date.day == 3);
	/* into the next week the view follows the cursor */
	for (int i = 0; i < 6; i++)
		ctui_cal_view_key(&v, KEY_SRIGHT, H);
	CHECK(v.date.day == 9 && v.sel.count == 6 &&
	      ctui_cal_view_first_day(&v) == day(2026, 11, 9));
	CHECK(ctui_cal_view_key(&v, 27, H) == CTUI_CAL_VIEW_SELECTED &&
	      v.sel.count == 0);
	CHECK(ctui_cal_view_key(&v, 27, H) == CTUI_CAL_VIEW_NONE);

	/* Space toggles, a plain move keeps the days */
	CHECK(ctui_cal_view_key(&v, ' ', H) == CTUI_CAL_VIEW_SELECTED &&
	      v.sel.count == 1);
	CHECK(ctui_cal_view_key(&v, KEY_RIGHT, H) == CTUI_CAL_VIEW_DATE &&
	      v.sel.count == 1 && !v.sel.has_anchor);
	CHECK(ctui_cal_view_key(&v, KEY_RIGHT, H) == CTUI_CAL_VIEW_DATE);
	CHECK(ctui_cal_view_key(&v, ' ', H) == CTUI_CAL_VIEW_SELECTED &&
	      v.sel.count == 2);
	CHECK(!ctui_cal_select_contiguous(&v.sel));
	CHECK(ctui_cal_view_key(&v, ' ', H) == CTUI_CAL_VIEW_SELECTED &&
	      v.sel.count == 1);

	/* Shift+Up / Shift+Down still resize the event, not the selection */
	ctui_cal_view e;
	mouse_view(&e, 1, 3);
	ctui_cal_view_key(&e, KEY_SF, H);
	CHECK(e.sel.count == 0 && g_ed[0].end_minute == 750);
	/* Esc cancels a grab before it clears the selection */
	ctui_cal_select_toggle(&e.sel, day(2026, 11, 4));
	ctui_cal_view_key(&e, 'm', H);
	CHECK(ctui_cal_view_key(&e, 27, H) == CTUI_CAL_VIEW_CANCELLED &&
	      e.sel.count == 1);
	CHECK(ctui_cal_view_key(&e, 27, H) == CTUI_CAL_VIEW_SELECTED &&
	      e.sel.count == 0);
	/* the day view extends day by day too */
	week_view(&v);
	v.mode = CTUI_CAL_VIEW_DAY;
	CHECK(ctui_cal_view_key(&v, KEY_SRIGHT, H) == CTUI_CAL_VIEW_SELECTED &&
	      v.sel.count == 2);
	/* limits: the end of year 9999 and the cap */
	ctui_cal_view_init(&v, CTUI_CAL_VIEW_WEEK, 9999, 12, 31);
	CHECK(ctui_cal_view_key(&v, KEY_SRIGHT, H) == CTUI_CAL_VIEW_NONE &&
	      v.sel.count == 0);
	ctui_cal_view_init(&v, CTUI_CAL_VIEW_WEEK, 2026, 12, 20);
	for (int i = 0; i < 70; i++)
		ctui_cal_view_key(&v, 'L', H);
	CHECK(v.sel.count == CTUI_CAL_SELECT_MAX &&
	      ctui_cal_select_last(&v.sel) == day(2026, 12, 20) + 63);
}

static void test_select_mouse(void)
{
	ctui_cal_view v;
	week_view(&v);
	/* headers: columns of 8 from x = 6 */
	CHECK(ctui_cal_view_press(&v, W, H, 0, 6 + 8 * 1) ==
		  CTUI_CAL_VIEW_DATE &&
	      v.date.day == 3);
	CHECK(v.sel.dragging && v.sel.count == 0 && ctui_cal_view_dragging(&v));
	CHECK(ctui_cal_view_drag(&v, W, H, 0, 6 + 8 * 3) ==
	      CTUI_CAL_VIEW_SELECTED);
	CHECK(v.date.day == 5 && v.sel.count == 3 &&
	      ctui_cal_select_first(&v.sel) == day(2026, 11, 3));
	CHECK(ctui_cal_view_drag(&v, W, H, 5, 6 + 8 * 3) ==
	      CTUI_CAL_VIEW_NONE); /* same day, other row */
	CHECK(ctui_cal_view_drag(&v, W, H, 0, 200) == CTUI_CAL_VIEW_SELECTED &&
	      v.date.day == 8 && v.sel.count == 6);
	CHECK(ctui_cal_view_release(&v, W, H, 0, 6 + 8 * 2) ==
	      CTUI_CAL_VIEW_SELECTED);
	CHECK(v.sel.count == 2 && !v.sel.dragging &&
	      !ctui_cal_view_dragging(&v));
	/* a press in the grid clears the selection and does not start a
	 * selection drag */
	CHECK(ctui_cal_view_press(&v, W, H, 3, 6 + 8 * 5) ==
	      CTUI_CAL_VIEW_DATE);
	CHECK(v.sel.count == 0 && !v.sel.dragging);
	ctui_cal_view_press(&v, W, H, 0, 6 + 8 * 1);
	ctui_cal_view_release(&v, W, H, 0, 6 + 8 * 1);
	CHECK(v.sel.count == 0);
	/* toggling */
	CHECK(ctui_cal_view_toggle_at(&v, W, H, 0, 6 + 8 * 2) ==
		  CTUI_CAL_VIEW_SELECTED &&
	      v.date.day == 4);
	CHECK(ctui_cal_view_toggle_at(&v, W, H, 4, 6 + 8 * 4) ==
		  CTUI_CAL_VIEW_SELECTED &&
	      v.sel.count == 2);
	CHECK(ctui_cal_view_toggle_at(&v, W, H, 4, 6 + 8 * 4) ==
		  CTUI_CAL_VIEW_SELECTED &&
	      v.sel.count == 1);
	CHECK(ctui_cal_view_toggle_at(&v, W, H, 4, 2) == CTUI_CAL_VIEW_NONE &&
	      v.sel.count == 1); /* the axis */
	/* a press on an hour label keeps the selection */
	CHECK(ctui_cal_view_press(&v, W, H, 5, 2) != CTUI_CAL_VIEW_SELECTED &&
	      v.sel.count == 1);

	/* a drag over slots of an editable view still creates an event */
	ctui_cal_view m;
	mouse_view(&m, 1, 3);
	ctui_cal_view_press(&m, W, H, 2, 40);
	CHECK(!m.sel.dragging && m.edit.mouse);
	ctui_cal_view_drag(&m, W, H, 3, 40);
	CHECK(ctui_cal_view_release(&m, W, H, 3, 40) == CTUI_CAL_VIEW_CREATED &&
	      m.sel.count == 0);
}

static void test_select_draw(void)
{
	screen_open(24, 80);
	screen_clear();
	ctui_cal_view v;
	week_view(&v);
	ctui_cal_view_draw(&v, 0, 0, W, H);
	CHECK(!screen_has_color(ctui_attr(CTUI_PAIR_CURSOR_SELECTED, true)));
	ctui_cal_select_set_range(&v.sel, day(2026, 11, 5), day(2026, 11, 6));
	screen_clear();
	ctui_cal_view_draw(&v, 0, 0, W, H);
	CHECK(PAIR_NUMBER(mvinch(0, 6 + 8 * 3) & A_COLOR) ==
	      PAIR_NUMBER((chtype)ctui_attr(CTUI_PAIR_CURSOR_SELECTED, true)));
	CHECK(PAIR_NUMBER(mvinch(0, 6 + 8 * 4) & A_COLOR) ==
	      PAIR_NUMBER((chtype)ctui_attr(CTUI_PAIR_CURSOR_SELECTED, true)));
	CHECK(PAIR_NUMBER(mvinch(0, 6 + 8 * 2) & A_COLOR) ==
	      PAIR_NUMBER((chtype)ctui_attr(CTUI_PAIR_SELECTION, true)));
	endwin();
}

int main(void)
{
	RUN(test_layout);
	RUN(test_geometry);
	RUN(test_hit);
	RUN(test_keys);
	RUN(test_state);
	RUN(test_mouse);
	RUN(test_edit_helpers);
	RUN(test_edit_create_delete);
	RUN(test_edit_move);
	RUN(test_edit_resize);
	RUN(test_edit_series);
	RUN(test_edit_mouse);
	RUN(test_recurring);
	RUN(test_draw);
	RUN(test_select);
	RUN(test_select_mouse);
	RUN(test_select_draw);
	return CHECK_DONE();
}
