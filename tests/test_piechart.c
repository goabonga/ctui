/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * test_piechart.c - shares, slices, layout, hit testing and drawing.
 */
#include <math.h>

#include "check.h"
#include "screen.h"
#include <ctui/piechart.h>

static const ctui_slice SLICES[] = {
    {"Linux", 50, CTUI_ICON_GREEN},
    {"Windows", 25, CTUI_ICON_BLUE},
    {"macOS", 15, CTUI_ICON_PURPLE},
    {"Other", 10, CTUI_ICON_ORANGE},
};

static void test_shares(void)
{
	ctui_piechart c;
	ctui_piechart_init(&c, SLICES, 4);
	CHECK_NEAR(ctui_piechart_total(&c), 100, 1e-9);
	CHECK_NEAR(ctui_piechart_share(&c, 0), 0.5, 1e-9);
	CHECK_NEAR(ctui_piechart_share(&c, 3), 0.1, 1e-9);
	CHECK_NEAR(ctui_piechart_share(&c, 9), 0, 1e-9);

	static const ctui_slice odd[] = {{"a", NAN, CTUI_ICON_BLUE},
					 {"b", -4, CTUI_ICON_BLUE},
					 {"c", 2, CTUI_ICON_BLUE}};
	ctui_piechart_init(&c, odd, 3);
	CHECK_NEAR(ctui_piechart_total(&c), 2,
		   1e-9); /* NaN and negatives count as 0 */
	CHECK_NEAR(ctui_piechart_share(&c, 2), 1, 1e-9);

	ctui_piechart_init(&c, NULL, 0);
	CHECK_NEAR(ctui_piechart_total(&c), 0, 1e-9);
	CHECK_NEAR(ctui_piechart_share(&c, 0), 0, 1e-9);
	CHECK(ctui_piechart_slice_at(&c, 0.3) == -1);
}

static void test_slice_at(void)
{
	ctui_piechart c;
	ctui_piechart_init(&c, SLICES, 4);
	CHECK(ctui_piechart_slice_at(&c, 0.0) == 0);
	CHECK(ctui_piechart_slice_at(&c, 0.49) == 0);
	CHECK(ctui_piechart_slice_at(&c, 0.5) == 1);
	CHECK(ctui_piechart_slice_at(&c, 0.74) == 1);
	CHECK(ctui_piechart_slice_at(&c, 0.76) == 2);
	CHECK(ctui_piechart_slice_at(&c, 0.95) == 3);
	CHECK(ctui_piechart_slice_at(&c, 1.25) == 0); /* turns wrap */
	CHECK(ctui_piechart_slice_at(&c, -0.1) == 3);
	CHECK(ctui_piechart_slice_at(&c, 0.9999999999) == 3);

	static const ctui_slice gap[] = {{"a", 0, CTUI_ICON_BLUE},
					 {"b", 1, CTUI_ICON_BLUE},
					 {"c", 0, CTUI_ICON_BLUE}};
	ctui_piechart_init(&c, gap, 3);
	CHECK(ctui_piechart_slice_at(&c, 0.0) ==
	      1); /* empty slices are skipped */
	CHECK(ctui_piechart_slice_at(&c, 0.99) == 1);
}

static void test_layout(void)
{
	ctui_piechart c;
	ctui_piechart_init(&c, SLICES, 4);
	ctui_piechart_layout l;
	CHECK(!ctui_piechart_layout_for(&c, 4, 10, &l));
	CHECK(!ctui_piechart_layout_for(&c, 40, 2, &l));

	CHECK(ctui_piechart_layout_for(&c, 60, 10, &l)); /* room for a legend */
	CHECK(l.legend.width > 0 && l.legend.x > l.pie.x + l.pie.width - 1);
	CHECK(l.pie.height == 10 && l.pie.width == 20);
	CHECK_NEAR(l.radius, 20, 1e-9);
	CHECK(l.legend.height == 4 && l.legend.y == 3);
	CHECK(l.legend.x + l.legend.width <= 60);

	CHECK(
	    ctui_piechart_layout_for(&c, 24, 10, &l)); /* too narrow for one */
	CHECK(l.legend.width == 0);
	CHECK(ctui_piechart_layout_for(
	    &c, 14, 10, &l)); /* limited by the width: round, not oval */
	CHECK(l.pie.width == 14 && l.pie.height == 7);

	c.legend = false;
	CHECK(ctui_piechart_layout_for(&c, 60, 10, &l));
	CHECK(l.legend.width == 0);
}

