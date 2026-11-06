/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * Events in the cells of the full month grid: which events a day has and in
 * what order, how many rows they get, the clipped titles, drawing and the
 * hit test of a click.
 */
#include "check.h"
#include "screen.h"

#include <ctui/calendar_month.h>
#include <ctui/text.h>

static long day(int y, int m, int d)
{
	return ctui_date_to_days(y, m, d);
}

#define N 8
static ctui_cal_event g_ev[N];
static ctui_cal_recur g_rc[N];
static ctui_cal_span g_sp[N];
static ctui_cal_color g_co[N];

static ctui_cal_month_events events(int count)
{
	return (ctui_cal_month_events){g_ev, g_rc, g_sp, g_co, count, -1};
}

static void clear_events(void)
{
	memset(g_ev, 0, sizeof g_ev);
	memset(g_rc, 0, sizeof g_rc);
	memset(g_sp, 0, sizeof g_sp);
	memset(g_co, 0, sizeof g_co);
}

static void test_day_events(void)
{
	clear_events();
	long d5 = day(2026, 11, 5);
	g_ev[0] = (ctui_cal_event){d5, 14 * 60, 15 * 60, "Late", 0};
	g_ev[1] = (ctui_cal_event){d5, 9 * 60, 10 * 60, "Early", 0};
	g_ev[2] = (ctui_cal_event){d5 + 1, 8 * 60, 9 * 60, "Next day", 0};
	g_ev[3] = (ctui_cal_event){d5, 9 * 60, 11 * 60, "Same start", 0};
	ctui_cal_month_events ev = events(4);
	int idx[8];
	CHECK(ctui_cal_month_day_events(&ev, d5, idx, 8) == 3);
	CHECK(idx[0] == 1 && idx[1] == 3 &&
	      idx[2] == 0); /* by start, then by index */
	/* fewer slots than events: the first ones in order, the total returned
	 */
	int two[2] = {-1, -1};
	CHECK(ctui_cal_month_day_events(&ev, d5, two, 2) == 3 && two[0] == 1 &&
	      two[1] == 3);
	CHECK(ctui_cal_month_day_events(&ev, d5, NULL, 0) == 3);
	CHECK(ctui_cal_month_day_events(&ev, d5 + 1, idx, 8) == 1 &&
	      idx[0] == 2);
	CHECK(ctui_cal_month_day_events(&ev, d5 + 2, idx, 8) == 0);
	ev.count = 0;
	CHECK(ctui_cal_month_day_events(&ev, d5, idx, 8) == 0);
	CHECK(ctui_cal_month_day_events(NULL, d5, idx, 8) == 0);

	/* a series: its occurrences, minus the exceptions; weekly on Thursdays
	 * from the 5th */
	clear_events();
	g_ev[0] = (ctui_cal_event){d5, 600, 660, "Weekly", 0};
	g_rc[0] = (ctui_cal_recur){.repeat = CTUI_CAL_REPEAT_WEEKLY,
				   .interval = 1,
				   .exceptions = {d5 + 14},
				   .exception_count = 1};
	ev = events(1);
	CHECK(ctui_cal_month_day_events(&ev, d5, idx, 8) == 1);
	CHECK(ctui_cal_month_day_events(&ev, d5 + 7, idx, 8) == 1);
	CHECK(ctui_cal_month_day_events(&ev, d5 + 14, idx, 8) ==
	      0); /* skipped */
	CHECK(ctui_cal_month_day_events(&ev, d5 - 7, idx, 8) ==
	      0); /* before the anchor */
	CHECK(ctui_cal_month_day_events(&ev, d5 + 1, idx, 8) == 0);
	ev.recurs = NULL; /* no rules: the first day only */
	CHECK(ctui_cal_month_day_events(&ev, d5 + 7, idx, 8) == 0);

	/* an event of three days: every day it covers, from midnight after the
	 * first */
	clear_events();
	long d11 = day(2026, 11, 11);
	g_ev[0] = (ctui_cal_event){d11, 9 * 60, 17 * 60, "Conference", 0};
	g_sp[0] = (ctui_cal_span){.extra_days = 2};
	g_ev[1] = (ctui_cal_event){d11 + 1, 30, 60, "Night", 0};
	ev = events(2);
	CHECK(ctui_cal_month_day_events(&ev, d11, idx, 8) == 1);
	CHECK(ctui_cal_month_day_events(&ev, d11 + 1, idx, 8) == 2);
	CHECK(idx[0] == 0 &&
	      idx[1] == 1); /* the long one is there since 00:00 */
	CHECK(ctui_cal_month_day_events(&ev, d11 + 2, idx, 8) == 1 &&
	      idx[0] == 0);
	CHECK(ctui_cal_month_day_events(&ev, d11 + 3, idx, 8) == 0);
	ev.spans = NULL;
	CHECK(ctui_cal_month_day_events(&ev, d11 + 2, idx, 8) == 0);
}

