/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */
#include "check.h"

#include <math.h>

#include <ctui/gauge.h>

static void test_filled(void)
{
	CHECK(ctui_gauge_filled(true, 50, 100, 10) == 5);
	CHECK(ctui_gauge_filled(true, 99.9, 100, 10) == 9);
	CHECK(ctui_gauge_filled(true, 150, 100, 10) == 10);
	CHECK(ctui_gauge_filled(true, -5, 100, 10) == 0);
	CHECK(ctui_gauge_filled(false, 50, 100, 10) == 0);
	CHECK(ctui_gauge_filled(true, 50, 0, 10) == 0);
	CHECK(ctui_gauge_filled(true, 50, NAN, 10) == 0);
	CHECK(ctui_gauge_filled(true, NAN, 100, 10) == 0);
}

static void test_zones(void)
{
	CHECK(ctui_gauge_zone(0, 10) == 0);
	CHECK(ctui_gauge_zone(2, 10) == 1);
	CHECK(ctui_gauge_zone(4, 10) == 2);
	CHECK(ctui_gauge_zone(6, 10) == 3);
	CHECK(ctui_gauge_zone(8, 10) == 4);
	CHECK(ctui_gauge_zone(9, 10) == 5);
	CHECK(ctui_gauge_zone(3, 0) == 0);
	CHECK(ctui_gauge_zone_pair(0) == CTUI_PAIR_GAUGE_GREEN);
	CHECK(ctui_gauge_zone_pair(5) == CTUI_PAIR_GAUGE_RED_BRIGHT);
	CHECK(ctui_gauge_zone_pair(99) == CTUI_PAIR_GAUGE_RED_BRIGHT);
}

static void test_bar(void)
{
	char buf[64];
	ctui_gauge_bar(buf, sizeof buf, true, 30, 100, 10, "#", "-");
	CHECK_STR(buf, "###-------");
	ctui_gauge_bar(buf, sizeof buf, false, 30, 100, 4, "#", "-");
	CHECK_STR(buf, "----");
	ctui_gauge_bar(buf, sizeof buf, true, 100, 100, 4, "\xe2\x96\x88",
		       "\xe2\x96\x91");
	CHECK_STR(buf, "\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x88");
	/* cut on a glyph boundary when the buffer is small */
	ctui_gauge_bar(buf, 8, true, 100, 100, 4, "\xe2\x96\x88",
		       "\xe2\x96\x91");
	CHECK_STR(buf, "\xe2\x96\x88\xe2\x96\x88");
	/* default glyphs come from the current set */
	ctui_gauge_bar(buf, sizeof buf, true, 100, 100, 2, NULL, NULL);
	CHECK_STR(buf, "\xe2\x96\x88\xe2\x96\x88");
}

static void test_level(void)
{
	CHECK(ctui_gauge_level_pair(0) == CTUI_PAIR_GAUGE_GREEN);
	CHECK(ctui_gauge_level_pair(69.9) == CTUI_PAIR_GAUGE_GREEN);
	CHECK(ctui_gauge_level_pair(70) == CTUI_PAIR_GAUGE_YELLOW);
	CHECK(ctui_gauge_level_pair(89.9) == CTUI_PAIR_GAUGE_YELLOW);
	CHECK(ctui_gauge_level_pair(90) == CTUI_PAIR_GAUGE_RED);
}

int main(void)
{
	RUN(test_filled);
	RUN(test_zones);
	RUN(test_bar);
	RUN(test_level);
	return CHECK_DONE();
}
