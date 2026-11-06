/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */
#include "check.h"
#include "screen.h"

#include <curses.h>

#include <ctui/stepseq.h>

static uint32_t PATTERN[3];
static uint8_t VELOCITY[3 * 16];
static const char *const LABELS[3] = {"KICK", "SNARE", "A very long label"};

static void fixture(ctui_stepseq *s)
{
	PATTERN[0] = PATTERN[1] = PATTERN[2] = 0;
	for (size_t i = 0; i < sizeof VELOCITY; i++)
		VELOCITY[i] = 0;
	ctui_stepseq_init(s, PATTERN, 3, 16);
}

static void test_bits(void)
{
	CHECK(ctui_stepseq_bit_mask(0) == 0 && ctui_stepseq_bit_mask(1) == 1);
	CHECK(ctui_stepseq_bit_mask(16) == 0xFFFFu);
	CHECK(ctui_stepseq_bit_mask(32) == 0xFFFFFFFFu &&
	      ctui_stepseq_bit_mask(99) == 0xFFFFFFFFu);
	CHECK(!ctui_stepseq_bit_test(0, 0) && ctui_stepseq_bit_test(1, 0));
	CHECK(ctui_stepseq_bit_test(0x80000000u, 31) &&
	      !ctui_stepseq_bit_test(0xFFFFFFFFu, 32));
	CHECK(ctui_stepseq_bit_set(0, 5, true) == 0x20);
	CHECK(ctui_stepseq_bit_set(0x20, 5, false) == 0);
	CHECK(ctui_stepseq_bit_set(0x20, 5, true) == 0x20);
	CHECK(ctui_stepseq_bit_set(7, 40, false) == 7 &&
	      ctui_stepseq_bit_set(7, 32, true) == 7);
	CHECK(ctui_stepseq_bit_toggle(0, 31) == 0x80000000u);
	CHECK(ctui_stepseq_bit_toggle(0x80000000u, 31) == 0);
	CHECK(ctui_stepseq_bit_count(0, 16) == 0 &&
	      ctui_stepseq_bit_count(0xFFFFFFFFu, 32) == 32);
	CHECK(ctui_stepseq_bit_count(0xFFFFFFFFu, 16) ==
	      16); /* bits past the pattern are ignored */
	CHECK(ctui_stepseq_bit_count(0x1111, 16) == 4);
}

static void test_rotate(void)
{
	CHECK(ctui_stepseq_bit_rotate(0x0001, 16, 1) == 0x0002);
	CHECK(ctui_stepseq_bit_rotate(0x8000, 16, 1) == 0x0001);
	CHECK(ctui_stepseq_bit_rotate(0x0001, 16, -1) == 0x8000);
	CHECK(ctui_stepseq_bit_rotate(0x0003, 16, -3) == 0x6000);
	CHECK(ctui_stepseq_bit_rotate(0x1234, 16, 16) == 0x1234);
	CHECK(ctui_stepseq_bit_rotate(0x1234, 16, 0) == 0x1234);
	CHECK(ctui_stepseq_bit_rotate(0x1234, 16, 35) ==
	      ctui_stepseq_bit_rotate(0x1234, 16, 3));
	CHECK(ctui_stepseq_bit_rotate(0x80000001u, 32, 1) == 0x00000003u);
	CHECK(ctui_stepseq_bit_rotate(0x00000001u, 32, -1) == 0x80000000u);
	CHECK(ctui_stepseq_bit_rotate(0xABCD1234u, 32, 32) == 0xABCD1234u);
	CHECK(ctui_stepseq_bit_rotate(0xF0001, 16, 1) ==
	      0x0002); /* the bits above are dropped */
	CHECK(ctui_stepseq_bit_rotate(5, 1, 3) == 1);
	CHECK(ctui_stepseq_bit_rotate(5, 0, 3) == 0);
	CHECK(ctui_stepseq_bit_rotate(0x1234, 16, 7) ==
	      ctui_stepseq_bit_rotate(ctui_stepseq_bit_rotate(0x1234, 16, 3),
				      16, 4));
}