static void test_hit(void)
{
	ctui_piechart c;
	ctui_piechart_init(&c, SLICES, 4);
	ctui_piechart_layout l;
	CHECK(ctui_piechart_layout_for(&c, 60, 10, &l));
	int midy = l.pie.y + l.pie.height / 2, midx = l.pie.x + l.pie.width / 2;
	CHECK(ctui_piechart_hit(&c, 60, 10, midy - 3, midx + 2) ==
	      0); /* the right half: Linux */
	CHECK(ctui_piechart_hit(&c, 60, 10, midy + 3, midx + 2) == 0);
	CHECK(ctui_piechart_hit(&c, 60, 10, midy + 3, midx - 2) ==
	      1); /* bottom left: Windows */
	CHECK(ctui_piechart_hit(&c, 60, 10, midy - 1, midx - 6) ==
	      2); /* left: macOS */
	CHECK(ctui_piechart_hit(&c, 60, 10, midy - 4, midx - 3) ==
	      3); /* top left: Other */
	CHECK(ctui_piechart_hit(&c, 60, 10, 0, 0) ==
	      -1); /* the corner is off the disc */
	CHECK(ctui_piechart_hit(&c, 60, 10, 9, 59) == -1);
	CHECK(ctui_piechart_hit(&c, 60, 10, l.legend.y + 2, l.legend.x + 1) ==
	      2); /* the legend */
	CHECK(ctui_piechart_hit(&c, 60, 10, l.legend.y + 4, l.legend.x + 1) ==
	      -1);

	c.hole = 0.5; /* a donut: the middle is empty */
	CHECK(ctui_piechart_hit(&c, 60, 10, midy, midx) == -1);
	CHECK(ctui_piechart_hit(&c, 60, 10, midy - 3, midx + 2) ==
	      0); /* the ring is still there */
}

static void test_draw(void)
{
	ctui_piechart c;
	ctui_piechart_init(&c, SLICES, 4);
	screen_clear();
	ctui_piechart_draw(&c, 1, 2, 60, 10);
	CHECK(screen_has_text("Linux") && screen_has_text("Other"));
	CHECK(screen_has_text("50.0%") && screen_has_text("10.0%"));
	CHECK(screen_has_color(ctui_attr_icon(CTUI_ICON_GREEN, false)));
	CHECK(screen_has_color(ctui_attr_icon(CTUI_ICON_BLUE, false)));
	CHECK(screen_has_color(ctui_attr_icon(CTUI_ICON_PURPLE, false)));
	CHECK(screen_has_color(ctui_attr_icon(CTUI_ICON_ORANGE, false)));
	CHECK(screen_has_text(
	    "\xe2\xa3\xbf")); /* a full Braille cell inside the disc */

	ctui_style style = {0};
	style.glyphs.knob = "@";
	c.style = &style;
	screen_clear();
	ctui_piechart_draw(&c, 1, 2, 60, 10);
	CHECK(screen_has_text("@ Linux"));

	screen_clear();
	ctui_piechart_draw(&c, 1, 2, 4, 2); /* too small */
	CHECK(!screen_has_text("Linux"));

	ctui_piechart empty;
	ctui_piechart_init(&empty, NULL, 0);
	screen_clear();
	ctui_piechart_draw(&empty, 1, 2, 60,
			   10); /* nothing to draw, no crash */
}

int main(void)
{
	screen_open(24, 80);
	RUN(test_shares);
	RUN(test_slice_at);
	RUN(test_layout);
	RUN(test_hit);
	RUN(test_draw);
	endwin();
	return CHECK_DONE();
}
