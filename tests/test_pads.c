/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */
#include "check.h"
#include "screen.h"

#include <curses.h>

#include <ctui/pads.h>

static const char *const NAMES[16] = {
    "KICK", "SNR",  "HAT", "OH",  "TOM1", "TOM2", "CLP", "RIM",
    "CRSH", "RIDE", "COW", "SHK", "A",	  "B",	  "C",	 "D"};

static void test_init(void)
{
	ctui_pads p;
	ctui_pads_init(&p, 0, 0, NULL);
	CHECK(p.rows == 4 && p.cols == 4 && ctui_pads_count(&p) == 16);
	CHECK(p.cursor == 0 && p.held == -1 && !ctui_pads_active(&p));
	ctui_pads_init(&p, 2, 3, NAMES);
	CHECK(ctui_pads_count(&p) == 6);
	ctui_pads_init(&p, 99, 1, NULL);
	CHECK(p.rows == CTUI_PADS_MAX && p.cols == 1);
}

static void test_hit_and_decay(void)
{
	ctui_pads p;
	ctui_pads_init(&p, 0, 0, NAMES);
	CHECK(ctui_pads_hit(&p, 5, 0.5f) && p.level[5] == 0.5f &&
	      ctui_pads_active(&p));
	CHECK(ctui_pads_hit(&p, 6, 7.0f) && p.level[6] == 1.0f);
	CHECK(ctui_pads_hit(&p, 7, -1.0f) && p.level[7] == 0.0f);
	CHECK(!ctui_pads_hit(&p, 16, 1.0f));
	CHECK(ctui_pads_tick(&p,
			     10.0)); /* the first call only records the time */
	CHECK(p.level[5] == 0.5f && p.level[6] == 1.0f);
	CHECK(ctui_pads_tick(&p, 10.1));
	CHECK_NEAR(p.level[5], 0.1, 1e-5);
	CHECK_NEAR(p.level[6], 0.6, 1e-5);
	CHECK(ctui_pads_tick(&p,
			     10.0)); /* time going backwards changes nothing */
	CHECK_NEAR(p.level[6], 0.6, 1e-5);
	CHECK(ctui_pads_tick(&p, 10.2));
	CHECK(p.level[5] == 0.0f);
	CHECK_NEAR(p.level[6], 0.2, 1e-5);
	CHECK(!ctui_pads_tick(&p, 20.0) && !ctui_pads_active(&p));
}

static void test_move(void)
{
	ctui_pads p;
	ctui_pads_init(&p, 3, 4, NULL);
	ctui_pads_move(&p, 0, -1);
	CHECK(p.cursor == 0);
	ctui_pads_move(&p, 1, 1);
	CHECK(p.cursor == 5);
	ctui_pads_move(&p, 9, 9);
	CHECK(p.cursor == 11);
	ctui_pads_move(&p, -9, 0);
	CHECK(p.cursor == 3);
}

static void test_keys(void)
{
	ctui_pads p;
	int hit;
	ctui_pads_init(&p, 0, 0, NAMES);
	CHECK(ctui_pads_key(&p, KEY_RIGHT, &hit) && p.cursor == 1 && hit == -1);
	CHECK(ctui_pads_key(&p, KEY_DOWN, &hit) && p.cursor == 5);
	CHECK(ctui_pads_key(&p, KEY_LEFT, NULL) && p.cursor == 4);
	CHECK(ctui_pads_key(&p, KEY_UP, NULL) && p.cursor == 0);
	CHECK(ctui_pads_key(&p, KEY_END, NULL) && p.cursor == 15);
	CHECK(ctui_pads_key(&p, KEY_HOME, NULL) && p.cursor == 0);
	CHECK(ctui_pads_key(&p, KEY_END, NULL) &&
	      ctui_pads_key(&p, ' ', &hit) && hit == 15);
	CHECK_NEAR(p.level[15], 0.8, 1e-6);
	p.velocity = 1.0f;
	CHECK(ctui_pads_key(&p, '\n', &hit) && hit == 15 &&
	      p.level[15] == 1.0f);
	CHECK(ctui_pads_key(&p, KEY_ENTER, &hit) && hit == 15);
	CHECK(!ctui_pads_key(&p, 'x', &hit) && hit == -1);
	CHECK(!ctui_pads_key(&p, 'q', &hit) && hit == -1 &&
	      !p.level[4]); /* mapping off */
}

