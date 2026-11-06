/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */
#include "check.h"
#include "screen.h"

#include <curses.h>

#include <ctui/calendar_view.h>
#include <ctui/theme.h>

#define W 62 /* axis 6 + seven columns of 8 (7 usable) */
#define H 11 /* header + ten rows */
#define DM 1440

static long day(int y, int m, int d)
{
	return ctui_date_to_days(y, m, d);
}

static ctui_cal_event g_ev[8];
static ctui_cal_recur g_rc[8];
static ctui_cal_span g_sp[8];

/* The week of Monday 2026-11-02, cursor on Wednesday the 4th at 10:00, the
 * shown hours start at 08:00. Event 0 is a trip: 10:00 on Tuesday the 3rd
 * to 11:00 on Thursday the 5th. */
static void trip_view(ctui_cal_view *v, int count, int cap)
{
	for (int i = 0; i < 8; i++) {
		g_rc[i] = (ctui_cal_recur){0};
		g_sp[i] = (ctui_cal_span){0};
	}
	g_ev[0] = (ctui_cal_event){day(2026, 11, 3), 600, 660, "Trip", 0};
	g_sp[0].extra_days = 2;
	g_ev[1] = (ctui_cal_event){day(2026, 11, 3), 0, DM, "Holiday", 0};
	g_sp[1].extra_days = 4;
	ctui_cal_view_init(v, CTUI_CAL_VIEW_WEEK, 2026, 11, 4);
	v->top = 480;
	v->minute = 600;
	ctui_cal_view_set_editable(v, g_ev, g_rc, count, cap);
	ctui_cal_view_set_editable_spans(v, g_sp);
}

static void test_layout(void)
{
	ctui_cal_view v;
	trip_view(&v, 2, 8);
	int lane[2], lanes[2];
	CHECK(ctui_cal_layout_span(g_ev, g_rc, g_sp, 2, day(2026, 11, 2), lane,
				   lanes) == 0);
	/* Tuesday: the trip starts at 10:00, the holiday is there all day */
	CHECK(ctui_cal_layout_span(g_ev, g_rc, g_sp, 2, day(2026, 11, 3), lane,
				   lanes) == 2);
	CHECK(lane[1] == 0 && lane[0] == 1 && lanes[0] == 2 && lanes[1] == 2);
	CHECK(ctui_cal_layout_span(g_ev, g_rc, g_sp, 2, day(2026, 11, 5), lane,
				   lanes) == 2);
	CHECK(ctui_cal_layout_span(g_ev, g_rc, g_sp, 2, day(2026, 11, 6), lane,
				   lanes) == 1 &&
	      lane[0] == -1 && lane[1] == 0 && lanes[1] == 1);
	CHECK(ctui_cal_layout_span(g_ev, g_rc, g_sp, 2, day(2026, 11, 7), lane,
				   lanes) == 1);
	CHECK(ctui_cal_layout_span(g_ev, g_rc, g_sp, 2, day(2026, 11, 8), lane,
				   lanes) == 0 &&
	      lane[1] == -1);
	/* without the spans, the old layouts see the events on their own day
	 * only */
	CHECK(ctui_cal_layout_recur(g_ev, g_rc, 2, day(2026, 11, 4), lane,
				    lanes) == 0);
	CHECK(ctui_cal_layout(g_ev, 2, day(2026, 11, 3), lane, lanes) == 2);

	/* a series keeps the length in every occurrence */
	g_rc[0] =
	    (ctui_cal_recur){.repeat = CTUI_CAL_REPEAT_WEEKLY, .interval = 1};
	CHECK(ctui_cal_layout_span(g_ev, g_rc, g_sp, 1, day(2026, 11, 10), lane,
				   lanes) == 1);
	CHECK(ctui_cal_layout_span(g_ev, g_rc, g_sp, 1, day(2026, 11, 12), lane,
				   lanes) == 1);
	CHECK(ctui_cal_layout_span(g_ev, g_rc, g_sp, 1, day(2026, 11, 13), lane,
				   lanes) == 0);
	CHECK(ctui_cal_layout_span(g_ev, g_rc, g_sp, 1, day(2026, 11, 9), lane,
				   lanes) == 0);
}

