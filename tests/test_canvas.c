/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * test_canvas.c - dots, lines, shapes, Braille glyphs and axis scales.
 */
#include "check.h"
#include "screen.h"
#include <ctui/canvas.h>

static void test_dots(void)
{
	ctui_canvas c;
	CHECK(!ctui_canvas_init(&c, 0, 3));
	CHECK(ctui_canvas_init(&c, 4, 2));
	CHECK(ctui_canvas_width(&c) == 8 && ctui_canvas_height(&c) == 8);
	CHECK(!ctui_canvas_get(&c, 3, 5));
	ctui_canvas_set(&c, 3, 5, CTUI_ICON_GREEN);
	CHECK(ctui_canvas_get(&c, 3, 5) && !ctui_canvas_get(&c, 2, 5));
	ctui_canvas_set(&c, -1, 0, CTUI_ICON_BLUE); /* clipped */
	ctui_canvas_set(&c, 8, 0, CTUI_ICON_BLUE);
	ctui_canvas_set(&c, 0, 8, CTUI_ICON_BLUE);
	CHECK(!ctui_canvas_get(&c, 8, 0) && !ctui_canvas_get(&c, -1, 0));
	ctui_canvas_clear(&c);
	CHECK(!ctui_canvas_get(&c, 3, 5));
	ctui_canvas_free(&c);
}

static void test_glyph(void)
{
	char g[4];
	ctui_canvas_glyph(0, g);
	CHECK_STR(g, "\xe2\xa0\x80");
	ctui_canvas_glyph(0x01, g); /* top left dot */
	CHECK_STR(g, "\xe2\xa0\x81");
	ctui_canvas_glyph(0xFF, g);
	CHECK_STR(g, "\xe2\xa3\xbf");
	ctui_canvas_glyph(0x40, g); /* bottom left dot */
	CHECK_STR(g, "\xe2\xa1\x80");
}

static void test_line(void)
{
	ctui_canvas c;
	CHECK(ctui_canvas_init(&c, 5, 3));
	ctui_canvas_line(&c, 0, 0, 9, 0, CTUI_ICON_BLUE);
	for (int x = 0; x < 10; x++)
		CHECK(ctui_canvas_get(&c, x, 0));
	CHECK(!ctui_canvas_get(&c, 0, 1));
	ctui_canvas_clear(&c);
	ctui_canvas_line(&c, 0, 0, 4, 4, CTUI_ICON_BLUE); /* diagonal */
	for (int i = 0; i < 5; i++)
		CHECK(ctui_canvas_get(&c, i, i));
	ctui_canvas_clear(&c);
	ctui_canvas_line(&c, 6, 7, 1, 2,
			 CTUI_ICON_BLUE); /* either direction, ends included */
	CHECK(ctui_canvas_get(&c, 6, 7) && ctui_canvas_get(&c, 1, 2));
	ctui_canvas_line(&c, 3, 3, 3, 3, CTUI_ICON_BLUE); /* a point */
	CHECK(ctui_canvas_get(&c, 3, 3));
	ctui_canvas_free(&c);
}

static int count_dots(const ctui_canvas *c)
{
	int n = 0;
	for (int y = 0; y < ctui_canvas_height(c); y++)
		for (int x = 0; x < ctui_canvas_width(c); x++)
			n += ctui_canvas_get(c, x, y);
	return n;
}

static void test_rect(void)
{
	ctui_canvas c;
	CHECK(ctui_canvas_init(&c, 5, 3)); /* 10 x 12 dots */
	ctui_canvas_rect(&c, 1, 1, 6, 4, CTUI_ICON_BLUE, false);
	CHECK(count_dots(&c) == 6 * 4 - 4 * 2); /* 6 x 4 outline */
	CHECK(ctui_canvas_get(&c, 1, 1) && ctui_canvas_get(&c, 6, 4) &&
	      ctui_canvas_get(&c, 6, 1));
	CHECK(!ctui_canvas_get(&c, 3, 2));
	ctui_canvas_clear(&c);
	ctui_canvas_rect(&c, 6, 4, 1, 1, CTUI_ICON_BLUE,
			 true); /* corners in any order */
	CHECK(count_dots(&c) == 24 && ctui_canvas_get(&c, 3, 2));
	ctui_canvas_clear(&c);
	ctui_canvas_rect(&c, 3, 3, 3, 3, CTUI_ICON_BLUE, false); /* one dot */
	CHECK(count_dots(&c) == 1);
	ctui_canvas_clear(&c);
	ctui_canvas_rect(&c, -100, -100, 100, 100, CTUI_ICON_BLUE,
			 true); /* clipped */
	CHECK(count_dots(&c) == 120);
	ctui_canvas_clear(&c);
	ctui_canvas_rect(&c, -100, -100, 100, 100, CTUI_ICON_BLUE,
			 false); /* outline is off canvas */
	CHECK(count_dots(&c) == 0);
	ctui_canvas_free(&c);
}