static void test_playhead(void)
{
	CHECK(ctui_stepseq_wrap(0, 16) == 0 && ctui_stepseq_wrap(15, 16) == 15);
	CHECK(ctui_stepseq_wrap(16, 16) == 0 && ctui_stepseq_wrap(35, 16) == 3);
	CHECK(ctui_stepseq_wrap(-1, 16) == 15 &&
	      ctui_stepseq_wrap(-17, 16) == 15);
	CHECK(ctui_stepseq_wrap(5, 0) == 0);
	CHECK(ctui_stepseq_next(0, 16) == 1 && ctui_stepseq_next(15, 16) == 0);
	CHECK(ctui_stepseq_next(31, 32) == 0 && ctui_stepseq_next(3, 0) == 0);
	CHECK(ctui_stepseq_group_start(0, 4) && ctui_stepseq_group_start(8, 4));
	CHECK(!ctui_stepseq_group_start(3, 4) &&
	      !ctui_stepseq_group_start(5, 4));
	CHECK(ctui_stepseq_group_start(0, 0) &&
	      !ctui_stepseq_group_start(4, 0));
	CHECK(ctui_stepseq_level(1) == 0 && ctui_stepseq_level(31) == 0);
	CHECK(ctui_stepseq_level(32) == 1 && ctui_stepseq_level(63) == 1);
	CHECK(ctui_stepseq_level(64) == 2 && ctui_stepseq_level(95) == 2);
	CHECK(ctui_stepseq_level(96) == 3 && ctui_stepseq_level(127) == 3);
}

static void test_init(void)
{
	ctui_stepseq s;
	fixture(&s);
	CHECK(s.tracks == 3 && s.steps == 16 && s.group == 4 && s.fresh == 100);
	CHECK(s.track == 0 && s.step == 0 && !s.show_playhead && !s.dragging);
	ctui_stepseq_init(&s, PATTERN, 2, 0);
	CHECK(s.steps == 1);
	ctui_stepseq_init(&s, PATTERN, 2, 64);
	CHECK(s.steps == 32);
	ctui_stepseq_init(&s, NULL, 4, 16);
	CHECK(s.tracks == 0);
	CHECK(!ctui_stepseq_key(&s, KEY_RIGHT));
	CHECK(!ctui_stepseq_press(&s, 1, 5));
	ctui_stepseq_clear(&s); /* nothing to clear, nothing to crash */
}

static void test_cells(void)
{
	ctui_stepseq s;
	fixture(&s);
	CHECK(!ctui_stepseq_on(&s, 0, 3));
	CHECK(ctui_stepseq_put(&s, 0, 3, true) && ctui_stepseq_on(&s, 0, 3));
	CHECK(!ctui_stepseq_put(&s, 0, 3, true)); /* unchanged */
	CHECK(PATTERN[0] == 8);
	CHECK(ctui_stepseq_put(&s, 0, 3, false) && PATTERN[0] == 0);
	CHECK(ctui_stepseq_flip(&s, 1, 15) && PATTERN[1] == 0x8000);
	CHECK(!ctui_stepseq_flip(&s, 1, 15) && PATTERN[1] == 0);
	CHECK(!ctui_stepseq_flip(&s, 3, 0) &&
	      !ctui_stepseq_flip(&s, 0, 16)); /* outside */
	CHECK(!ctui_stepseq_put(&s, 0, 16, true) && !ctui_stepseq_on(&s, 9, 9));
	CHECK(ctui_stepseq_velocity(&s, 0, 0) == 0);
	ctui_stepseq_flip(&s, 0, 0);
	CHECK(ctui_stepseq_velocity(&s, 0, 0) ==
	      CTUI_STEPSEQ_VELOCITY_MAX); /* no array: full level */
	CHECK(!ctui_stepseq_adjust(&s, 0, 0, 8));
}

static void test_velocity(void)
{
	ctui_stepseq s;
	fixture(&s);
	s.velocity = VELOCITY;
	ctui_stepseq_flip(&s, 1, 2);
	CHECK(VELOCITY[16 + 2] == 100 &&
	      ctui_stepseq_velocity(&s, 1, 2) == 100);
	CHECK(ctui_stepseq_adjust(&s, 1, 2, 8) && VELOCITY[18] == 108);
	CHECK(ctui_stepseq_adjust(&s, 1, 2, 100) && VELOCITY[18] == 127);
	CHECK(!ctui_stepseq_adjust(&s, 1, 2, 5));
	CHECK(ctui_stepseq_adjust(&s, 1, 2, -500) && VELOCITY[18] == 1);
	CHECK(!ctui_stepseq_adjust(&s, 1, 3, 8)); /* off */
	ctui_stepseq_flip(&s, 1, 2);		  /* off keeps the level... */
	CHECK(ctui_stepseq_velocity(&s, 1, 2) == 0 && VELOCITY[18] == 1);
	ctui_stepseq_flip(&s, 1, 2); /* ...and back on gets it again */
	CHECK(ctui_stepseq_velocity(&s, 1, 2) == 1);
	s.fresh = 300;
	ctui_stepseq_flip(&s, 2, 0);
	CHECK(VELOCITY[32] == 127);
	s.fresh = 0;
	ctui_stepseq_flip(&s, 2, 1);
	CHECK(VELOCITY[33] == 1);
	PATTERN[2] =
	    1U << 5U; /* an engine left a zero velocity on a lit step */
	CHECK(ctui_stepseq_velocity(&s, 2, 5) == 1);
}

