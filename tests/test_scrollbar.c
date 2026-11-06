/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * test_scrollbar.c - thumb geometry, press, drag, release and wheel, vertical
 * and horizontal.
 */
#include "check.h"
#include "screen.h"
#include <ctui/scrollbar.h>

static void test_thumb(void)
{
	int start, length;
	/* fits: no thumb */
	CHECK(!ctui_scrollbar_thumb(10, 10, 0, 10, &start, &length));
	CHECK(!ctui_scrollbar_thumb(5, 10, 0, 10, &start, &length));
	CHECK(!ctui_scrollbar_thumb(20, 10, 0, 0, &start, &length));
	CHECK(!ctui_scrollbar_thumb(20, 0, 0, 10, &start, &length));

	/* half the content: half the track, top then bottom */
	CHECK(ctui_scrollbar_thumb(20, 10, 0, 10, &start, &length));
	CHECK(start == 0 && length == 5);
	CHECK(ctui_scrollbar_thumb(20, 10, 10, 10, &start, &length));
	CHECK(start == 5 && length == 5);
	CHECK(ctui_scrollbar_thumb(20, 10, 99, 10, &start, &length));
	CHECK(start == 5);

	/* huge content: one row, still reaching the last row */
	CHECK(ctui_scrollbar_thumb(10000, 10, 0, 10, &start, &length));
	CHECK(start == 0 && length == 1);
	CHECK(ctui_scrollbar_thumb(10000, 10, 9990, 10, &start, &length));
	CHECK(start == 9 && length == 1);

	/* a one-row track */
	CHECK(ctui_scrollbar_thumb(20, 10, 5, 1, &start, &length));
	CHECK(start == 0 && length == 1);
}

static ctui_scrollbar make(size_t total, size_t visible, size_t offset)
{
	ctui_scrollbar bar = {0};
	ctui_scrollbar_set(&bar, total, visible, offset);
	ctui_scrollbar_place(&bar, 2, 40, 10);
	return bar;
}

static void test_set(void)
{
	ctui_scrollbar bar = make(30, 10, 50);
	CHECK(bar.offset == 20);
	ctui_scrollbar_set(&bar, 5, 10, 3);
	CHECK(bar.offset == 0);
	CHECK(!ctui_scrollbar_needed(&bar));
}

static void test_over(void)
{
	ctui_scrollbar bar = make(30, 10, 0);
	CHECK(ctui_scrollbar_over(&bar, 2, 40));
	CHECK(ctui_scrollbar_over(&bar, 11, 40));
	CHECK(!ctui_scrollbar_over(&bar, 12, 40));
	CHECK(!ctui_scrollbar_over(&bar, 1, 40));
	CHECK(!ctui_scrollbar_over(&bar, 5, 39));
	ctui_scrollbar_set(&bar, 10, 10, 0);
	CHECK(!ctui_scrollbar_over(&bar, 5, 40));
	CHECK(!ctui_scrollbar_press(&bar, 5, 40));
}

static void test_track_press(void)
{
	ctui_scrollbar bar = make(100, 10, 0);
	/* thumb is row 0 (length 1): a press below pages down */
	CHECK(ctui_scrollbar_press(&bar, 8, 40));
	CHECK(!bar.dragging);
	CHECK(bar.offset == 10);
	CHECK(ctui_scrollbar_press(&bar, 11, 40));
	CHECK(bar.offset == 20);
	/* a press above the thumb pages up, down to the top */
	bar = make(100, 10, 85);
	CHECK(ctui_scrollbar_press(&bar, 2, 40));
	CHECK(bar.offset == 75);
	bar = make(100, 10, 5);
	int start, length;
	ctui_scrollbar_thumb(100, 10, 5, 10, &start, &length);
	CHECK(ctui_scrollbar_press(&bar, 2 + start + length, 40));
	CHECK(bar.offset == 15);
	/* clamped at the end */
	bar = make(100, 10, 85);
	CHECK(ctui_scrollbar_press(&bar, 11, 40)); /* the thumb itself */
	CHECK(bar.dragging && bar.offset == 85);
	bar = make(100, 10, 80);
	CHECK(ctui_scrollbar_press(&bar, 11, 40));
	CHECK(!bar.dragging && bar.offset == 90);
	CHECK(!ctui_scrollbar_press(&bar, 5, 41));
}