static void test_geometry_and_hit(void)
{
	ctui_cal_view v;
	trip_view(&v, 1, 8);
	int y, x, h, w;
	/* Tuesday (x 14): 10:00 on, to the bottom of the shown hours; then
	 * whole columns; then up to 11:00 */
	CHECK(ctui_cal_view_event_rect(&v, W, H, 0, &y, &x, &h, &w) && y == 5 &&
	      x == 14 && h == 6 && w == 7);
	CHECK(ctui_cal_view_event_at(&v, W, H, 5, 14) == 0 &&
	      ctui_cal_view_event_at(&v, W, H, 4, 14) == -1);
	CHECK(ctui_cal_view_event_at(&v, W, H, 1, 22) == 0 &&
	      ctui_cal_view_event_at(&v, W, H, 10, 22) == 0);
	CHECK(ctui_cal_view_event_at(&v, W, H, 6, 30) == 0 &&
	      ctui_cal_view_event_at(&v, W, H, 7, 30) == -1);
	CHECK(ctui_cal_view_event_at(&v, W, H, 5, 38) == -1 &&
	      ctui_cal_view_event_at(&v, W, H, 5, 6) == -1);
	/* the event of the week before is found in the day it still lasts in */
	v.date = (ctui_date){2026, 11, 11};
	CHECK(!ctui_cal_view_event_rect(&v, W, H, 0, &y, &x, &h, &w));
	g_rc[0] =
	    (ctui_cal_recur){.repeat = CTUI_CAL_REPEAT_WEEKLY, .interval = 1};
	CHECK(ctui_cal_view_event_rect(&v, W, H, 0, &y, &x, &h, &w) &&
	      x == 14 && y == 5);
	g_rc[0] = (ctui_cal_recur){0};
	/* an event that began in the previous week shows on the first columns
	 * of this one */
	g_ev[0].day = day(2026, 10, 31);
	g_sp[0].extra_days = 3; /* to 11:00 on Tuesday 3rd */
	v.date = (ctui_date){2026, 11, 4};
	CHECK(ctui_cal_view_event_rect(&v, W, H, 0, &y, &x, &h, &w) && x == 6 &&
	      y == 1 && h == 10);
	CHECK(ctui_cal_view_event_at(&v, W, H, 3, 6) == 0 &&
	      ctui_cal_view_event_at(&v, W, H, 3, 22) == -1);
	CHECK(ctui_cal_view_event_at(&v, W, H, 3, 14) == 0 &&
	      ctui_cal_view_event_at(&v, W, H, 7, 14) == -1);

	/* the edges are on the first and last days only */
	trip_view(&v, 1, 8);
	int idx = -1;
	long d = 0;
	CHECK(ctui_cal_view_event_part(&v, W, H, 5, 14, &idx, &d) ==
		  CTUI_CAL_EDGE_TOP &&
	      idx == 0 && d == day(2026, 11, 3));
	CHECK(ctui_cal_view_event_part(&v, W, H, 6, 14, NULL, NULL) ==
	      CTUI_CAL_EDGE_BODY);
	CHECK(ctui_cal_view_event_part(&v, W, H, 10, 14, NULL, NULL) ==
	      CTUI_CAL_EDGE_BODY); /* it goes on */
	CHECK(ctui_cal_view_event_part(&v, W, H, 1, 22, NULL, NULL) ==
	      CTUI_CAL_EDGE_BODY);
	CHECK(ctui_cal_view_event_part(&v, W, H, 10, 22, NULL, NULL) ==
	      CTUI_CAL_EDGE_BODY);
	CHECK(ctui_cal_view_event_part(&v, W, H, 1, 30, NULL, NULL) ==
	      CTUI_CAL_EDGE_BODY); /* came from before */
	CHECK(ctui_cal_view_event_part(&v, W, H, 6, 30, &idx, &d) ==
		  CTUI_CAL_EDGE_BOTTOM &&
	      d == day(2026, 11, 5));
	CHECK(ctui_cal_view_event_part(&v, W, H, 3, 30, NULL, NULL) ==
	      CTUI_CAL_EDGE_BODY);

	/* a click picks it from any of its days */
	CHECK(ctui_cal_view_press(&v, W, H, 3, 22) == CTUI_CAL_VIEW_PICKED &&
	      v.selected == 0);
	ctui_cal_view_cancel(&v, H);
	/* the cursor on a middle day finds it, on a day after it does not */
	v.selected = -1;
	CHECK(ctui_cal_view_event_under_cursor(&v) == 0);
	v.date = (ctui_date){2026, 11, 5};
	v.minute = 660; /* the event ends at 11:00 */
	CHECK(ctui_cal_view_event_under_cursor(&v) == -1);
	v.minute = 600;
	CHECK(ctui_cal_view_event_under_cursor(&v) == 0);
	v.date = (ctui_date){2026, 11, 6};
	CHECK(ctui_cal_view_event_under_cursor(&v) == -1);
}