static void test_clear_and_rotate(void)
{
	ctui_stepseq s;
	fixture(&s);
	s.velocity = VELOCITY;
	PATTERN[0] = 0x0003;
	PATTERN[1] = 0x8001;
	VELOCITY[0] = 10;
	VELOCITY[1] = 20;
	VELOCITY[2] = 30;
	ctui_stepseq_rotate(&s, 0, 2);
	CHECK(PATTERN[0] == 0x000C && VELOCITY[2] == 10 && VELOCITY[3] == 20 &&
	      VELOCITY[0] == 0);
	ctui_stepseq_rotate(&s, 0, -3);
	CHECK(PATTERN[0] == 0x8001);
	CHECK(VELOCITY[15] == 10 && VELOCITY[0] == 20);
	ctui_stepseq_rotate(&s, 9, 1); /* no such track */
	CHECK(PATTERN[1] == 0x8001);
	ctui_stepseq_clear_track(&s, 1);
	CHECK(PATTERN[1] == 0 && PATTERN[0] != 0);
	ctui_stepseq_clear_track(&s, 7);
	PATTERN[2] = 5;
	ctui_stepseq_clear(&s);
	CHECK(PATTERN[0] == 0 && PATTERN[1] == 0 && PATTERN[2] == 0);
}

static void test_keys(void)
{
	ctui_stepseq s;
	fixture(&s);
	s.velocity = VELOCITY;
	CHECK(ctui_stepseq_key(&s, KEY_LEFT) && s.step == 0);
	CHECK(ctui_stepseq_key(&s, KEY_UP) && s.track == 0);
	CHECK(ctui_stepseq_key(&s, KEY_RIGHT) && s.step == 1);
	CHECK(ctui_stepseq_key(&s, KEY_DOWN) && s.track == 1);
	CHECK(ctui_stepseq_key(&s, KEY_DOWN) &&
	      ctui_stepseq_key(&s, KEY_DOWN) && s.track == 2);
	CHECK(ctui_stepseq_key(&s, KEY_END) && s.step == 15);
	CHECK(ctui_stepseq_key(&s, KEY_RIGHT) && s.step == 15);
	CHECK(ctui_stepseq_key(&s, KEY_HOME) && s.step == 0);
	CHECK(ctui_stepseq_key(&s, ' ') && PATTERN[2] == 1);
	CHECK(ctui_stepseq_key(&s, '\n') && PATTERN[2] == 0);
	CHECK(ctui_stepseq_key(&s, KEY_ENTER) && PATTERN[2] == 1);
	CHECK(ctui_stepseq_key(&s, '+') && VELOCITY[32] == 108);
	CHECK(ctui_stepseq_key(&s, '-') && ctui_stepseq_key(&s, '-') &&
	      VELOCITY[32] == 92);
	CHECK(ctui_stepseq_key(&s, '[') && PATTERN[2] == 0x8000);
	CHECK(ctui_stepseq_key(&s, ']') && PATTERN[2] == 1);
	CHECK(ctui_stepseq_key(&s, KEY_DC) && PATTERN[2] == 0);
	PATTERN[0] = PATTERN[1] = PATTERN[2] = 0xFFFF;
	CHECK(ctui_stepseq_key(&s, 'c') && PATTERN[2] == 0 &&
	      PATTERN[1] == 0xFFFF);
	CHECK(ctui_stepseq_key(&s, 'C') && PATTERN[0] == 0 && PATTERN[1] == 0);
	CHECK(!ctui_stepseq_key(&s, 'x') && !ctui_stepseq_key(&s, KEY_F(1)));
	s.track = 9; /* a stale cursor is pulled back */
	s.step = 99;
	CHECK(ctui_stepseq_key(&s, KEY_LEFT) && s.track == 2 && s.step == 14);
}