static void test_rows(void)
{
	CHECK(ctui_cal_month_rows(0) == 0 && ctui_cal_month_rows(1) == 0 &&
	      ctui_cal_month_rows(-3) == 0);
	CHECK(ctui_cal_month_rows(2) == 1 && ctui_cal_month_rows(3) == 2);
	CHECK(ctui_cal_month_rows(4) == 2 && ctui_cal_month_rows(5) == 3 &&
	      ctui_cal_month_rows(10) == 8);

	int hidden;
	/* they fit */
	CHECK(ctui_cal_month_visible(3, 0, &hidden) == 0 && hidden == 0);
	CHECK(ctui_cal_month_visible(3, 2, &hidden) == 2 && hidden == 0);
	CHECK(ctui_cal_month_visible(3, 3, &hidden) == 3 && hidden == 0);
	/* they do not: the last row is "+N" */
	CHECK(ctui_cal_month_visible(3, 4, &hidden) == 2 && hidden == 2);
	CHECK(ctui_cal_month_visible(2, 3, &hidden) == 1 && hidden == 2);
	CHECK(ctui_cal_month_visible(1, 2, &hidden) == 0 && hidden == 2);
	CHECK(ctui_cal_month_visible(1, 1, &hidden) == 1 && hidden == 0);
	CHECK(ctui_cal_month_visible(8, 100, &hidden) == 7 && hidden == 93);
	/* no room at all, or odd input */
	CHECK(ctui_cal_month_visible(0, 5, &hidden) == 0 && hidden == 5);
	CHECK(ctui_cal_month_visible(-2, 5, &hidden) == 0 && hidden == 5);
	CHECK(ctui_cal_month_visible(3, -1, &hidden) == 0 && hidden == 0);
	CHECK(ctui_cal_month_visible(3, 5, NULL) == 2);
}

static bool valid_utf8(const char *s)
{
	for (size_t i = 0; s[i];) {
		unsigned long cp;
		size_t n = ctui_utf8_decode(s + i, &cp);
		if (cp == 0xFFFDul && !(n == 3 && (unsigned char)s[i] == 0xEF))
			return false;
		i += n;
	}
	return true;
}

