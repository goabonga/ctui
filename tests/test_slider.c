/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * test_slider.c - value mapping, keys, pointer handling and drawing.
 */
#include "check.h"
#include "screen.h"

#include <curses.h>
#include <string.h>

#include <ctui/slider.h>

static void test_init(void)
{
	ctui_slider s;
	ctui_slider_init(&s, "Vol", 0, 100, 1, 42.4);
	CHECK(s.value == 42 && s.decimals == 0 && !s.vertical && !s.dragging);
	ctui_slider_init(&s, NULL, 10, -10, 0.25, 0.3);
	CHECK(s.min == -10 && s.max == 10);
	CHECK_NEAR(s.value, 0.25, 1e-9);
	CHECK(s.decimals == 2);
	ctui_slider_init(&s, NULL, 0, 1, 0.1, 5);
	CHECK(s.value == 1 && s.decimals == 1);
	ctui_slider_init(&s, NULL, 0, 1, 0, 0.123);
	CHECK_NEAR(s.value, 0.123, 1e-12); /* continuous */
	CHECK(s.decimals == 2);		   /* the effective step is 0.01 */
	char text[32];
	ctui_slider_init(&s, NULL, 0, 1, 0, 0.5);
	CHECK(strcmp(ctui_slider_text(&s, 0.5, text, sizeof text), "0.50") ==
	      0);
	ctui_slider_init(&s, NULL, 0, 100, 0, 50);
	CHECK(s.decimals == 0);
}

static void test_snap_set(void)
{
	ctui_slider s;
	ctui_slider_init(&s, NULL, 0, 10, 2, 0);
	CHECK(ctui_slider_snap(&s, 3.1) == 4);
	CHECK(ctui_slider_snap(&s, 2.9) == 2);
	CHECK(ctui_slider_snap(&s, -5) == 0);
	CHECK(ctui_slider_snap(&s, 99) == 10);
	CHECK(ctui_slider_snap(&s, 0.0 / 0.0) == 0);
	CHECK(ctui_slider_set(&s, 5.5));
	CHECK(s.value == 6);
	CHECK(!ctui_slider_set(&s, 6.4));
	/* a range that the step does not divide still reaches the maximum */
	ctui_slider_init(&s, NULL, 0, 10, 3, 0);
	CHECK(ctui_slider_snap(&s, 10) == 10 && ctui_slider_snap(&s, 9) == 9 &&
	      ctui_slider_snap(&s, 9.9) == 10);
}

static void test_ratio_pos(void)
{
	ctui_slider s;
	ctui_slider_init(&s, NULL, 0, 100, 1, 0);
	CHECK(ctui_slider_ratio(&s) == 0 && ctui_slider_pos(&s, 11) == 0);
	ctui_slider_set(&s, 50);
	CHECK_NEAR(ctui_slider_ratio(&s), 0.5, 1e-12);
	CHECK(ctui_slider_pos(&s, 11) == 5);
	ctui_slider_set(&s, 100);
	CHECK(ctui_slider_pos(&s, 11) == 10);
	CHECK(ctui_slider_pos(&s, 1) == 0);
	CHECK(ctui_slider_pos(&s, 0) == -1);
	s.vertical = true; /* the maximum is at the top */
	CHECK(ctui_slider_pos(&s, 11) == 0);
	ctui_slider_set(&s, 0);
	CHECK(ctui_slider_pos(&s, 11) == 10);
	ctui_slider_init(&s, NULL, 5, 5, 1, 5); /* empty range */
	CHECK(ctui_slider_ratio(&s) == 0 && ctui_slider_pos(&s, 9) == 0);
}

static void test_value_at(void)
{
	ctui_slider s;
	ctui_slider_init(&s, NULL, 0, 100, 1, 0);
	CHECK(ctui_slider_value_at(&s, 11, 0) == 0);
	CHECK(ctui_slider_value_at(&s, 11, 5) == 50);
	CHECK(ctui_slider_value_at(&s, 11, 10) == 100);
	CHECK(ctui_slider_value_at(&s, 11, -4) == 0);
	CHECK(ctui_slider_value_at(&s, 11, 40) == 100);
	CHECK(ctui_slider_value_at(&s, 1, 0) == 0);
	s.vertical = true;
	CHECK(ctui_slider_value_at(&s, 11, 0) == 100);
	CHECK(ctui_slider_value_at(&s, 11, 10) == 0);
	CHECK(ctui_slider_value_at(&s, 11, 5) == 50);
	/* pos and value_at agree on every cell, both ways */
	for (int v = 0; v < 2; v++) {
		s.vertical = v;
		for (int cell = 0; cell < 21; cell++) {
			ctui_slider_set(&s, ctui_slider_value_at(&s, 21, cell));
			CHECK(ctui_slider_pos(&s, 21) == cell);
		}
	}
	/* coarse steps snap */
	ctui_slider_init(&s, NULL, 0, 100, 25, 0);
	CHECK(ctui_slider_value_at(&s, 11, 4) == 50);
}