static void test_key_map(void)
{
	ctui_pads p;
	int hit;
	ctui_pads_init(&p, 0, 0, NULL);
	CHECK(ctui_pads_key_pad(&p, '1') == 0 &&
	      ctui_pads_key_pad(&p, '4') == 3);
	CHECK(ctui_pads_key_pad(&p, 'q') == 4 &&
	      ctui_pads_key_pad(&p, 'r') == 7);
	CHECK(ctui_pads_key_pad(&p, 'a') == 8 &&
	      ctui_pads_key_pad(&p, 'f') == 11);
	CHECK(ctui_pads_key_pad(&p, 'z') == 12 &&
	      ctui_pads_key_pad(&p, 'v') == 15);
	CHECK(ctui_pads_key_pad(&p, 'p') == -1 &&
	      ctui_pads_key_pad(&p, '5') == -1);
	p.key_map = true;
	CHECK(ctui_pads_key(&p, 'e', &hit) && hit == 6 && p.cursor == 6 &&
	      p.level[6] > 0);
	CHECK(!ctui_pads_key(&p, 'p', &hit) && hit == -1);
	ctui_pads small;
	ctui_pads_init(&small, 2, 3,
		       NULL); /* the mapping follows the columns of the grid */
	CHECK(ctui_pads_key_pad(&small, 'q') == 3 &&
	      ctui_pads_key_pad(&small, 'e') == 5);
	CHECK(ctui_pads_key_pad(&small, 'r') == -1 &&
	      ctui_pads_key_pad(&small, 'a') == -1);
	ctui_pads wide;
	ctui_pads_init(&wide, 4, 8, NULL);
	CHECK(ctui_pads_key_pad(&wide, 'q') == 8 &&
	      ctui_pads_key_pad(&wide, 'v') == 27);
}

static void test_cells(void)
{
	ctui_pads p;
	ctui_pads_init(&p, 0, 0, NULL);
	int y, x, w, h;
	/* 4 columns of 4 with 3 gaps = 19; 4 rows of 2 with 3 gaps = 11 */
	CHECK(ctui_pads_cell(&p, 19, 11, 0, &y, &x, &w, &h) && y == 0 &&
	      x == 0 && w == 4 && h == 2);
	CHECK(ctui_pads_cell(&p, 19, 11, 5, &y, &x, &w, &h) && y == 3 &&
	      x == 5);
	CHECK(ctui_pads_cell(&p, 19, 11, 15, &y, &x, &w, &h) && y == 9 &&
	      x == 15 && x + w == 19);
	CHECK(!ctui_pads_cell(&p, 19, 11, 16, &y, &x, &w, &h));
	CHECK(!ctui_pads_cell(&p, 6, 11, 0, &y, &x, &w, &h)); /* too narrow */
	CHECK(!ctui_pads_cell(&p, 19, 3, 0, &y, &x, &w, &h));
	/* 4 rows in 4 lines: no room for gaps, one line each */
	CHECK(ctui_pads_cell(&p, 19, 4, 4, &y, &x, &w, &h) && y == 1 && h == 1);
	CHECK(ctui_pads_cell(&p, 19, 7, 4, &y, &x, &w, &h) && y == 2 && h == 1);
}