static void test_circle(void)
{
	ctui_canvas c;
	CHECK(ctui_canvas_init(&c, 10, 6)); /* 20 x 24 dots */
	ctui_canvas_circle(&c, 10, 12, 5, CTUI_ICON_ORANGE, false);
	CHECK(ctui_canvas_get(&c, 15, 12) && ctui_canvas_get(&c, 5, 12));
	CHECK(ctui_canvas_get(&c, 10, 17) && ctui_canvas_get(&c, 10, 7));
	CHECK(!ctui_canvas_get(&c, 10, 12)); /* hollow */
	CHECK(!ctui_canvas_get(&c, 16, 12) && !ctui_canvas_get(&c, 10, 18));
	for (int y = 0; y < 24; y++) /* symmetric */
		for (int x = 0; x < 20; x++)
			CHECK(ctui_canvas_get(&c, x, y) ==
				  ctui_canvas_get(&c, 20 - x, y) ||
			      x == 0);
	ctui_canvas_clear(&c);
	ctui_canvas_circle(&c, 10, 12, 5, CTUI_ICON_ORANGE, true);
	CHECK(ctui_canvas_get(&c, 10, 12) && ctui_canvas_get(&c, 13, 14));
	CHECK(!ctui_canvas_get(&c, 16, 12) && !ctui_canvas_get(&c, 14, 16));
	int area = count_dots(&c);
	CHECK(area >= 80 && area <= 110); /* a disc of radius 5 or so */
	ctui_canvas_clear(&c);
	ctui_canvas_circle(&c, 4, 4, 0, CTUI_ICON_ORANGE, false);
	CHECK(count_dots(&c) == 1 && ctui_canvas_get(&c, 4, 4));
	ctui_canvas_clear(&c);
	ctui_canvas_circle(&c, 4, 4, -1, CTUI_ICON_ORANGE, true);
	CHECK(count_dots(&c) == 0);
	ctui_canvas_circle(&c, 0, 0, 6, CTUI_ICON_ORANGE,
			   true); /* partly outside: clipped */
	CHECK(ctui_canvas_get(&c, 0, 0) && ctui_canvas_get(&c, 6, 0) &&
	      !ctui_canvas_get(&c, 7, 0));
	ctui_canvas_free(&c);
}

static void test_polyline(void)
{
	ctui_canvas c;
	CHECK(ctui_canvas_init(&c, 5, 3));
	const ctui_dot path[] = {{0, 0}, {5, 0}, {5, 5}};
	ctui_canvas_polyline(&c, path, 3, CTUI_ICON_GREEN);
	CHECK(count_dots(&c) == 11); /* 6 + 5, the corner counted once */
	CHECK(ctui_canvas_get(&c, 5, 0) && ctui_canvas_get(&c, 5, 5) &&
	      ctui_canvas_get(&c, 2, 0));
	ctui_canvas_clear(&c);
	ctui_canvas_polyline(&c, path, 1, CTUI_ICON_GREEN);
	CHECK(count_dots(&c) == 1 && ctui_canvas_get(&c, 0, 0));
	ctui_canvas_clear(&c);
	ctui_canvas_polyline(&c, path, 0, CTUI_ICON_GREEN);
	CHECK(count_dots(&c) == 0);
	ctui_canvas_free(&c);
}

static void test_axis(void)
{
	ctui_axis a = ctui_axis_nice(0, 97, 6);
	CHECK_NEAR(a.min, 0, 1e-9);
	CHECK_NEAR(a.max, 100, 1e-9);
	CHECK_NEAR(a.step, 20, 1e-9);
	CHECK(a.ticks == 6);

	a = ctui_axis_nice(3.2, 8.7, 5);
	CHECK(a.min <= 3.2 && a.max >= 8.7 && a.ticks <= 5 && a.ticks >= 2);

	a = ctui_axis_nice(-0.4, 1.1, 6);
	CHECK(a.min <= -0.4 && a.max >= 1.1 && a.ticks <= 6);
	char s[16];
	ctui_axis_label(&a, 0.0, s, sizeof s);
	CHECK_STR(s, "0.0");

	a = ctui_axis_nice(5, 5, 4); /* an empty range is widened */
	CHECK(a.min < 5 && a.max > 5);
	a = ctui_axis_nice(0, 0, 4);
	CHECK(a.min < 0 && a.max > 0);

	a = ctui_axis_nice(10, -10, 3); /* reversed bounds */
	CHECK(a.min <= -10 && a.max >= 10 && a.ticks <= 3);

	a = ctui_axis_nice(0, 1000, 5);
	ctui_axis_label(&a, a.max, s, sizeof s);
	CHECK_STR(s, "1000");
	a = ctui_axis_nice(0, 0.05, 6);
	ctui_axis_label(&a, 0.05, s, sizeof s);
	CHECK_STR(s, "0.05");
}