static void test_drag(void)
{
	ctui_scrollbar bar = make(20, 10, 0);	  /* thumb: 5 rows */
	CHECK(!ctui_scrollbar_drag(&bar, 5));	  /* not grabbed */
	CHECK(ctui_scrollbar_press(&bar, 4, 40)); /* row 2 of the thumb */
	CHECK(bar.dragging && bar.grab == 2);
	CHECK(bar.offset == 0);
	/* thumb top follows the pointer minus the grab: pointer row 7 -> top 5
	 */
	CHECK(ctui_scrollbar_drag(&bar, 2 + 7));
	CHECK(bar.offset == 10);
	CHECK(!ctui_scrollbar_drag(&bar, 2 + 7)); /* no change */
	/* beyond the track, past either end */
	CHECK(!ctui_scrollbar_drag(&bar, 50));
	CHECK(bar.offset == 10);
	CHECK(ctui_scrollbar_drag(&bar, -9));
	CHECK(bar.offset == 0);
	/* half way */
	CHECK(ctui_scrollbar_drag(&bar, 2 + 4));
	CHECK(bar.offset == 4);
	CHECK(ctui_scrollbar_release(&bar));
	CHECK(!bar.dragging);
	CHECK(!ctui_scrollbar_release(&bar));
	CHECK(!ctui_scrollbar_drag(&bar, 2 + 7));
}

static void test_drag_survives_set(void)
{
	ctui_scrollbar bar = make(200, 10, 0);
	CHECK(ctui_scrollbar_press(&bar, 2, 40));
	CHECK(bar.dragging);
	CHECK(ctui_scrollbar_drag(&bar, 2 + 9));
	size_t off = bar.offset;
	CHECK(off == 190);
	ctui_scrollbar_set(&bar, 200, 10, off);
	CHECK(bar.dragging);
	CHECK(bar.offset == off);
}

static void test_wheel(void)
{
	ctui_scrollbar bar = make(30, 10, 0);
	CHECK(!ctui_scrollbar_wheel(&bar, -3));
	CHECK(ctui_scrollbar_wheel(&bar, 3));
	CHECK(bar.offset == 3);
	CHECK(ctui_scrollbar_wheel(&bar, 100));
	CHECK(bar.offset == 20);
	CHECK(!ctui_scrollbar_wheel(&bar, 3));
	CHECK(ctui_scrollbar_wheel(&bar, -5));
	CHECK(bar.offset == 15);
	CHECK(ctui_scrollbar_wheel(&bar, -100));
	CHECK(bar.offset == 0);
}

static void test_glyphs(void)
{
	ctui_scrollbar bar = {0};
	CHECK_STR(ctui_scrollbar_glyph(&bar, true), "\xe2\x96\x88");
	CHECK_STR(ctui_scrollbar_glyph(&bar, false), "\xe2\x94\x82");
	bar.thumb_glyph = "#";
	CHECK_STR(ctui_scrollbar_glyph(&bar, true), "#");
	CHECK_STR(ctui_scrollbar_glyph(&bar, false),
		  "\xe2\x94\x82"); /* the track keeps the theme's */
	bar.track_glyph = ".";
	CHECK_STR(ctui_scrollbar_glyph(&bar, false), ".");
}

static ctui_scrollbar make_h(size_t total, size_t visible, size_t offset)
{
	ctui_scrollbar bar = {0};
	ctui_scrollbar_set(&bar, total, visible, offset);
	ctui_scrollbar_place_horizontal(&bar, 20, 5,
					10); /* row 20, columns 5 .. 14 */
	return bar;
}

static void test_horizontal_over_and_press(void)
{
	ctui_scrollbar bar = make_h(100, 10, 0);
	CHECK(bar.horizontal && bar.y == 20 && bar.x == 5 && bar.height == 10);
	CHECK(ctui_scrollbar_over(&bar, 20, 5) &&
	      ctui_scrollbar_over(&bar, 20, 14));
	CHECK(!ctui_scrollbar_over(&bar, 20, 15) &&
	      !ctui_scrollbar_over(&bar, 20, 4));
	CHECK(!ctui_scrollbar_over(&bar, 21, 8) &&
	      !ctui_scrollbar_over(&bar, 19, 8));
	CHECK(!ctui_scrollbar_over(
	    &bar, 8, 5)); /* the cells of a vertical track are not on it */
	/* the thumb is column 0 (length 1): a press to its right pages right */
	CHECK(ctui_scrollbar_press(&bar, 20, 12));
	CHECK(!bar.dragging && bar.offset == 10);
	bar = make_h(100, 10, 85);
	CHECK(ctui_scrollbar_press(&bar, 20,
				   5)); /* left of the thumb: pages left */
	CHECK(!bar.dragging && bar.offset == 75);
	CHECK(!ctui_scrollbar_press(&bar, 21, 8));
	ctui_scrollbar_set(&bar, 10, 10, 0);
	CHECK(!ctui_scrollbar_press(&bar, 20, 8)); /* it all fits */
}