static void test_delete(void)
{
	ctui_cal_view v;
	trip_view(&v, 2, 8);
	v.date = (ctui_date){2026, 11, 4};
	v.selected = 0;
	CHECK(ctui_cal_view_delete(&v, false) == CTUI_CAL_VIEW_DELETED &&
	      v.event_count == 1);
	CHECK(strcmp(g_ev[0].title, "Holiday") == 0 &&
	      g_sp[0].extra_days == 4); /* spans moved down too */
	CHECK(ctui_cal_view_delete(&v, false) == CTUI_CAL_VIEW_DELETED &&
	      v.event_count == 0);

	/* an occurrence of a series is skipped by the day it starts */
	trip_view(&v, 1, 8);
	g_rc[0] =
	    (ctui_cal_recur){.repeat = CTUI_CAL_REPEAT_WEEKLY, .interval = 1};
	v.date = (ctui_date){
	    2026, 11, 11}; /* Wednesday, the middle of the 2nd occurrence */
	CHECK(ctui_cal_view_delete(&v, false) == CTUI_CAL_VIEW_EXCEPTED);
	CHECK(g_rc[0].exception_count == 1 &&
	      g_rc[0].exceptions[0] == day(2026, 11, 10) &&
	      v.changed_day == day(2026, 11, 10));
	int lane[1], lanes[1];
	CHECK(ctui_cal_layout_span(g_ev, g_rc, g_sp, 1, day(2026, 11, 11), lane,
				   lanes) == 0);
	CHECK(ctui_cal_layout_span(g_ev, g_rc, g_sp, 1, day(2026, 11, 3), lane,
				   lanes) == 1);
	CHECK(ctui_cal_layout_span(g_ev, g_rc, g_sp, 1, day(2026, 11, 17), lane,
				   lanes) == 1);
	CHECK(ctui_cal_view_delete(&v, true) ==
	      CTUI_CAL_VIEW_NONE); /* nothing under the cursor now */
}