static void test_text(void)
{
	char buf[40];
	ctui_slider s;
	ctui_slider_init(&s, NULL, -1, 1, 0.5, 0);
	CHECK_STR(ctui_slider_text(&s, 0.5, buf, sizeof buf), "0.5");
	CHECK_STR(ctui_slider_text(&s, -0.0, buf, sizeof buf), "0.0");
	CHECK_STR(ctui_slider_text(&s, -0.001, buf, sizeof buf), "0.0");
	CHECK_STR(ctui_slider_text(&s, -1, buf, sizeof buf), "-1.0");
}

static void test_keys(void)
{
	ctui_slider s;
	ctui_slider_init(&s, NULL, 0, 100, 5, 50);
	CHECK(ctui_slider_key(&s, KEY_RIGHT) && s.value == 55);
	CHECK(ctui_slider_key(&s, KEY_UP) && s.value == 60);
	CHECK(ctui_slider_key(&s, KEY_LEFT) && s.value == 55);
	CHECK(ctui_slider_key(&s, KEY_DOWN) && s.value == 50);
	CHECK(ctui_slider_key(&s, KEY_PPAGE) && s.value == 100);
	CHECK(ctui_slider_key(&s, KEY_RIGHT) &&
	      s.value == 100); /* handled at the end */
	CHECK(ctui_slider_key(&s, KEY_NPAGE) && s.value == 50);
	CHECK(ctui_slider_key(&s, KEY_HOME) && s.value == 0);
	CHECK(ctui_slider_key(&s, KEY_END) && s.value == 100);
	CHECK(!ctui_slider_key(&s, 'x') && s.value == 100);
	ctui_slider_init(&s, NULL, 0, 1, 0, 0.5); /* continuous: a hundredth */
	ctui_slider_key(&s, KEY_RIGHT);
	CHECK_NEAR(s.value, 0.51, 1e-9);
}

static void test_wheel(void)
{
	ctui_slider s;
	ctui_slider_init(&s, NULL, 0, 10, 1, 5);
	CHECK(ctui_slider_wheel(&s, 1) && s.value == 6);
	CHECK(ctui_slider_wheel(&s, -3) && s.value == 3);
	CHECK(ctui_slider_wheel(&s, -9) && s.value == 0);
	CHECK(!ctui_slider_wheel(&s, -1));
}

static void test_track_layout(void)
{
	ctui_slider s;
	int y, x, len;
	ctui_slider_init(&s, "Vol", 0, 100, 1, 0);
	ctui_slider_place(&s, 3, 5, 30, 1);
	/* "Vol " + track + " " + 3 value columns */
	CHECK(ctui_slider_track(&s, &y, &x, &len));
	CHECK(y == 3 && x == 9 && len == 30 - 4 - 1 - 3);
	ctui_slider_place(&s, 3, 5, 8, 1); /* too narrow */
	CHECK(!ctui_slider_track(&s, NULL, NULL, NULL));
	s.label = NULL;
	ctui_slider_place(&s, 3, 5, 20, 1);
	CHECK(ctui_slider_track(&s, &y, &x, &len) && x == 5 && len == 16);

	s.vertical = true;
	ctui_slider_place(&s, 2, 10, 7, 12);
	CHECK(ctui_slider_track(&s, &y, &x, &len));
	CHECK(y == 3 && x == 13 && len == 11);
	s.label = "Cut"; /* the label takes the last row */
	CHECK(ctui_slider_track(&s, NULL, NULL, &len) && len == 10);
	ctui_slider_place(&s, 2, 10, 7, 3);
	CHECK(!ctui_slider_track(&s, NULL, NULL, NULL));
}