static void test_clip(void)
{
	char out[64];
	CHECK(ctui_cal_month_clip("Planning", 8, NULL, out, sizeof out) == 8);
	CHECK_STR(out, "Planning");
	CHECK(ctui_cal_month_clip("Planning review", 10, NULL, out,
				  sizeof out) == 10);
	CHECK_STR(out, "Plannin...");
	CHECK(ctui_cal_month_clip("Planning review", 10, "\xE2\x80\xA6", out,
				  sizeof out) == 10);
	CHECK_STR(out, "Planning \xE2\x80\xA6");
	/* no room for more than the ellipsis, or not even for that */
	CHECK(ctui_cal_month_clip("Planning", 3, NULL, out, sizeof out) == 3);
	CHECK_STR(out, "...");
	CHECK(ctui_cal_month_clip("Planning", 2, NULL, out, sizeof out) == 2);
	CHECK_STR(out, "..");
	CHECK(ctui_cal_month_clip("Planning", 1, NULL, out, sizeof out) == 1);
	CHECK_STR(out, ".");
	CHECK(ctui_cal_month_clip("Planning", 0, NULL, out, sizeof out) == 0);
	CHECK_STR(out, "");
	CHECK(ctui_cal_month_clip("Planning", -4, NULL, out, sizeof out) == 0);
	CHECK(ctui_cal_month_clip(NULL, 5, NULL, out, sizeof out) == 0);
	CHECK_STR(out, "");
	CHECK(ctui_cal_month_clip("", 5, NULL, out, sizeof out) == 0);
	/* a title of exactly the width is kept, one more column is cut */
	CHECK(ctui_cal_month_clip("abcde", 5, NULL, out, sizeof out) == 5);
	CHECK_STR(out, "abcde");
	CHECK(ctui_cal_month_clip("abcdef", 5, NULL, out, sizeof out) == 5);
	CHECK_STR(out, "ab...");

	/* wide characters: two columns each, never split */
	const char *wide = "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E\xE3\x81\xAE"
			   "\xE3\x82\xBF"; /* 5 chars, 10 cols */
	CHECK(ctui_cal_month_clip(wide, 10, NULL, out, sizeof out) == 10);
	CHECK_STR(out, wide);
	CHECK(ctui_cal_month_clip(wide, 9, NULL, out, sizeof out) ==
	      9); /* 3 chars (6) + "..." */
	CHECK_STR(out, "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E...");
	int c = ctui_cal_month_clip(
	    wide, 8, NULL, out, sizeof out); /* 5 left: 2 chars (4) + ... = 7 */
	CHECK(c == 7);
	CHECK_STR(out, "\xE6\x97\xA5\xE6\x9C\xAC...");
	c = ctui_cal_month_clip(wide, 6, "\xE2\x80\xA6", out,
				sizeof out); /* 5 left: 2 chars + … = 5 */
	CHECK(c == 5 && valid_utf8(out));
	CHECK_STR(out, "\xE6\x97\xA5\xE6\x9C\xAC\xE2\x80\xA6");
	CHECK(ctui_cal_month_clip(wide, 4, NULL, out, sizeof out) ==
	      3); /* 1 left: no character fits */
	CHECK_STR(out, "...");
	CHECK(ctui_cal_month_clip(wide, 2, "\xE2\x80\xA6", out, sizeof out) <=
	      2);
	/* a wide ellipsis is cut by columns too */
	CHECK(ctui_cal_month_clip("abcdef", 3, "\xE3\x80\x9C\xE3\x80\x9C", out,
				  sizeof out) <= 3 &&
	      valid_utf8(out));
	/* mixed width and emoji */
	for (int cols = 0; cols < 20; cols++) {
		ctui_cal_month_clip("a\xF0\x9F\x98\x80"
				    "b\xE6\x97\xA5"
				    "cd\xF0\x9F\x98\x80"
				    "efgh",
				    cols, NULL, out, sizeof out);
		CHECK((int)ctui_utf8_columns(out) <= cols && valid_utf8(out));
	}
	/* a small output buffer is never overrun and stays valid */
	char tiny[6];
	ctui_cal_month_clip("Planning review", 10, NULL, tiny, sizeof tiny);
	CHECK(strlen(tiny) < sizeof tiny);
	CHECK(ctui_cal_month_clip("x", 3, NULL, tiny, 0) == 0);
	ctui_cal_month_clip(wide, 10, NULL, tiny,
			    3); /* cut inside a character... */
	CHECK(strlen(tiny) < 3);
}