static void test_move(void)
{
	ctui_cal_view v;
	trip_view(&v, 1, 8);
	CHECK(ctui_cal_view_key(&v, 'm', H) == CTUI_CAL_VIEW_GRABBED);
	/* a day right: the whole trip shifts */
	CHECK(ctui_cal_view_key(&v, KEY_RIGHT, H) == CTUI_CAL_VIEW_EVENT_MOVED);
	CHECK(g_ev[0].day == day(2026, 11, 4) && g_ev[0].start_minute == 600 &&
	      g_ev[0].end_minute == 660 && g_sp[0].extra_days == 2 &&
	      v.date.day == 5);
	CHECK(ctui_cal_view_key(&v, KEY_DOWN, H) == CTUI_CAL_VIEW_EVENT_MOVED);
	CHECK(g_ev[0].start_minute == 630 && g_ev[0].end_minute == 690 &&
	      g_sp[0].extra_days == 2);
	CHECK(ctui_cal_view_key(&v, 27, H) == CTUI_CAL_VIEW_CANCELLED);
	CHECK(g_ev[0].day == day(2026, 11, 3) && g_ev[0].start_minute == 600 &&
	      g_sp[0].extra_days == 2);

	/* the week is the limit for the grabbed column; a late start crossing
	 * midnight keeps its length */
	g_ev[0] = (ctui_cal_event){day(2026, 11, 3), 1380, 60, "Trip", 0};
	g_sp[0].extra_days = 2; /* 23:00 on the 3rd to 01:00 on the 5th */
	v.date = (ctui_date){2026, 11, 3};
	v.minute = 1380;
	ctui_cal_view_key(&v, 'm', H);
	for (int i = 0; i < 9; i++)
		ctui_cal_view_key(&v, KEY_RIGHT, H);
	CHECK(v.date.day == 8 && g_ev[0].day == day(2026, 11, 8));
	CHECK(g_ev[0].start_minute == 1380 && g_ev[0].end_minute == 60 &&
	      g_sp[0].extra_days == 2);
	CHECK(ctui_cal_view_key(&v, KEY_DOWN, H) == CTUI_CAL_VIEW_EVENT_MOVED &&
	      g_ev[0].start_minute == 1410 && g_ev[0].end_minute == 90);
	CHECK(ctui_cal_view_key(&v, KEY_DOWN, H) ==
	      CTUI_CAL_VIEW_NONE); /* the last slot of the day */
	CHECK(ctui_cal_view_key(&v, '\n', H) == CTUI_CAL_VIEW_DROPPED);

	/* a series moves with its length */
	trip_view(&v, 1, 8);
	g_rc[0] = (ctui_cal_recur){.repeat = CTUI_CAL_REPEAT_WEEKLY,
				   .interval = 1,
				   .end = CTUI_CAL_END_UNTIL,
				   .until = day(2026, 12, 1)};
	v.date =
	    (ctui_date){2026, 11, 4}; /* the middle of the first occurrence */
	ctui_cal_view_key(&v, 'm', H);
	CHECK(ctui_cal_view_key(&v, KEY_RIGHT, H) == CTUI_CAL_VIEW_EVENT_MOVED);
	CHECK(g_ev[0].day == day(2026, 11, 4) && g_sp[0].extra_days == 2 &&
	      g_rc[0].until == day(2026, 12, 2));
	ctui_cal_view_key(&v, 27, H);
	CHECK(g_ev[0].day == day(2026, 11, 3) &&
	      g_rc[0].until == day(2026, 12, 1));

	/* mouse: press the middle of the trip on Wednesday, drag to Thursday
	 * two rows down */
	trip_view(&v, 1, 8);
	CHECK(ctui_cal_view_press(&v, W, H, 6, 24) == CTUI_CAL_VIEW_PICKED &&
	      v.edit.grab == CTUI_CAL_GRAB_MOVE);
	CHECK(ctui_cal_view_drag(&v, W, H, 8, 32) == CTUI_CAL_VIEW_EVENT_MOVED);
	CHECK(g_ev[0].day == day(2026, 11, 4) && g_ev[0].start_minute == 660 &&
	      g_ev[0].end_minute == 720 && g_sp[0].extra_days == 2);
	CHECK(ctui_cal_view_release(&v, W, H, 8, 32) == CTUI_CAL_VIEW_DROPPED);
	/* a drag outside the box stops at the last column of the week */
	ctui_cal_view_press(&v, W, H, 6, 40);
	ctui_cal_view_drag(&v, W, H, 6, 500);
	CHECK(g_ev[0].day <= day(2026, 11, 8) &&
	      g_ev[0].day >= day(2026, 11, 5));
	ctui_cal_view_cancel(&v, H);
	CHECK(g_ev[0].day == day(2026, 11, 4) && g_ev[0].start_minute == 660);

	/* the limit of year 9999: the trip is held back so that it ends in it
	 */
	trip_view(&v, 1, 8);
	g_ev[0] = (ctui_cal_event){day(9999, 12, 28), 600, 660, "Trip", 0};
	g_sp[0].extra_days = 2;
	ctui_cal_view_goto(&v, 9999, 12, 29, 600);
	ctui_cal_view_key(&v, 'm', H);
	for (int i = 0; i < 4; i++)
		ctui_cal_view_key(&v, KEY_RIGHT, H);
	CHECK(ctui_cal_event_end_abs(&g_ev[0], &g_sp[0]) <=
	      (day(9999, 12, 31) + 1) * DM);
	CHECK(g_ev[0].day == day(9999, 12, 29) && g_sp[0].extra_days == 2);
}