static void test_mouse_horizontal(void)
{
	ctui_slider s;
	ctui_slider_init(&s, NULL, 0, 100, 1, 0);
	ctui_slider_place(&s, 4, 10, 21,
			  1); /* track: 10 .. 25 (16 cells) after the value */
	int ty, tx, len;
	CHECK(ctui_slider_track(&s, &ty, &tx, &len) && tx == 10 &&
	      len == 21 - 1 - 3);
	CHECK(!ctui_slider_over(&s, 3, 12) && !ctui_slider_over(&s, 4, 9) &&
	      !ctui_slider_over(&s, 4, 10 + len));
	CHECK(ctui_slider_over(&s, 4, 10) &&
	      ctui_slider_over(&s, 4, 10 + len - 1));
	CHECK(!ctui_slider_press(&s, 5, 12) && !s.dragging);

	CHECK(ctui_slider_press(&s, 4, 10 + len - 1) && s.dragging &&
	      s.value == 100);
	CHECK(ctui_slider_drag(&s, 9, 10) &&
	      s.value == 0); /* the row does not matter */
	CHECK(!ctui_slider_drag(&s, 4, 3) &&
	      s.value == 0); /* before the start: the minimum */
	CHECK(ctui_slider_drag(&s, 4, 200) &&
	      s.value == 100); /* far past the end: the maximum */
	CHECK(ctui_slider_release(&s) && !s.dragging);
	CHECK(!ctui_slider_release(&s));
	CHECK(!ctui_slider_drag(&s, 4, 10) &&
	      s.value == 100); /* no drag in progress */
}

static void test_mouse_vertical(void)
{
	ctui_slider s;
	ctui_slider_init(&s, "V", 0, 10, 1, 0);
	s.vertical = true;
	ctui_slider_place(&s, 2, 6, 5,
			  13); /* value row, 11 track rows 3 .. 13, label row */
	int ty, tx, len;
	CHECK(ctui_slider_track(&s, &ty, &tx, &len) && ty == 3 && tx == 8 &&
	      len == 11);
	CHECK(ctui_slider_over(&s, 3, 6) &&
	      ctui_slider_over(&s, 13, 10)); /* any column */
	CHECK(!ctui_slider_over(&s, 2, 8) && !ctui_slider_over(&s, 14, 8) &&
	      !ctui_slider_over(&s, 5, 11));
	CHECK(ctui_slider_press(&s, 3, 8) && s.value == 10);
	CHECK(ctui_slider_drag(&s, 8, 8) && s.value == 5);
	CHECK(ctui_slider_drag(&s, 99, 0) && s.value == 0);
	CHECK(ctui_slider_drag(&s, -4, 0) && s.value == 10);
	CHECK(ctui_slider_release(&s));
}

static void test_draw_horizontal(void)
{
	screen_open(24, 80);
	ctui_slider s;
	ctui_slider_init(&s, "Volume", 0, 100, 1, 50);
	ctui_slider_place(&s, 2, 4, 40, 1);
	screen_clear();
	ctui_slider_draw(&s, false);
	CHECK(screen_has_text("Volume"));
	CHECK(screen_has_text("50"));
	CHECK(screen_has_text("\xe2\x97\x8f")); /* knob */
	CHECK(screen_has_text("\xe2\x94\x81")); /* fill */
	CHECK(screen_has_text("\xe2\x94\x80")); /* rest of the track */
	CHECK(screen_has_color(ctui_attr(CTUI_PAIR_CURSOR, false)));

	/* at the minimum there is no fill; at the maximum no rest of the track
	 */
	ctui_slider_set(&s, 0);
	screen_clear();
	ctui_slider_draw(&s, false);
	CHECK(!screen_has_text("\xe2\x94\x81"));
	ctui_slider_set(&s, 100);
	screen_clear();
	ctui_slider_draw(&s, true);
	CHECK(!screen_has_text("\xe2\x94\x80") && screen_has_text("100"));
	CHECK(screen_has_color(ctui_attr(CTUI_PAIR_CURSOR_SELECTED, true)));

	/* the knob sits on the cell given by pos */
	int ty, tx, len;
	ctui_slider_set(&s, 25);
	ctui_slider_track(&s, &ty, &tx, &len);
	screen_clear();
	ctui_slider_draw(&s, false);
	char cell[8];
	mvinnstr(ty, tx + ctui_slider_pos(&s, len), cell, 3);
	CHECK(strncmp(cell, "\xe2\x97\x8f", 3) == 0);
}