/* The grid of November 2026 from Monday in 76 x 22 at (1, 2): rows of 3, 3, 4,
 * 3, 3, 4. */
#define AX 2
#define AY 1
#define AW 76
#define AH 22

static ctui_calendar make_cal(void)
{
	ctui_calendar cal;
	ctui_calendar_init(&cal, 2026, 11, 1);
	ctui_calendar_set_today(&cal, 2026, 11, 1);
	ctui_calendar_set_area(&cal, AW, AH);
	return cal;
}

static ctui_rect cell_of(const ctui_calendar *cal, int d)
{
	int row, col;
	ctui_calendar_cell(cal->year, cal->month, cal->week_start, d, &row,
			   &col);
	return ctui_calendar_full_cell(
	    AW, AH, ctui_calendar_weeks(cal->year, cal->month, cal->week_start),
	    row, col);
}

static void screen_text(int y, int x, int n, char *out)
{
	mvinnstr(y, x, out, n);
	out[n] = '\0';
}

static void test_draw(void)
{
	screen_open(24, 80);
	clear_events();
	long d5 = day(2026, 11, 5), d11 = day(2026, 11, 11);
	g_ev[0] = (ctui_cal_event){d5, 9 * 60, 10 * 60, "Alpha",
				   CTUI_PAIR_GAUGE_GREEN};
	g_ev[1] = (ctui_cal_event){d5, 11 * 60, 12 * 60, "Beta", 0};
	g_ev[2] = (ctui_cal_event){d5, 8 * 60, 8 * 60 + 30, "Gamma", 0};
	g_ev[3] = (ctui_cal_event){day(2026, 11, 6), 9 * 60, 10 * 60,
				   "A very long title indeed", 0};
	g_ev[4] = (ctui_cal_event){d11, 9 * 60, 17 * 60, "Conference", 0};
	g_sp[4] = (ctui_cal_span){.extra_days = 2};
	g_co[4].index = 3; /* yellow */
	g_ev[5] = (ctui_cal_event){day(2026, 11, 2), 600, 660, "Weekly", 0};
	g_rc[5] = (ctui_cal_recur){.repeat = CTUI_CAL_REPEAT_WEEKLY,
				   .interval = 1,
				   .exceptions = {day(2026, 11, 16)},
				   .exception_count = 1};
	ctui_cal_month_events ev = events(6);
	ctui_calendar cal = make_cal();
	char row[80];

	screen_clear();
	ctui_calendar_draw(&cal, AY, AX);
	ctui_cal_month_events_draw(&cal, &ev, AY, AX);

	/* the 5th has three events and two rows: the first by time, then "+2"
	 */
	ctui_rect c = cell_of(&cal, 5);
	CHECK(c.height == 3);
	screen_text(AY + c.y, AX + c.x, 2, row);
	CHECK_STR(row, "5 ");
	screen_text(AY + c.y + 1, AX + c.x + 2, 5, row);
	CHECK_STR(row, "Gamma"); /* 08:00 comes first */
	screen_text(AY + c.y + 2, AX + c.x + 1, 2, row);
	CHECK_STR(row, "+2");
	CHECK(!screen_has_text("Alpha") && !screen_has_text("Beta"));

	/* the 6th has one event, cut with the ellipsis at the width of the cell
	 */
	ctui_rect c6 = cell_of(&cal, 6);
	int iw = c6.width - 1;
	screen_text(AY + c6.y + 1, AX + c6.x + 2, iw - 2, row);
	CHECK(strncmp(row, "A very", 6) == 0 && strstr(row, "indeed") == NULL);

	/* the three days of the conference, on the yellow tone */
	int seen = 0;
	for (int d = 11; d <= 13; d++) {
		ctui_rect cd = cell_of(&cal, d);
		screen_text(AY + cd.y + 1, AX + cd.x + 1, 4, row);
		seen += strcmp(row, "Conf") == 0;
	}
	CHECK(seen == 3);
	CHECK(screen_has_color(ctui_cal_color_attr(3, false)));
	/* the 14th is free */
	ctui_rect c14 = cell_of(&cal, 14);
	screen_text(AY + c14.y + 1, AX + c14.x, 8, row);
	CHECK_STR(row, "        ");

	/* the series on Mondays, but not on the 16th (an exception) */
	int weekly = 0;
	for (int d = 2; d <= 30; d += 7) {
		ctui_rect cd = cell_of(&cal, d);
		screen_text(AY + cd.y + 1, AX + cd.x + 2, 6, row);
		weekly += strcmp(row, "Weekly") == 0;
	}
	CHECK(weekly == 4); /* 2, 9, 23, 30 */
	ctui_rect c16 = cell_of(&cal, 16);
	screen_text(AY + c16.y + 1, AX + c16.x + 2, 6, row);
	CHECK(strcmp(row, "Weekly") != 0);
	/* the marker of the green event is drawn in its pair */
	CHECK(screen_has_color(ctui_attr(CTUI_PAIR_GAUGE_GREEN, false)) ==
	      false);	       /* hidden in "+2" */
	g_ev[2].day = d5 + 30; /* one event less: Alpha is visible */
	g_ev[1].day = d5 + 30;
	screen_clear();
	ctui_calendar_draw(&cal, AY, AX);
	ctui_cal_month_events_draw(&cal, &ev, AY, AX);
	CHECK(screen_has_text("Alpha") &&
	      screen_has_color(ctui_attr(CTUI_PAIR_GAUGE_GREEN, false)));

	/* the picked event is underlined */
	ev.selected = 0;
	screen_clear();
	ctui_calendar_draw(&cal, AY, AX);
	ctui_cal_month_events_draw(&cal, &ev, AY, AX);
	CHECK((mvinch(AY + c.y + 1, AX + c.x + 3) & A_UNDERLINE) != 0);
	ev.selected = -1;

	/* the compact grid has no room for events */
	ctui_calendar_set_area(&cal, 0, 0);
	screen_clear();
	ctui_calendar_draw(&cal, 0, 0);
	ctui_cal_month_events_draw(&cal, &ev, 0, 0);
	CHECK(!screen_has_text("Alpha") && !screen_has_text("Conference"));
}