static void test_resize(void)
{
	ctui_cal_view v;
	trip_view(&v, 1, 8);
	/* mouse: the bottom edge (Thursday 10:30..11:00 row) to Saturday 12:00
	 */
	CHECK(ctui_cal_view_press(&v, W, H, 6, 30) == CTUI_CAL_VIEW_PICKED);
	CHECK(v.edit.grab == CTUI_CAL_GRAB_RESIZE && !v.edit.top);
	CHECK(ctui_cal_view_drag(&v, W, H, 8, 46) == CTUI_CAL_VIEW_RESIZED);
	CHECK(g_ev[0].day == day(2026, 11, 3) && g_ev[0].start_minute == 600 &&
	      g_sp[0].extra_days == 4 && g_ev[0].end_minute == 720);
	/* back to Wednesday early: it ends 09:00 on the 4th */
	CHECK(ctui_cal_view_drag(&v, W, H, 2, 22) == CTUI_CAL_VIEW_RESIZED);
	CHECK(g_sp[0].extra_days == 1 && g_ev[0].end_minute == 540);
	/* before the start: one slot after it, in the first day: a single-day
	 * event again */
	CHECK(ctui_cal_view_drag(&v, W, H, 1, 14) == CTUI_CAL_VIEW_RESIZED);
	CHECK(g_sp[0].extra_days == 0 && g_ev[0].start_minute == 600 &&
	      g_ev[0].end_minute == 630);
	CHECK(ctui_cal_view_cancel(&v, H) == CTUI_CAL_VIEW_CANCELLED);
	CHECK(g_sp[0].extra_days == 2 && g_ev[0].end_minute == 660);

	/* the top edge to Monday 09:00 */
	CHECK(ctui_cal_view_press(&v, W, H, 5, 14) == CTUI_CAL_VIEW_PICKED &&
	      v.edit.top);
	CHECK(ctui_cal_view_drag(&v, W, H, 3, 6) == CTUI_CAL_VIEW_RESIZED);
	CHECK(g_ev[0].day == day(2026, 11, 2) && g_ev[0].start_minute == 540 &&
	      g_sp[0].extra_days == 3 && g_ev[0].end_minute == 660);
	/* past the end: stops a slot before it */
	ctui_cal_view_drag(&v, W, H, 10, 46);
	CHECK(ctui_cal_event_end_abs(&g_ev[0], &g_sp[0]) >
	      ctui_cal_event_start_abs(&g_ev[0]));
	CHECK(g_ev[0].day == day(2026, 11, 5) && g_ev[0].start_minute == 630 &&
	      g_sp[0].extra_days == 0);
	ctui_cal_view_release(&v, W, H, 10, 46);

	/* a repeating trip keeps its start day */
	trip_view(&v, 1, 8);
	g_rc[0] =
	    (ctui_cal_recur){.repeat = CTUI_CAL_REPEAT_WEEKLY, .interval = 1};
	ctui_cal_view_press(&v, W, H, 5, 14);
	ctui_cal_view_drag(&v, W, H, 3, 6);
	CHECK(g_ev[0].day == day(2026, 11, 3) && g_ev[0].start_minute == 540 &&
	      g_sp[0].extra_days == 2);
	ctui_cal_view_cancel(&v, H);

	/* keys: r grabs the end, Down goes on into the next day at midnight */
	trip_view(&v, 1, 8);
	g_ev[0].end_minute = DM - 30;
	v.date = (ctui_date){2026, 11, 5};
	v.minute = 600;
	CHECK(ctui_cal_view_key(&v, 'r', H) == CTUI_CAL_VIEW_GRABBED);
	CHECK(ctui_cal_view_key(&v, KEY_DOWN, H) == CTUI_CAL_VIEW_RESIZED &&
	      g_ev[0].end_minute == DM);
	CHECK(ctui_cal_view_key(&v, KEY_DOWN, H) == CTUI_CAL_VIEW_RESIZED);
	CHECK(g_sp[0].extra_days == 3 && g_ev[0].end_minute == 30 &&
	      v.date.day == 6);
	CHECK(ctui_cal_view_key(&v, KEY_UP, H) == CTUI_CAL_VIEW_RESIZED &&
	      g_sp[0].extra_days == 2 && g_ev[0].end_minute == DM);
	/* Tab: the start edge; Up earlier, never at or after the end */
	CHECK(ctui_cal_view_key(&v, '\t', H) == CTUI_CAL_VIEW_GRABBED);
	CHECK(ctui_cal_view_key(&v, KEY_UP, H) == CTUI_CAL_VIEW_RESIZED &&
	      g_ev[0].start_minute == 570);
	CHECK(v.date.day == 3);
	CHECK(ctui_cal_view_key(&v, 27, H) == CTUI_CAL_VIEW_CANCELLED);
	CHECK(g_ev[0].start_minute == 600 && g_ev[0].end_minute == DM - 30 &&
	      g_sp[0].extra_days == 2);
	/* Shift+Down moves the end without a mode */
	v.date = (ctui_date){2026, 11, 4};
	v.minute = 600;
	CHECK(ctui_cal_view_key(&v, KEY_SF, H) == CTUI_CAL_VIEW_RESIZED &&
	      g_ev[0].end_minute == DM);
	CHECK(ctui_cal_view_key(&v, KEY_SR, H) == CTUI_CAL_VIEW_RESIZED &&
	      g_ev[0].end_minute == DM - 30);
}