static void test_draw_centre(void)
{
	screen_open(24, 80);
	ctui_slider s;
	ctui_slider_init(&s, "Pan", -1, 1, 0.1, 0);
	s.centre = true;
	ctui_slider_place(&s, 0, 0, 30, 1);
	screen_clear();
	ctui_slider_draw(&s, false);
	CHECK(screen_has_text("0.0"));
	/* knob on the centre: no fill, the knob replaces the mark */
	CHECK(!screen_has_text("\xe2\x94\x81") &&
	      screen_has_text("\xe2\x97\x8f"));
	ctui_slider_set(&s, -0.5);
	screen_clear();
	ctui_slider_draw(&s, false);
	CHECK(screen_has_text("\xe2\x94\x81") && screen_has_text("-0.5"));
	CHECK(
	    screen_has_text("\xe2\x94\x82")); /* the centre mark, not filled */
	ctui_slider_set(&s, 0.5);
	screen_clear();
	ctui_slider_draw(&s, false);
	CHECK(screen_has_text("\xe2\x94\x81") && screen_has_text("0.5"));
}

static void test_draw_vertical(void)
{
	screen_open(24, 80);
	ctui_slider s;
	ctui_slider_init(&s, "Cut", 0, 127, 1, 64);
	s.vertical = true;
	ctui_slider_place(&s, 1, 2, 5, 12);
	screen_clear();
	ctui_slider_draw(&s, false);
	CHECK(screen_has_text("64") && screen_has_text("Cut"));
	CHECK(screen_has_text("\xe2\x97\x8f") &&
	      screen_has_text("\xe2\x96\x88")); /* knob and fill */
	int ty, tx, len;
	ctui_slider_track(&s, &ty, &tx, &len);
	char cell[8];
	mvinnstr(ty + ctui_slider_pos(&s, len), tx, cell, 3);
	CHECK(strncmp(cell, "\xe2\x97\x8f", 3) == 0);
	/* the fill is below the knob, the empty part above it */
	mvinnstr(ty + len - 1, tx, cell, 3);
	CHECK(strncmp(cell, "\xe2\x96\x88", 3) == 0);
	mvinnstr(ty, tx, cell, 3);
	CHECK(strncmp(cell, "\xe2\x94\x82", 3) == 0);
	/* the label is on the last row */
	char row[80];
	mvinnstr(12, 0, row, 79);
	CHECK(strstr(row, "Cut") != NULL);
}

static void test_draw_ascii_and_style(void)
{
	screen_open(24, 80);
	setenv("CTUI_ASCII", "1", 1);
	ctui_theme_init();
	ctui_slider s;
	ctui_slider_init(&s, "Mix", 0, 10, 1, 5);
	ctui_slider_place(&s, 0, 0, 30, 1);
	screen_clear();
	ctui_slider_draw(&s, false);
	CHECK(screen_has_text("O") && screen_has_text("=") &&
	      screen_has_text("-"));
	CHECK(!screen_has_text("\xe2\x97\x8f"));

	ctui_style st = {0};
	st.glyphs.knob = "@";
	s.style = &st;
	screen_clear();
	ctui_slider_draw(&s, false);
	CHECK(screen_has_text("@") && !screen_has_text("O"));
	unsetenv("CTUI_ASCII");
	ctui_theme_init();
}

static void test_draw_small(void)
{
	screen_open(24, 80);
	ctui_slider s;
	ctui_slider_init(&s, "Longer label than fits", 0, 100, 1, 50);
	ctui_slider_place(&s, 0, 0, 6,
			  1); /* no track: only text, nothing breaks */
	screen_clear();
	ctui_slider_draw(&s, false);
	CHECK(!screen_has_text("\xe2\x97\x8f"));
	ctui_slider_place(&s, 0, 0, 0, 0);
	ctui_slider_draw(&s, true);
	s.vertical = true;
	ctui_slider_place(&s, 0, 0, 3, 2);
	ctui_slider_draw(&s, false);
	CHECK(true);
}

int main(void)
{
	RUN(test_init);
	RUN(test_snap_set);
	RUN(test_ratio_pos);
	RUN(test_value_at);
	RUN(test_text);
	RUN(test_keys);
	RUN(test_wheel);
	RUN(test_track_layout);
	RUN(test_mouse_horizontal);
	RUN(test_mouse_vertical);
	RUN(test_draw_horizontal);
	RUN(test_draw_centre);
	RUN(test_draw_vertical);
	RUN(test_draw_ascii_and_style);
	RUN(test_draw_small);
	return CHECK_DONE();
}