static void test_event_at(void)
{
	clear_events();
	long d5 = day(2026, 11, 5);
	g_ev[0] = (ctui_cal_event){d5, 9 * 60, 10 * 60, "Alpha", 0};
	g_ev[1] = (ctui_cal_event){d5, 11 * 60, 12 * 60, "Beta", 0};
	g_ev[2] = (ctui_cal_event){d5, 8 * 60, 8 * 60 + 30, "Gamma", 0};
	g_ev[3] =
	    (ctui_cal_event){day(2026, 11, 6), 9 * 60, 10 * 60, "Solo", 0};
	ctui_cal_month_events ev = events(4);
	ctui_calendar cal = make_cal();
	long dd = -1;

	ctui_rect c = cell_of(&cal, 5); /* 3 rows: number, Gamma, "+2" */
	CHECK(ctui_cal_month_event_at(&cal, &ev, c.y, c.x + 2, &dd) == -1 &&
	      dd == d5); /* the number */
	CHECK(ctui_cal_month_event_at(&cal, &ev, c.y + 1, c.x, &dd) == 2 &&
	      dd == d5); /* Gamma */
	CHECK(ctui_cal_month_event_at(&cal, &ev, c.y + 1, c.x + c.width - 2,
				      &dd) == 2); /* its end */
	CHECK(ctui_cal_month_event_at(&cal, &ev, c.y + 1, c.x + c.width - 1,
				      &dd) == -1 &&
	      dd == d5); /* the bar */
	CHECK(ctui_cal_month_event_at(&cal, &ev, c.y + 2, c.x + 1, &dd) == -1 &&
	      dd == d5); /* "+2" */
	ctui_rect c6 = cell_of(&cal, 6);
	CHECK(ctui_cal_month_event_at(&cal, &ev, c6.y + 1, c6.x + 3, &dd) ==
		  3 &&
	      dd == d5 + 1);
	CHECK(ctui_cal_month_event_at(&cal, &ev, c6.y + 2, c6.x + 3, &dd) ==
		  -1 &&
	      dd == d5 + 1);
	/* off the days: the header, the cells of other months, the outside */
	CHECK(ctui_cal_month_event_at(&cal, &ev, 0, 10, &dd) == -1 && dd == 0);
	CHECK(ctui_cal_month_event_at(&cal, &ev, 1, 10, &dd) == -1 && dd == 0);
	CHECK(ctui_cal_month_event_at(&cal, &ev, 3, 0, &dd) == -1 &&
	      dd == 0); /* a day of October */
	CHECK(ctui_cal_month_event_at(&cal, &ev, -1, 0, &dd) == -1 && dd == 0);
	CHECK(ctui_cal_month_event_at(&cal, &ev, 5, AW + 3, NULL) == -1);
	/* no events given */
	CHECK(ctui_cal_month_event_at(&cal, NULL, c.y + 1, c.x, &dd) == -1 &&
	      dd == d5);
	/* the compact grid: no event rows */
	ctui_calendar_set_area(&cal, 0, 0);
	CHECK(ctui_cal_month_event_at(&cal, &ev, 4, 2, &dd) == -1 && dd == 0);

	/* every row of every cell agrees with the drawing logic: shown events
	 * then "+N" */
	cal = make_cal();
	for (int d = 1; d <= 30; d++) {
		ctui_rect cd = cell_of(&cal, d);
		int idx[8];
		int rows = ctui_cal_month_rows(cd.height);
		int n =
		    ctui_cal_month_day_events(&ev, day(2026, 11, d), idx, 8);
		int shown = ctui_cal_month_visible(rows, n, NULL);
		for (int k = 0; k < rows; k++)
			CHECK(ctui_cal_month_event_at(&cal, &ev, cd.y + 1 + k,
						      cd.x, NULL) ==
			      (k < shown ? idx[k] : -1));
	}
}