static void test_place(void)
{
	ctui_cal_view v;
	trip_view(&v, 0, 8);
	ctui_cal_place p;
	ctui_cal_select_clear(&v.sel);
	/* nothing selected */
	p = ctui_cal_plan_repeat(&v.sel);
	CHECK(ctui_cal_view_place(&v, &p, 540, 600) == CTUI_CAL_VIEW_NONE &&
	      v.event_count == 0);
	/* Monday and Thursday: one weekly series */
	ctui_cal_select_add(&v.sel, day(2026, 11, 2));
	ctui_cal_select_add(&v.sel, day(2026, 11, 5));
	p = ctui_cal_plan_repeat(&v.sel);
	CHECK(ctui_cal_view_place(&v, &p, 600, 660) == CTUI_CAL_VIEW_CREATED &&
	      v.event_count == 1);
	CHECK(g_ev[0].day == day(2026, 11, 2) && g_ev[0].start_minute == 600 &&
	      g_ev[0].end_minute == 660 &&
	      g_rc[0].repeat == CTUI_CAL_REPEAT_WEEKLY &&
	      g_rc[0].weekdays == (1U << 0U | 1U << 3U) &&
	      g_sp[0].extra_days == 0 && v.selected == 0 && v.changed == 0);
	CHECK(ctui_cal_event_occurs(&g_ev[0], &g_rc[0], day(2026, 11, 12)));

	/* three dates in different weeks: one event each, or none at all when
	 * they do not fit */
	ctui_cal_select_clear(&v.sel);
	ctui_cal_select_add(&v.sel, day(2026, 11, 2));
	ctui_cal_select_add(&v.sel, day(2026, 11, 11));
	ctui_cal_select_add(&v.sel, day(2026, 11, 30));
	p = ctui_cal_plan_repeat(&v.sel);
	CHECK(p.kind == CTUI_CAL_PLACE_EACH);
	ctui_cal_select sel = v.sel;
	trip_view(&v, 6, 8);
	v.sel = sel;
	CHECK(ctui_cal_view_place(&v, &p, 600, 660) == CTUI_CAL_VIEW_FULL &&
	      v.event_count == 6);
	trip_view(&v, 2, 8);
	v.sel = sel;
	CHECK(ctui_cal_view_place(&v, &p, 600, 660) == CTUI_CAL_VIEW_CREATED &&
	      v.event_count == 5);
	CHECK(v.changed == 2 && v.selected == 2 &&
	      g_ev[2].day == day(2026, 11, 2) &&
	      g_ev[3].day == day(2026, 11, 11) &&
	      g_ev[4].day == day(2026, 11, 30) &&
	      g_rc[3].repeat == CTUI_CAL_REPEAT_NONE &&
	      g_sp[4].extra_days == 0);

	/* the 3rd to the 7th, all day: one event of five days */
	trip_view(&v, 0, 8);
	ctui_cal_select_clear(&v.sel);
	ctui_cal_select_set_range(&v.sel, day(2026, 11, 3), day(2026, 11, 7));
	p = ctui_cal_plan_span(&v.sel);
	CHECK(ctui_cal_view_place(&v, &p, 0, DM) == CTUI_CAL_VIEW_CREATED &&
	      v.event_count == 1);
	CHECK(g_ev[0].day == day(2026, 11, 3) && g_ev[0].start_minute == 0 &&
	      g_ev[0].end_minute == DM && g_sp[0].extra_days == 4);
	/* from 15:00 on the first to 10:00 on the last day */
	CHECK(ctui_cal_view_place(&v, &p, 900, 600) ==
	      CTUI_CAL_VIEW_NONE); /* a single-day end must follow the start */
	ctui_cal_select_set_range(&v.sel, day(2026, 12, 30), day(2027, 1, 2));
	p = ctui_cal_plan_span(&v.sel);
	CHECK(ctui_cal_view_place(&v, &p, 600, 660) == CTUI_CAL_VIEW_CREATED);
	CHECK(g_ev[1].day == day(2026, 12, 30) && g_sp[1].extra_days == 3 &&
	      g_ev[1].end_minute == 660);
	/* a view without spans cannot take it; a view at capacity is full */
	ctui_cal_view_set_editable_spans(&v, NULL);
	v.edit_spans = NULL;
	CHECK(ctui_cal_view_place(&v, &p, 600, 660) == CTUI_CAL_VIEW_NONE);
	trip_view(&v, 8, 8);
	CHECK(ctui_cal_view_create_range(&v, day(2026, 11, 3) * DM,
					 day(2026, 11, 4) * DM,
					 NULL) == CTUI_CAL_VIEW_FULL);
	trip_view(&v, 0, 8);
	CHECK(ctui_cal_view_create_range(&v, 100, 100, NULL) ==
	      CTUI_CAL_VIEW_NONE);
	CHECK(ctui_cal_view_create_range(&v, day(2026, 11, 3) * DM,
					 day(2026, 11, 3) * DM + 60,
					 NULL) == CTUI_CAL_VIEW_CREATED);
	CHECK(ctui_cal_view_key(&v, 'a', H) == CTUI_CAL_VIEW_CREATED &&
	      g_sp[1].extra_days == 0);
}