static void test_horizontal_drag_and_wheel(void)
{
	ctui_scrollbar bar = make_h(20, 10, 0);	  /* thumb: 5 columns */
	CHECK(!ctui_scrollbar_drag(&bar, 12));	  /* not grabbed */
	CHECK(ctui_scrollbar_press(&bar, 20, 7)); /* column 2 of the thumb */
	CHECK(bar.dragging && bar.grab == 2 && bar.offset == 0);
	/* the left of the thumb follows the pointer minus the grab: column 12
	 * -> left 5 */
	CHECK(ctui_scrollbar_drag(&bar, 5 + 7));
	CHECK(bar.offset == 10);
	CHECK(!ctui_scrollbar_drag(&bar, 5 + 7));
	CHECK(!ctui_scrollbar_drag(&bar, 200));
	CHECK(ctui_scrollbar_drag(&bar, -9));
	CHECK(bar.offset == 0);
	CHECK(ctui_scrollbar_drag(&bar, 5 + 4));
	CHECK(bar.offset == 4);
	CHECK(ctui_scrollbar_release(&bar) && !bar.dragging);

	CHECK(ctui_scrollbar_wheel(&bar, 3) && bar.offset == 7);
	CHECK(ctui_scrollbar_wheel(&bar, 100) && bar.offset == 10);
	CHECK(ctui_scrollbar_wheel(&bar, -100) && bar.offset == 0);
	CHECK(!ctui_scrollbar_wheel(&bar, -1));

	/* the same arithmetic as the vertical bar */
	ctui_scrollbar v = make(100, 10, 0);
	bar = make_h(100, 10, 0);
	for (int step = 0; step < 12; step++) {
		CHECK(ctui_scrollbar_press(&v, 2 + 9, 40) ==
		      ctui_scrollbar_press(&bar, 20, 5 + 9));
		CHECK(v.offset == bar.offset);
	}
}

static wchar_t cell_at(int y, int x)
{
	cchar_t cell;
	wchar_t wch[CCHARW_MAX] = {0};
	attr_t attrs;
	short pair;
	if (mvin_wch(y, x, &cell) != OK ||
	    getcchar(&cell, wch, &attrs, &pair, NULL) != OK)
		return 0;
	return wch[0];
}

static void test_horizontal_glyphs_and_draw(void)
{
	ctui_scrollbar bar = {0};
	CHECK_STR(ctui_scrollbar_glyph(&bar, false),
		  "\xe2\x94\x82"); /* vertical track */
	bar.horizontal = true;
	CHECK_STR(ctui_scrollbar_glyph(&bar, false),
		  "\xe2\x94\x80"); /* horizontal line */
	/* half as thick as the full block of a vertical thumb */
	CHECK_STR(ctui_scrollbar_glyph(&bar, true), "\xe2\x96\x84");
	bar.thumb_glyph = "#";
	CHECK_STR(ctui_scrollbar_glyph(&bar, true), "#");
	bar.thumb_glyph = NULL;
	bar.track_glyph = ".";
	CHECK_STR(ctui_scrollbar_glyph(&bar, false), ".");

	screen_open(24, 80);
	bar = make_h(20, 10, 0); /* thumb: columns 5 .. 9 */
	screen_clear();
	CHECK(ctui_scrollbar_draw(&bar, false));
	CHECK(cell_at(20, 5) == L'▄' && cell_at(20, 9) == L'▄');
	CHECK(cell_at(20, 10) == L'─' && cell_at(20, 14) == L'─');
	CHECK(cell_at(21, 5) == L' ' &&
	      cell_at(19, 5) == L' '); /* a single row */
	bar.offset = 10;
	screen_clear();
	CHECK(ctui_scrollbar_draw(&bar, false));
	CHECK(cell_at(20, 5) == L'─' && cell_at(20, 14) == L'▄');
	ctui_scrollbar_set(&bar, 5, 10, 0);
	screen_clear();
	CHECK(!ctui_scrollbar_draw(&bar, false)); /* it all fits: nothing */
	CHECK(cell_at(20, 5) == L' ');
	endwin();
}

int main(void)
{
	RUN(test_thumb);
	RUN(test_set);
	RUN(test_over);
	RUN(test_track_press);
	RUN(test_drag);
	RUN(test_drag_survives_set);
	RUN(test_wheel);
	RUN(test_glyphs);
	RUN(test_horizontal_over_and_press);
	RUN(test_horizontal_drag_and_wheel);
	RUN(test_horizontal_glyphs_and_draw);
	return CHECK_DONE();
}