static void test_geometry(void)
{
	ctui_stepseq s;
	fixture(&s);
	s.labels = LABELS;
	CHECK(ctui_stepseq_label_width(&s) == CTUI_STEPSEQ_LABEL_MAX);
	s.labels = NULL;
	CHECK(ctui_stepseq_label_width(&s) == 2); /* T1 .. T3 */
	static const char *const SHORT[3] = {"BD", "SD", "CH"};
	s.labels = SHORT;
	CHECK(ctui_stepseq_label_width(&s) == 2);
	CHECK(ctui_stepseq_x(&s, 0) == 3 && ctui_stepseq_x(&s, 3) == 9);
	CHECK(ctui_stepseq_x(&s, 4) == 12); /* a gap before the second group */
	CHECK(ctui_stepseq_x(&s, 15) == 3 + 30 + 3);
	CHECK(ctui_stepseq_width(&s) == 3 + 30 + 3 + 2);
	CHECK(ctui_stepseq_height(&s) == 4);
	s.group = 0;
	CHECK(ctui_stepseq_x(&s, 4) == 11 && ctui_stepseq_width(&s) == 3 + 32);
	s.group = 4;
	unsigned kept = s.steps;
	s.steps = 0; /* misuse after init must not overflow */
	CHECK(ctui_stepseq_width(&s) == ctui_stepseq_x(&s, 0));
	s.steps = kept;
	unsigned t = 9, st = 9;
	CHECK(ctui_stepseq_hit(&s, 1, 3, &t, &st) && t == 0 && st == 0);
	CHECK(ctui_stepseq_hit(&s, 1, 4, &t, &st) && st == 0);
	CHECK(ctui_stepseq_hit(&s, 3, 12, &t, &st) && t == 2 && st == 4);
	CHECK(ctui_stepseq_hit(&s, 2, 36, &t, &st) && t == 1 && st == 15);
	CHECK(ctui_stepseq_hit(&s, 2, 36, NULL, NULL));
	CHECK(!ctui_stepseq_hit(&s, 2, 11, &t, &st)); /* the gap */
	CHECK(!ctui_stepseq_hit(&s, 2, 2, &t, &st));  /* the labels */
	CHECK(!ctui_stepseq_hit(&s, 0, 3, &t, &st));  /* the header */
	CHECK(!ctui_stepseq_hit(&s, 4, 3, &t, &st));  /* below the last track */
	CHECK(!ctui_stepseq_hit(&s, 1, 38, &t, &st)); /* right of the grid */
	CHECK(!ctui_stepseq_hit(&s, 1, -1, &t, &st));
}

static void test_mouse(void)
{
	ctui_stepseq s;
	fixture(&s);
	s.velocity = VELOCITY;
	s.labels = (const char *const[]){"BD", "SD", "CH"};
	CHECK(!ctui_stepseq_press(&s, 0, 3) && !s.dragging);
	CHECK(ctui_stepseq_press(&s, 1, 3) && s.dragging && PATTERN[0] == 1 &&
	      s.paint);
	CHECK(ctui_stepseq_drag(&s, 1, 5) && PATTERN[0] == 3 && s.step == 1);
	CHECK(ctui_stepseq_drag(&s, 2, 6) && PATTERN[1] == 2 && s.track == 1);
	CHECK(ctui_stepseq_drag(&s, 2, 0) &&
	      PATTERN[1] == 2); /* outside: nothing painted */
	CHECK(ctui_stepseq_release(&s) && !s.dragging);
	CHECK(!ctui_stepseq_release(&s) && !ctui_stepseq_drag(&s, 1, 3));
	/* A press on a lit cell turns it off and the drag erases. */
	CHECK(ctui_stepseq_press(&s, 1, 3) && PATTERN[0] == 2 && !s.paint);
	ctui_stepseq_drag(&s, 1, 5);
	CHECK(PATTERN[0] == 0);
	ctui_stepseq_release(&s);
	/* Wheel: velocity of the cell under the pointer. */
	ctui_stepseq_put(&s, 2, 0, true);
	CHECK(ctui_stepseq_wheel(&s, 3, 3, 2) && VELOCITY[32] == 116);
	CHECK(ctui_stepseq_wheel(&s, 3, 3, -1) && VELOCITY[32] == 108);
	CHECK(!ctui_stepseq_wheel(&s, 3, 5, 1)); /* off cell */
	CHECK(!ctui_stepseq_wheel(&s, 3, 0, 1)); /* labels */
}