static void test_draw(void)
{
	screen_open(24, 80);
	screen_clear();
	ctui_cal_view v;
	trip_view(&v, 1, 8);
	ctui_cal_view_draw(&v, 0, 0, W, H);
	char row[16] = {0};
	/* Tuesday: the title and the time, a marker where it goes on */
	CHECK(screen_has_text("Trip"));
	mvinnstr(5, 14 + 6, row, 3);
	CHECK(strncmp(row, "\xe2\x96\xb8", 3) == 0);
	/* Wednesday: both markers; Thursday: the way in and the end time */
	mvinnstr(1, 22, row, 3);
	CHECK(strncmp(row, "\xe2\x97\x82", 3) == 0);
	mvinnstr(1, 22 + 6, row, 3);
	CHECK(strncmp(row, "\xe2\x96\xb8", 3) == 0);
	mvinnstr(1, 30, row, 3);
	CHECK(strncmp(row, "\xe2\x97\x82", 3) == 0);
	mvinnstr(1, 30 + 6, row, 1);
	CHECK(row[0] != '>' && strncmp(row, "\xe2\x96\xb8", 3) != 0);
	/* three blocks of the same colour */
	CHECK(PAIR_NUMBER(mvinch(8, 22) & A_COLOR) ==
	      PAIR_NUMBER((chtype)ctui_attr(CTUI_PAIR_DIALOG, false)));
	CHECK(PAIR_NUMBER(mvinch(2, 30) & A_COLOR) ==
	      PAIR_NUMBER((chtype)ctui_attr(CTUI_PAIR_DIALOG, false)));
	CHECK(PAIR_NUMBER(mvinch(7, 30) & A_COLOR) !=
	      PAIR_NUMBER((chtype)ctui_attr(CTUI_PAIR_DIALOG, false)));

	/* the day view shows the part on that day, with the markers at both
	 * ends */
	v.mode = CTUI_CAL_VIEW_DAY;
	v.date = (ctui_date){2026, 11, 4};
	screen_clear();
	ctui_cal_view_draw(&v, 0, 0, 40, H);
	CHECK(screen_has_text("Trip"));
	mvinnstr(1, 6, row, 3);
	CHECK(strncmp(row, "\xe2\x97\x82", 3) == 0);
	mvinnstr(1, 39, row, 3);
	CHECK(strncmp(row, "\xe2\x96\xb8", 3) == 0);
	v.date = (ctui_date){2026, 11, 3};
	screen_clear();
	ctui_cal_view_draw(&v, 0, 0, 40, H);
	CHECK(screen_has_text("10:00-") && !screen_has_text("-11:00"));
	v.date = (ctui_date){2026, 11, 5};
	screen_clear();
	ctui_cal_view_draw(&v, 0, 0, 40, H);
	CHECK(screen_has_text("-11:00") && !screen_has_text("10:00-"));
	v.date = (ctui_date){2026, 11, 6};
	screen_clear();
	ctui_cal_view_draw(&v, 0, 0, 40, H);
	CHECK(!screen_has_text("Trip"));

	/* a repeating trip: the repeat marker stays left of the continuing one
	 */
	g_rc[0] =
	    (ctui_cal_recur){.repeat = CTUI_CAL_REPEAT_WEEKLY, .interval = 1};
	v.mode = CTUI_CAL_VIEW_WEEK;
	v.date = (ctui_date){2026, 11, 11};
	screen_clear();
	ctui_cal_view_draw(&v, 0, 0, W, H);
	mvinnstr(5, 14 + 4, row, 3);
	CHECK(strncmp(row, "\xe2\x86\xbb", 3) == 0);
	mvinnstr(5, 14 + 6, row, 3);
	CHECK(strncmp(row, "\xe2\x96\xb8", 3) == 0);

	/* ASCII markers */
	setenv("CTUI_ASCII", "1", 1);
	ctui_theme_init();
	g_rc[0] = (ctui_cal_recur){0};
	v.date = (ctui_date){2026, 11, 4};
	screen_clear();
	ctui_cal_view_draw(&v, 0, 0, W, H);
	CHECK((mvinch(5, 14 + 6) & A_CHARTEXT) == '>' &&
	      (mvinch(1, 22) & A_CHARTEXT) == '<');
	CHECK(!screen_has_text("\xe2\x96\xb8"));
	unsetenv("CTUI_ASCII");
	endwin();
}

int main(void)
{
	RUN(test_layout);
	RUN(test_geometry_and_hit);
	RUN(test_delete);
	RUN(test_move);
	RUN(test_resize);
	RUN(test_place);
	RUN(test_draw);
	return CHECK_DONE();
}
