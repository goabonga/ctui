/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * Event colours: the table of tones, the side table of a view, and what is
 * drawn.
 */
#include "check.h"
#include "screen.h"

#include <ctui/calendar_view.h>

#define DM 1440

static long day(int y, int m, int d)
{
	return ctui_date_to_days(y, m, d);
}

static void test_table(void)
{
	ctui_tone tone = CTUI_TONE_COUNT;
	CHECK(ctui_cal_color_valid(0) && ctui_cal_color_valid(CTUI_CAL_COLORS));
	CHECK(!ctui_cal_color_valid(-1) &&
	      !ctui_cal_color_valid(CTUI_CAL_COLORS + 1));
	CHECK(!ctui_cal_color_tone(0, &tone) && tone == CTUI_TONE_COUNT);
	CHECK(!ctui_cal_color_tone(CTUI_CAL_COLORS + 1, &tone) &&
	      tone == CTUI_TONE_COUNT);
	CHECK(ctui_cal_color_tone(1, &tone) && tone == CTUI_TONE_OK);
	CHECK(ctui_cal_color_tone(2, &tone) && tone == CTUI_TONE_BAD);
	CHECK(ctui_cal_color_tone(3, &tone) && tone == CTUI_TONE_BUSY);
	CHECK(ctui_cal_color_tone(4, &tone) && tone == CTUI_TONE_INFO);
	CHECK(ctui_cal_color_tone(5, &tone) && tone == CTUI_TONE_IDLE);
	CHECK(ctui_cal_color_tone(6, &tone) && tone == CTUI_TONE_DARK);
	CHECK(ctui_cal_color_tone(1, NULL));
	CHECK_STR(ctui_cal_color_name(0), "none");
	CHECK_STR(ctui_cal_color_name(1), "green");
	CHECK_STR(ctui_cal_color_name(6), "dark");
	CHECK_STR(ctui_cal_color_name(-4), "none");
	CHECK_STR(ctui_cal_color_name(99), "none");
}

static void test_next(void)
{
	CHECK(ctui_cal_color_next(0, 1) == 1);
	CHECK(ctui_cal_color_next(1, 1) == 2);
	CHECK(ctui_cal_color_next(CTUI_CAL_COLORS, 1) == 0); /* back to none */
	CHECK(ctui_cal_color_next(0, -1) == CTUI_CAL_COLORS);
	CHECK(ctui_cal_color_next(3, -1) == 2);
	CHECK(ctui_cal_color_next(-5, 1) == 1 &&
	      ctui_cal_color_next(99, 1) == 1); /* invalid is none */
	CHECK(ctui_cal_color_next(2, 0) == 2);
	CHECK(ctui_cal_color_next(1, 7) == 1 &&
	      ctui_cal_color_next(1, -7) == 1); /* full turns */
	int c = 0;
	for (int i = 0; i < CTUI_CAL_COLORS + 1; i++)
		c = ctui_cal_color_next(c, 1);
	CHECK(c == 0);
}

static ctui_cal_event g_ev[5];
static ctui_cal_recur g_rc[5];
static ctui_cal_color g_co[5];

static void colour_view(ctui_cal_view *v, int count)
{
	memset(g_ev, 0, sizeof g_ev);
	memset(g_rc, 0, sizeof g_rc);
	memset(g_co, 0, sizeof g_co);
	g_ev[0] = (ctui_cal_event){day(2026, 11, 4), 600, 660, "A", 0};
	g_ev[1] = (ctui_cal_event){day(2026, 11, 5), 600, 660, "B", 0};
	g_ev[2] = (ctui_cal_event){day(2026, 11, 6), 600, 660, "C", 0};
	ctui_cal_view_init(v, CTUI_CAL_VIEW_WEEK, 2026, 11, 4);
	v->top = 480;
	v->minute = 600;
	ctui_cal_view_set_editable(v, g_ev, g_rc, count, 5);
	ctui_cal_view_set_editable_colors(v, g_co);
}

static void test_view_table(void)
{
	ctui_cal_view v;
	colour_view(&v, 3);
	CHECK(ctui_cal_view_set_color(&v, 1, 2) && g_co[1].index == 2);
	CHECK(ctui_cal_view_set_color(&v, 2, 5));
	CHECK(!ctui_cal_view_set_color(&v, 3, 1)); /* no such event */
	CHECK(!ctui_cal_view_set_color(&v, -1, 1));
	CHECK(!ctui_cal_view_set_color(&v, 0, 7)); /* no such colour */
	CHECK(!ctui_cal_view_set_color(&v, 0, -1));
	CHECK(g_co[0].index == 0);

	/* a created event has no colour, even where an old one was left */
	g_co[3].index = 4;
	CHECK(ctui_cal_view_key(&v, 'a', 11) == CTUI_CAL_VIEW_CREATED &&
	      g_co[3].index == 0);

	/* deleting event 0 moves the colours down with their events */
	v.selected = 0;
	CHECK(ctui_cal_view_delete(&v, false) == CTUI_CAL_VIEW_DELETED);
	CHECK(v.event_count == 3 && g_co[0].index == 2 && g_co[1].index == 5);
	CHECK_STR(g_ev[0].title, "B");

	/* no editable colours: nothing to set */
	ctui_cal_view w;
	colour_view(&w, 3);
	w.edit_colors = NULL;
	CHECK(!ctui_cal_view_set_color(&w, 0, 1));
}