static void test_taller_cells(void)
{
	/* 100 x 40: cells of 6 and 7 rows hold 4 or 5 events */
	clear_events();
	long d5 = day(2026, 11, 5);
	const char *names[6] = {"E0", "E1", "E2", "E3", "E4", "E5"};
	for (int i = 0; i < 6; i++)
		g_ev[i] = (ctui_cal_event){d5, 480 + 30 * i, 500 + 30 * i,
					   names[i], 0};
	ctui_cal_month_events ev = events(6);
	ctui_calendar cal;
	ctui_calendar_init(&cal, 2026, 11, 5);
	ctui_calendar_set_area(&cal, 100, 40);
	int weeks = ctui_calendar_weeks(2026, 11, 0);
	int row, col;
	ctui_calendar_cell(2026, 11, 0, 5, &row, &col);
	ctui_rect c = ctui_calendar_full_cell(100, 40, weeks, row, col);
	int rows = ctui_cal_month_rows(c.height), n = 6;
	int shown = ctui_cal_month_visible(rows, n, NULL);
	CHECK(c.height >= 5 && rows >= 3);
	for (int k = 0; k < shown; k++)
		CHECK(ctui_cal_month_event_at(&cal, &ev, c.y + 1 + k, c.x,
					      NULL) == k);
	if (n > rows)
		CHECK(ctui_cal_month_event_at(&cal, &ev, c.y + 1 + shown, c.x,
					      NULL) == -1);
}

int main(void)
{
	RUN(test_day_events);
	RUN(test_rows);
	RUN(test_clip);
	RUN(test_draw);
	RUN(test_event_at);
	RUN(test_taller_cells);
	return CHECK_DONE();
}
