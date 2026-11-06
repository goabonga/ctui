/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * test_barchart.c - values, layout, hit testing and drawing of the bar chart.
 */
#include <math.h>

#include "check.h"
#include "screen.h"
#include <ctui/barchart.h>

static const double SALES[] = {10, 40, 20, 80};
static const double COSTS[] = {5, 10, NAN, -3};
static const ctui_series SERIES[] = {
    {.name = "sales", .values = SALES, .count = 4, .color = CTUI_ICON_GREEN},
    {.name = "costs", .values = COSTS, .count = 4, .color = CTUI_ICON_ORANGE},
};
static const char *const CATEGORIES[] = {"Q1", "Q2", "Q3", "Q4"};

static void fixture(ctui_barchart *c)
{
	ctui_barchart_init(c, SERIES, 2);
	c->categories = CATEGORIES;
}

static void test_values(void)
{
	ctui_barchart c;
	fixture(&c);
	CHECK(ctui_barchart_categories(&c) == 4);
	c.category_count = 3;
	CHECK(ctui_barchart_categories(&c) == 3);
	CHECK_NEAR(ctui_barchart_value(&c, 0, 3), 80, 1e-9);
	CHECK_NEAR(ctui_barchart_value(&c, 1, 2), 0, 1e-9); /* NaN */
	CHECK_NEAR(ctui_barchart_value(&c, 1, 3), 0, 1e-9); /* negative */
	CHECK_NEAR(ctui_barchart_value(&c, 2, 0), 0, 1e-9); /* no such series */
	CHECK_NEAR(ctui_barchart_value(&c, 0, 9), 0,
		   1e-9); /* no such category */
}

static void test_layout(void)
{
	ctui_barchart c;
	fixture(&c);
	ctui_barchart_layout l;
	CHECK(!ctui_barchart_layout_for(&c, 5, 10, &l));
	CHECK(ctui_barchart_layout_for(&c, 40, 12, &l));
	CHECK(l.legend_row == 0 && l.labels_row == 11 && l.plot.y == 1 &&
	      l.plot.height == 10);
	CHECK(l.axis.min == 0 && l.axis.max >= 80);
	CHECK(l.plot.x == l.label_width + 1 && l.plot.x + l.plot.width == 40);

	c.stacked = true; /* piled up: the axis fits the totals */
	CHECK(ctui_barchart_layout_for(&c, 40, 12, &l));
	CHECK(l.axis.max >= 80);

	c.stacked = false;
	c.horizontal = true;
	CHECK(ctui_barchart_layout_for(&c, 40, 12, &l));
	CHECK(l.label_width == 2 && l.plot.x == 3); /* "Q1" */
	CHECK(l.labels_row == 11 && l.plot.width == 37);

	c.fixed = true;
	c.min = 0;
	c.max = 200;
	CHECK(ctui_barchart_layout_for(&c, 40, 12, &l));
	CHECK(l.axis.max >= 200);

	ctui_barchart none;
	ctui_barchart_init(&none, NULL, 0); /* no data: still a layout */
	CHECK(ctui_barchart_layout_for(&none, 40, 12, &l));
}

static void test_hit(void)
{
	ctui_barchart c;
	fixture(&c);
	ctui_barchart_layout l;
	CHECK(ctui_barchart_layout_for(&c, 44, 12, &l));
	size_t k, s;
	int bottom = l.plot.y + l.plot.height - 1;

	/* the tallest bar, the first series of Q4: a hit just above the
	 * baseline */
	bool found = false;
	for (int x = l.plot.x; x < 44 && !found; x++)
		if (ctui_barchart_hit(&c, 44, 12, bottom, x, &k, &s) &&
		    k == 3 && s == 0)
			found = true;
	CHECK(found);

	/* above a short bar and outside the plot nothing is hit */
	CHECK(!ctui_barchart_hit(&c, 44, 12, 0, l.plot.x, &k, &s));
	CHECK(!ctui_barchart_hit(&c, 44, 12, bottom, 0, &k, &s));
	CHECK(!ctui_barchart_hit(&c, 44, 12, bottom + 1, l.plot.x, &k, &s));

	/* the top of Q4 is reached, the top of Q1 is not */
	int top_row = l.plot.y;
	bool q4_top = false, q1_top = false;
	for (int x = l.plot.x; x < 44; x++)
		if (ctui_barchart_hit(&c, 44, 12, top_row, x, &k, &s)) {
			q4_top = q4_top || k == 3;
			q1_top = q1_top || k == 0;
		}
	CHECK(q4_top && !q1_top);
}

static void test_stacked_hit(void)
{
	static const double a[] = {30}, b[] = {30};
	const ctui_series two[] = {
	    {.name = "a", .values = a, .count = 1, .color = CTUI_ICON_BLUE},
	    {.name = "b", .values = b, .count = 1, .color = CTUI_ICON_CYAN}};
	ctui_barchart c;
	ctui_barchart_init(&c, two, 2);
	c.stacked = true;
	c.legend = false;
	ctui_barchart_layout l;
	CHECK(ctui_barchart_layout_for(&c, 20, 10, &l));
	size_t k, s;
	int x = l.plot.x + l.plot.width / 2;
	int bottom = l.plot.y + l.plot.height - 1;
	CHECK(ctui_barchart_hit(&c, 20, 10, bottom, x, &k, &s) && s == 0);
	CHECK(ctui_barchart_hit(&c, 20, 10, l.plot.y, x, &k, &s) && s == 1);
}

static void test_draw(void)
{
	ctui_barchart c;
	fixture(&c);
	screen_clear();
	ctui_barchart_draw(&c, 1, 2, 44, 14);
	CHECK(screen_has_text("sales") && screen_has_text("costs"));
	CHECK(screen_has_text("Q1") && screen_has_text("Q4"));
	CHECK(screen_has_text("\xe2\x96\x88")); /* a full block */
	CHECK(screen_has_color(ctui_attr_icon(CTUI_ICON_GREEN, false)));
	CHECK(screen_has_color(ctui_attr_icon(CTUI_ICON_ORANGE, false)));
	CHECK(screen_has_text("80") || screen_has_text("100"));

	c.horizontal = true;
	screen_clear();
	ctui_barchart_draw(&c, 1, 2, 44, 14);
	CHECK(screen_has_text("Q1") && screen_has_text("\xe2\x96\x88"));
	CHECK(screen_has_color(ctui_attr_icon(CTUI_ICON_GREEN, false)));

	c.horizontal = false;
	c.stacked = true;
	screen_clear();
	ctui_barchart_draw(&c, 1, 2, 44, 14);
	CHECK(screen_has_color(ctui_attr_icon(CTUI_ICON_GREEN, false)));
	CHECK(screen_has_color(ctui_attr_icon(CTUI_ICON_ORANGE, false)));

	ctui_style style = {0};
	style.glyphs.knob = "@";
	c.style = &style;
	screen_clear();
	ctui_barchart_draw(&c, 1, 2, 44, 14);
	CHECK(screen_has_text("@ sales"));

	screen_clear();
	ctui_barchart_draw(&c, 1, 2, 4, 2); /* too small */
	CHECK(!screen_has_text("sales"));
}

int main(void)
{
	screen_open(24, 80);
	RUN(test_values);
	RUN(test_layout);
	RUN(test_hit);
	RUN(test_stacked_hit);
	RUN(test_draw);
	endwin();
	return CHECK_DONE();
}