static void test_at(void)
{
	ctui_pads p;
	ctui_pads_init(&p, 0, 0, NULL);
	CHECK(ctui_pads_at(&p, 19, 11, 0, 0) == 0);
	CHECK(ctui_pads_at(&p, 19, 11, 1, 3) == 0);
	CHECK(ctui_pads_at(&p, 19, 11, 1, 4) == -1); /* column gap */
	CHECK(ctui_pads_at(&p, 19, 11, 2, 0) == -1); /* row gap */
	CHECK(ctui_pads_at(&p, 19, 11, 3, 5) == 5);
	CHECK(ctui_pads_at(&p, 19, 11, 10, 18) == 15);
	CHECK(ctui_pads_at(&p, 19, 11, 11, 18) == -1);
	CHECK(ctui_pads_at(&p, 19, 11, -1, 0) == -1);
	CHECK(ctui_pads_at(&p, 19, 11, 0, 19) == -1);
	CHECK(ctui_pads_at(&p, 3, 3, 0, 0) == -1);
}

static void test_velocity_pair(void)
{
	CHECK_NEAR(ctui_pads_velocity(0, 5), 1.0, 1e-6);
	CHECK_NEAR(ctui_pads_velocity(4, 5), 0.4, 1e-6);
	CHECK_NEAR(ctui_pads_velocity(2, 5), 0.7, 1e-6);
	CHECK_NEAR(ctui_pads_velocity(0, 1), 0.8, 1e-6);
	CHECK_NEAR(ctui_pads_velocity(-3, 5), 1.0, 1e-6);
	CHECK_NEAR(ctui_pads_velocity(30, 5), 0.4, 1e-6);
	CHECK(ctui_pads_level_pair(0) == CTUI_PAIR_GAUGE_EMPTY);
	CHECK(ctui_pads_level_pair(0.1f) == CTUI_PAIR_GAUGE_GREEN);
	CHECK(ctui_pads_level_pair(0.5f) == CTUI_PAIR_GAUGE_YELLOW);
	CHECK(ctui_pads_level_pair(0.67f) == CTUI_PAIR_GAUGE_RED);
	CHECK(ctui_pads_level_pair(1.0f) == CTUI_PAIR_GAUGE_RED);
}

static void test_mouse(void)
{
	ctui_pads p;
	ctui_pads_init(&p, 0, 0, NAMES);
	CHECK(ctui_pads_press(&p, 19, 11, 0, 0) == 0);
	CHECK(p.held == 0 && p.cursor == 0 &&
	      p.level[0] == 1.0f); /* first row of the pad */
	CHECK(ctui_pads_press(&p, 19, 11, 4, 6) == 5);
	CHECK(p.held == 5 && p.cursor == 5);
	CHECK_NEAR(p.level[5], 0.4, 1e-6); /* second of two rows: softer */
	CHECK(ctui_pads_press(&p, 19, 11, 2, 0) == -1 &&
	      p.held == 5);			       /* a gap does nothing */
	CHECK(ctui_pads_drag(&p, 19, 11, 4, 7) == -1); /* still on pad 5 */
	CHECK(ctui_pads_drag(&p, 19, 11, 4, 11) == 6 && p.held == 6 &&
	      p.cursor == 6);
	CHECK(ctui_pads_drag(&p, 19, 11, 2, 11) == -1 &&
	      p.held == 6); /* over a gap */
	CHECK(ctui_pads_drag(&p, 19, 11, 50, 50) == -1 && p.held == 6);
	CHECK(ctui_pads_release(&p) && p.held == -1);
	CHECK(!ctui_pads_release(&p));
	CHECK(ctui_pads_drag(&p, 19, 11, 4, 11) ==
	      -1); /* no press held, no hit */
	float before = p.level[12];
	CHECK(ctui_pads_drag(&p, 19, 11, 9, 0) == -1 && p.level[12] == before);
}