static void test_detach_copies_colour(void)
{
	ctui_cal_view v;
	colour_view(&v, 1);
	g_ev[0] = (ctui_cal_event){day(2026, 10, 28), 600, 660, "Weekly", 0};
	g_rc[0] =
	    (ctui_cal_recur){.repeat = CTUI_CAL_REPEAT_WEEKLY, .interval = 1};
	g_co[0].index = 3;
	v.ask_scope = true;
	v.date = (ctui_date){2026, 11, 11};
	ctui_cal_view_key(&v, '\n', 11);
	ctui_cal_view_key(&v, KEY_RIGHT, 11);
	CHECK(ctui_cal_view_key(&v, '\n', 11) == CTUI_CAL_VIEW_ASK_SCOPE);
	CHECK(ctui_cal_view_resolve_scope(&v, CTUI_CAL_SCOPE_OCCURRENCE, 11) ==
	      CTUI_CAL_VIEW_DETACHED);
	CHECK(v.changed == 1 && g_co[1].index == 3 && g_co[0].index == 3);
	CHECK(g_rc[1].repeat == CTUI_CAL_REPEAT_NONE);

	/* with the series scope the colour stays with the one event */
	colour_view(&v, 1);
	g_ev[0] = (ctui_cal_event){day(2026, 10, 28), 600, 660, "Weekly", 0};
	g_rc[0] =
	    (ctui_cal_recur){.repeat = CTUI_CAL_REPEAT_WEEKLY, .interval = 1};
	g_co[0].index = 6;
	v.ask_scope = true;
	v.date = (ctui_date){2026, 11, 11};
	ctui_cal_view_key(&v, '\n', 11);
	ctui_cal_view_key(&v, KEY_RIGHT, 11);
	ctui_cal_view_key(&v, '\n', 11);
	CHECK(ctui_cal_view_resolve_scope(&v, CTUI_CAL_SCOPE_SERIES, 11) ==
	      CTUI_CAL_VIEW_DROPPED);
	CHECK(v.event_count == 1 && g_co[0].index == 6);
}

static void test_draw(void)
{
	screen_open(24, 80);
	ctui_cal_view v;
	colour_view(&v, 1);
	g_ev[0].pair = CTUI_PAIR_GAUGE_GREEN;
	v.selected = -1;

	/* no colour: the pair of the event, no tone */
	screen_clear();
	ctui_cal_view_draw(&v, 0, 0, 62, 11);
	CHECK(screen_has_text("A"));
	CHECK(screen_has_color(ctui_attr(CTUI_PAIR_GAUGE_GREEN, false)));
	CHECK(!screen_has_color(ctui_cal_color_attr(2, false)));

	/* a colour replaces the pair and fills the block */
	g_co[0].index = 2;
	screen_clear();
	ctui_cal_view_draw(&v, 0, 0, 62, 11);
	CHECK(screen_has_color(ctui_cal_color_attr(2, false)));
	CHECK(!screen_has_color(ctui_attr(CTUI_PAIR_GAUGE_GREEN, false)));
	CHECK(ctui_cal_color_attr(0, false) == 0 &&
	      ctui_cal_color_attr(99, true) == 0);
	CHECK(ctui_cal_color_attr(1, false) != ctui_cal_color_attr(2, false));

	/* the colour table can also be read-only */
	ctui_cal_view r;
	ctui_cal_view_init(&r, CTUI_CAL_VIEW_WEEK, 2026, 11, 4);
	r.top = 480;
	ctui_cal_view_set_events(&r, g_ev, 1);
	ctui_cal_view_set_colors(&r, g_co);
	g_co[0].index = 4;
	screen_clear();
	ctui_cal_view_draw(&r, 0, 0, 62, 11);
	CHECK(screen_has_color(ctui_cal_color_attr(4, false)));
	(void)DM;
}

int main(void)
{
	RUN(test_table);
	RUN(test_next);
	RUN(test_view_table);
	RUN(test_detach_copies_colour);
	RUN(test_draw);
	return CHECK_DONE();
}