static void test_draw(void)
{
	ctui_canvas c;
	CHECK(ctui_canvas_init(&c, 4, 2));
	ctui_canvas_set(&c, 0, 0, CTUI_ICON_GREEN);
	screen_clear();
	ctui_canvas_draw(&c, 3, 5);
	CHECK(screen_has_text("\xe2\xa0\x81"));
	CHECK(screen_has_color(ctui_attr_icon(CTUI_ICON_GREEN, false)));
	screen_clear();
	ctui_canvas_clear(&c);
	ctui_canvas_draw(&c, 3, 5);
	CHECK(!screen_has_text("\xe2\xa0\x81")); /* empty cells draw nothing */
	ctui_canvas_free(&c);
}

static int dot_count(const ctui_canvas *c)
{
	int n = 0;
	for (int y = 0; y < ctui_canvas_height(c); y++)
		for (int x = 0; x < ctui_canvas_width(c); x++)
			n += ctui_canvas_get(c, x, y);
	return n;
}

static void test_clipped_line(void)
{
	ctui_canvas c;
	CHECK(ctui_canvas_init(&c, 5, 3)); /* 10 x 12 dots */
	/* inside: same as the plain line, rounded ends */
	ctui_canvas_clipped_line(&c, 0.4, 0.4, 9.4, 0.4, CTUI_ICON_BLUE);
	CHECK(dot_count(&c) == 10 && ctui_canvas_get(&c, 0, 0) &&
	      ctui_canvas_get(&c, 9, 0));
	ctui_canvas_clear(&c);
	/* far outside on both sides: only the visible row is drawn */
	ctui_canvas_clipped_line(&c, -1e9, 5, 1e9, 5, CTUI_ICON_BLUE);
	CHECK(dot_count(&c) == 10);
	for (int x = 0; x < 10; x++)
		CHECK(ctui_canvas_get(&c, x, 5));
	ctui_canvas_clear(&c);
	/* entirely on one side, or crossing a corner region without touching */
	ctui_canvas_clipped_line(&c, -5, -5, -1, 100, CTUI_ICON_BLUE);
	ctui_canvas_clipped_line(&c, 20, 0, 30, 11, CTUI_ICON_BLUE);
	ctui_canvas_clipped_line(&c, -10, 2, 2, -10, CTUI_ICON_BLUE);
	CHECK(dot_count(&c) == 0);
	/* a diagonal through the canvas keeps its visible part only */
	ctui_canvas_clipped_line(&c, -100, -100, 100, 100, CTUI_ICON_BLUE);
	CHECK(ctui_canvas_get(&c, 0, 0) && ctui_canvas_get(&c, 9, 9) &&
	      !ctui_canvas_get(&c, 9, 10));
	ctui_canvas_clear(&c);
	/* a vertical line, partly above */
	ctui_canvas_clipped_line(&c, 3, -50, 3, 6, CTUI_ICON_BLUE);
	CHECK(dot_count(&c) == 7 && ctui_canvas_get(&c, 3, 0) &&
	      ctui_canvas_get(&c, 3, 6));
	ctui_canvas_clear(&c);
	/* not a number: nothing */
	ctui_canvas_clipped_line(&c, NAN, 0, 5, 5, CTUI_ICON_BLUE);
	ctui_canvas_clipped_line(&c, 0, 0, INFINITY, 5, CTUI_ICON_BLUE);
	CHECK(dot_count(&c) == 0);
	ctui_canvas_free(&c);
}

int main(void)
{
	screen_open(24, 80);
	RUN(test_dots);
	RUN(test_glyph);
	RUN(test_line);
	RUN(test_rect);
	RUN(test_circle);
	RUN(test_polyline);
	RUN(test_clipped_line);
	RUN(test_axis);
	RUN(test_draw);
	endwin();
	return CHECK_DONE();
}