static void test_draw(void)
{
	screen_open(24, 80);
	ctui_pads p;
	ctui_pads_init(&p, 0, 0, NAMES);
	screen_clear();
	ctui_pads_draw(&p, 0, 0, 40, 12);
	CHECK(screen_has_text("KICK") &&
	      screen_has_text("[KICK]")); /* the cursor frames its label */
	CHECK(screen_has_text("RIDE") && !screen_has_text("[RIDE]"));
	CHECK(screen_has_text("\xe2\x96\x91"));
	CHECK(screen_has_color(ctui_attr(CTUI_PAIR_SELECTION, true)));
	CHECK(!screen_has_color(ctui_attr(CTUI_PAIR_GAUGE_RED, false)));

	ctui_pads_hit(&p, 1, 1.0f);
	ctui_pads_hit(&p, 2, 0.5f);
	ctui_pads_hit(&p, 3, 0.2f);
	screen_clear();
	ctui_pads_draw(&p, 0, 0, 40, 12);
	CHECK(screen_has_text("\xe2\x96\x88"));
	CHECK(screen_has_color(ctui_attr(CTUI_PAIR_GAUGE_RED, true)));
	CHECK(screen_has_color(ctui_attr(CTUI_PAIR_GAUGE_YELLOW, false)));
	CHECK(screen_has_color(ctui_attr(CTUI_PAIR_GAUGE_GREEN, false)));

	/* no labels: the pads are numbered */
	ctui_pads_init(&p, 2, 2, NULL);
	screen_clear();
	ctui_pads_draw(&p, 0, 0, 20, 5);
	CHECK(screen_has_text("[1]") && screen_has_text(" 4 "));

	/* an area too small to hold the pads draws nothing, and nothing crashes
	 */
	screen_clear();
	ctui_pads_draw(&p, 0, 0, 1, 1);
	CHECK(!screen_has_text("1"));
	ctui_pads_draw(&p, 20, 70, 40, 12); /* partly off screen */

	/* a label wider than its pad is cut */
	ctui_pads_init(&p, 1, 2, NAMES);
	screen_clear();
	ctui_pads_draw(&p, 0, 0, 7, 1);
	CHECK(screen_has_text("KIC") && screen_has_text("SNR"));

	/* a wide-glyph label is centred by its columns, not its bytes */
	static const char *const WIDE[] = {"\xe6\x97\xa5\xe6\x9c\xac"};
	ctui_pads_init(&p, 1, 1, WIDE);
	screen_clear();
	ctui_pads_draw(&p, 0, 0, 10, 1);
	CHECK((mvinch(0, 2) & A_CHARTEXT) ==
	      '['); /* "[日本]" is 6 columns in 10 */
	/* a label longer than the buffer is cut on a character boundary */
	char longname[130];
	memset(longname, 'a', sizeof longname - 1);
	longname[sizeof longname - 1] = '\0';
	memcpy(longname + 62, "\xe6\x97\xa5", 3);
	const char *const LONG[] = {longname};
	ctui_pads_init(&p, 1, 1, LONG);
	screen_clear();
	ctui_pads_draw(&p, 0, 0, 80, 1);
	CHECK(!screen_has_text("\xef\xbf\xbd") && screen_has_text("aaaa"));

	/* the style of the instance replaces the colours and the glyphs */
	ctui_style hot = {0};
	hot.attr[CTUI_PAIR_GAUGE_RED] = ctui_attr_color(16, 214);
	hot.glyphs.gauge_filled = "@";
	ctui_pads_init(&p, 0, 0, NAMES);
	p.style = &hot;
	ctui_pads_hit(&p, 0, 1.0f);
	screen_clear();
	ctui_pads_draw(&p, 0, 0, 40, 12);
	CHECK(screen_has_text("@") && !screen_has_text("\xe2\x96\x88"));
	CHECK(screen_has_color(hot.attr[CTUI_PAIR_GAUGE_RED]));
	endwin();
}

int main(void)
{
	RUN(test_init);
	RUN(test_hit_and_decay);
	RUN(test_move);
	RUN(test_keys);
	RUN(test_key_map);
	RUN(test_cells);
	RUN(test_at);
	RUN(test_velocity_pair);
	RUN(test_mouse);
	RUN(test_draw);
	return CHECK_DONE();
}