static void test_draw(void)
{
	ctui_stepseq s;
	fixture(&s);
	s.labels = (const char *const[]){"KICK", "SNARE", "A very long label"};
	screen_open(24, 80);
	PATTERN[0] = 0x1111;
	screen_clear();
	ctui_stepseq_draw(&s, 2, 1, 70, 10);
	CHECK(screen_has_text("KICK") && screen_has_text("SNARE") &&
	      screen_has_text("A very l"));
	CHECK(!screen_has_text("A very lo")); /* labels are cut */
	CHECK(screen_has_text("13") && screen_has_text("9"));
	CHECK(screen_has_text("\xe2\x96\x88") && screen_has_text("\xc2\xb7"));
	CHECK(screen_has_color(
	    ctui_attr(CTUI_PAIR_SELECTION, true))); /* cursor on a lit cell */
	CHECK(!screen_has_color(ctui_attr(CTUI_PAIR_PLAYHEAD, false)));
	s.hide_cursor = true;
	screen_clear();
	ctui_stepseq_draw(&s, 2, 1, 70, 10);
	CHECK(!screen_has_color(ctui_attr(CTUI_PAIR_SELECTION, true)));
	s.hide_cursor = false;
	s.show_playhead = true;
	s.playhead = 5;
	screen_clear();
	ctui_stepseq_draw(&s, 2, 1, 70, 10);
	CHECK(screen_has_color(ctui_attr(CTUI_PAIR_PLAYHEAD, false)));
	CHECK(screen_has_color(ctui_attr(CTUI_PAIR_GAUGE_GREEN_BRIGHT, true)));

	/* Velocity shows as the height of the glyph. */
	s.velocity = VELOCITY;
	s.show_playhead = false;
	PATTERN[0] = 0xF;
	VELOCITY[0] = 10;
	VELOCITY[1] = 40;
	VELOCITY[2] = 70;
	VELOCITY[3] = 120;
	screen_clear();
	ctui_stepseq_draw(&s, 2, 1, 70, 10);
	CHECK(screen_has_text("\xe2\x96\x82") &&
	      screen_has_text("\xe2\x96\x84"));
	CHECK(screen_has_text("\xe2\x96\x86") &&
	      screen_has_text("\xe2\x96\x88"));
	CHECK(screen_has_color(ctui_attr(CTUI_PAIR_GAUGE_YELLOW_BRIGHT, true)));

	/* Clipping: a narrow area draws what fits and nothing outside. */
	screen_clear();
	ctui_stepseq_draw(&s, 0, 0, 12, 2);
	CHECK(screen_has_text("KICK"));
	CHECK(
	    !screen_has_text("SNARE")); /* only one track row fits in 2 rows */
	screen_clear();
	ctui_stepseq_draw(&s, 0, 0, 0, 5);
	ctui_stepseq_draw(&s, 0, 0, 20, 0);
	CHECK(!screen_has_text("KICK"));
	ctui_stepseq_init(&s, NULL, 0, 16);
	ctui_stepseq_draw(&s, 0, 0, 40, 5); /* no tracks: nothing */

	/* The instance style replaces the cell colours and does not leak. */
	ctui_style style = {0};
	style.attr[CTUI_PAIR_GAUGE_GREEN_BRIGHT] = ctui_attr_color(201, -1);
	ctui_stepseq_init(&s, PATTERN, 3, 16);
	PATTERN[0] = 0xFF;
	s.track = 2;
	s.style = &style;
	screen_clear();
	ctui_stepseq_draw(&s, 0, 0, 70, 6);
	CHECK(screen_has_color(style.attr[CTUI_PAIR_GAUGE_GREEN_BRIGHT]));
	CHECK(ctui_attr(CTUI_PAIR_GAUGE_GREEN_BRIGHT, true) != 0);
	CHECK(PATTERN[0] == 0xFF);
	endwin();
}

static void test_ascii(void)
{
	setenv("CTUI_ASCII", "1", 1);
	ctui_theme_init();
	ctui_stepseq s;
	fixture(&s);
	PATTERN[1] = 0x3;
	s.track = 2;
	screen_clear();
	ctui_stepseq_draw(&s, 0, 0, 70, 6);
	CHECK(screen_has_text("#") && screen_has_text("."));
	CHECK(!screen_has_text("\xe2\x96\x88") && !screen_has_text("\xc2\xb7"));
	s.velocity = VELOCITY;
	VELOCITY[16] = 10;
	VELOCITY[17] = 50;
	screen_clear();
	ctui_stepseq_draw(&s, 0, 0, 70, 6);
	CHECK(screen_has_text("-") && screen_has_text("="));
	unsetenv("CTUI_ASCII");
}

int main(void)
{
	RUN(test_bits);
	RUN(test_rotate);
	RUN(test_playhead);
	RUN(test_init);
	RUN(test_cells);
	RUN(test_velocity);
	RUN(test_clear_and_rotate);
	RUN(test_keys);
	RUN(test_geometry);
	RUN(test_mouse);
	RUN(test_draw);
	RUN(test_ascii);
	return CHECK_DONE();
}
