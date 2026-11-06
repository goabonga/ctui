/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * test_sparkline.c - block levels and the drawn row.
 */
#include <math.h>
#include <string.h>

#include "check.h"
#include "screen.h"
#include <ctui/sparkline.h>

static void test_level(void)
{
	CHECK(ctui_sparkline_level(0, 0, 10, 8) == 0);
	CHECK(ctui_sparkline_level(10, 0, 10, 8) == 7);
	CHECK(ctui_sparkline_level(5, 0, 10, 8) == 4);
	CHECK(ctui_sparkline_level(-3, 0, 10, 8) == 0); /* clamped */
	CHECK(ctui_sparkline_level(99, 0, 10, 8) == 7);
	CHECK(ctui_sparkline_level(5, 3, 3, 8) == 0); /* empty range */
	CHECK(ctui_sparkline_level(5, 0, 10, 0) == 0);
}

static void test_draw(void)
{
	static const double v[] = {1, 2, 3, 4, 5, 6, 7, 8};
	screen_clear();
	CHECK(ctui_sparkline_draw(2, 4, 20, v, 8, 0, 0, CTUI_ICON_CYAN) == 8);
	CHECK(screen_has_text("\xe2\x96\x81\xe2\x96\x82\xe2\x96\x83\xe2\x96\x84"
			      "\xe2\x96\x85\xe2\x96\x86"
			      "\xe2\x96\x87\xe2\x96\x88"));
	CHECK(screen_has_color(ctui_attr_icon(CTUI_ICON_CYAN, false)));

	screen_clear(); /* too narrow: the latest values are shown */
	CHECK(ctui_sparkline_draw(2, 4, 3, v, 8, 0, 0, CTUI_ICON_CYAN) == 3);
	CHECK(screen_has_text(
	    "\xe2\x96\x81\xe2\x96\x85\xe2\x96\x88")); /* the range of the shown
							 ones */
	CHECK(!screen_has_text("\xe2\x96\x82"));

	screen_clear(); /* a given range */
	ctui_sparkline_draw(2, 4, 20, v, 2, 0, 16, CTUI_ICON_CYAN);
	CHECK(screen_has_text("\xe2\x96\x81\xe2\x96\x82"));

	static const double gap[] = {1, NAN, 3};
	screen_clear();
	ctui_sparkline_draw(2, 4, 20, gap, 3, 0, 0, CTUI_ICON_CYAN);
	CHECK(screen_has_text("\xe2\x96\x81")); /* the first value */
	CHECK(screen_has_text(
	    "\xe2\x96\x81 \xe2\x96\x88")); /* the NaN leaves a gap */

	static const double flat[] = {5, 5};
	screen_clear();
	ctui_sparkline_draw(2, 4, 20, flat, 2, 0, 0, CTUI_ICON_CYAN);
	CHECK(screen_has_text("\xe2\x96\x85\xe2\x96\x85"));

	CHECK(ctui_sparkline_draw(2, 4, 0, v, 8, 0, 0, CTUI_ICON_CYAN) == 0);
	CHECK(ctui_sparkline_draw(2, 4, 5, v, 0, 0, 0, CTUI_ICON_CYAN) == 0);
}

/* Row y from column x, as many bytes as n UTF-8 blocks (3 bytes each). */
static bool row_is(int y, int x, const char *want)
{
	char row[256];
	if (mvinnstr(y, x, row, (int)strlen(want)) == ERR)
		return false;
	return strcmp(row, want) == 0;
}

#define B1 "\xe2\x96\x81" /* one eighth */
#define B5 "\xe2\x96\x85" /* five eighths */
#define B8 "\xe2\x96\x88" /* full */

static void test_draw_rows(void)
{
	/* 0, 75 and 100 on two rows: one eighth; a full bottom row under part
	 * of the top one; both rows full. */
	static const double v[] = {0, 75, 100};
	screen_clear();
	CHECK(ctui_sparkline_draw_rows(3, 4, 20, 2, v, 3, 0, 100,
				       CTUI_ICON_GREEN) == 3);
	/* 75% of 16 eighths is level 12, so 13 lit: 5 of them on the top row.
	 */
	CHECK(ctui_sparkline_level(75, 0, 100, 16) == 12);
	CHECK(row_is(3, 4, " " B5 B8));
	CHECK(row_is(4, 4, B1 B8 B8));
	CHECK(screen_has_color(ctui_attr_icon(CTUI_ICON_GREEN, false)));

	/* One row: the same blocks as the one-row chart. */
	static const double steps[] = {1, 2, 3, 4, 5, 6, 7, 8};
	screen_clear();
	ctui_sparkline_draw_rows(2, 4, 20, 1, steps, 8, 0, 0, CTUI_ICON_CYAN);
	char rows_drawn[64], row_drawn[64];
	mvinnstr(2, 4, rows_drawn, 24);
	screen_clear();
	ctui_sparkline_draw(2, 4, 20, steps, 8, 0, 0, CTUI_ICON_CYAN);
	mvinnstr(2, 4, row_drawn, 24);
	CHECK_STR(rows_drawn, row_drawn);

	/* The latest values, a NaN gap, a flat series at mid height, and
	 * nothing for an empty area or series. */
	static const double gap[] = {100, NAN, 100};
	screen_clear();
	CHECK(ctui_sparkline_draw_rows(5, 0, 2, 3, gap, 3, 0, 100,
				       CTUI_ICON_CYAN) == 2);
	CHECK(row_is(5, 0, " " B8) && row_is(7, 0, " " B8));
	static const double flat[] = {5, 5};
	screen_clear();
	ctui_sparkline_draw_rows(5, 0, 10, 2, flat, 2, 0, 0, CTUI_ICON_CYAN);
	CHECK(row_is(6, 0, B8 B8) && row_is(5, 0, "\xe2\x96\x81\xe2\x96\x81"));
	CHECK(ctui_sparkline_draw_rows(0, 0, 0, 2, v, 3, 0, 0,
				       CTUI_ICON_CYAN) == 0);
	CHECK(ctui_sparkline_draw_rows(0, 0, 5, 0, v, 3, 0, 0,
				       CTUI_ICON_CYAN) == 0);
	CHECK(ctui_sparkline_draw_rows(0, 0, 5, 2, v, 0, 0, 0,
				       CTUI_ICON_CYAN) == 0);
}

int main(void)
{
	screen_open(24, 80);
	RUN(test_level);
	RUN(test_draw);
	RUN(test_draw_rows);
	endwin();
	return CHECK_DONE();
}
