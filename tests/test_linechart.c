/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * test_linechart.c - layout, hit testing and drawing of the line chart.
 */
#include <math.h>
#include <string.h>

#include "check.h"
#include "screen.h"
#include <ctui/linechart.h>

static const double UP[] = {0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100};
static const double DOWN[] = {100, 80, 60, 40, 20, 0};
static const ctui_series SERIES[] = {
    {.name = "load", .values = UP, .count = 11, .color = CTUI_ICON_GREEN},
    {.name = "idle", .values = DOWN, .count = 6, .color = CTUI_ICON_ORANGE},
};

static void test_points_and_axis(void)
{
	ctui_linechart c;
	ctui_linechart_init(&c, SERIES, 2);
	CHECK(ctui_linechart_points(&c) == 11);
	ctui_axis a = ctui_linechart_axis(&c, 10);
	CHECK(a.min <= 0 && a.max >= 100);

	c.fixed = true;
	c.min = -50;
	c.max = 50;
	a = ctui_linechart_axis(&c, 10);
	CHECK(a.min <= -50 && a.max >= 50);

	ctui_linechart empty;
	ctui_linechart_init(&empty, NULL, 0);
	CHECK(ctui_linechart_points(&empty) == 0);
	a = ctui_linechart_axis(&empty, 6); /* no data: a default range */
	CHECK(a.min <= 0 && a.max >= 1);
}

static void test_layout(void)
{
	ctui_linechart c;
	ctui_linechart_init(&c, SERIES, 2);
	ctui_linechart_layout l;
	CHECK(!ctui_linechart_layout_for(&c, 5, 10, &l));
	CHECK(!ctui_linechart_layout_for(&c, 40, 2, &l));
	CHECK(ctui_linechart_layout_for(&c, 40, 10, &l));
	CHECK(l.legend_row == 0 && l.plot.y == 1 && l.plot.height == 9);
	CHECK(l.labels_row == -1);
	CHECK(l.label_width >= 1 && l.plot.x == l.label_width + 1);
	CHECK(l.plot.x + l.plot.width == 40);

	static const char *const labels[] = {"mon", "sun"};
	c.x_labels = labels;
	c.x_label_count = 2;
	c.legend = false;
	CHECK(ctui_linechart_layout_for(&c, 40, 10, &l));
	CHECK(l.legend_row == -1 && l.plot.y == 0);
	CHECK(l.labels_row == 9 && l.plot.height == 9);

	CHECK(ctui_linechart_layout_for(&c, 40, 4,
					&l)); /* too short for labels */
	CHECK(l.labels_row == -1 && l.plot.height == 4);
}

static void test_index_at(void)
{
	ctui_linechart c;
	ctui_linechart_init(&c, SERIES, 2);
	ctui_linechart_layout l;
	CHECK(ctui_linechart_layout_for(&c, 40, 10, &l));
	CHECK(ctui_linechart_index_at(&c, 40, 10, 0) == -1); /* on the labels */
	CHECK(ctui_linechart_index_at(&c, 40, 10, 40) == -1);
	CHECK(ctui_linechart_index_at(&c, 40, 10, l.plot.x) == 0);
	CHECK(ctui_linechart_index_at(&c, 40, 10, 39) == 10);
	int mid =
	    ctui_linechart_index_at(&c, 40, 10, l.plot.x + l.plot.width / 2);
	CHECK(mid >= 4 && mid <= 6);
	int prev = 0;
	for (int x = l.plot.x; x < 40; x++) { /* never goes backwards */
		int i = ctui_linechart_index_at(&c, 40, 10, x);
		CHECK(i >= prev);
		prev = i;
	}
}

static void test_draw(void)
{
	ctui_linechart c;
	ctui_linechart_init(&c, SERIES, 2);
	static const char *const labels[] = {"mon", "sun"};
	c.x_labels = labels;
	c.x_label_count = 2;

	screen_clear();
	ctui_linechart_draw(&c, 1, 2, 40, 12);
	CHECK(screen_has_text("load") &&
	      screen_has_text("idle")); /* the legend */
	CHECK(screen_has_text("100"));	/* the top tick */
	CHECK(screen_has_text("mon") && screen_has_text("sun"));
	CHECK(screen_has_color(ctui_attr_icon(CTUI_ICON_GREEN, false)));
	CHECK(screen_has_color(ctui_attr_icon(CTUI_ICON_ORANGE, false)));
	CHECK(screen_has_text("\xe2\xa1")); /* Braille dots were drawn */

	ctui_style style = {0};
	style.glyphs.knob = "@";
	c.style = &style;
	screen_clear();
	ctui_linechart_draw(&c, 1, 2, 40, 12);
	CHECK(screen_has_text("@ load"));

	screen_clear(); /* too small: nothing, no crash */
	ctui_linechart_draw(&c, 1, 2, 4, 2);
	CHECK(!screen_has_text("load"));
}

static void test_gaps_and_single_point(void)
{
	static const double gap[] = {1, NAN, 3, 4};
	static const double one[] = {7};
	const ctui_series s1[] = {{.name = "gap",
				   .values = gap,
				   .count = 4,
				   .color = CTUI_ICON_BLUE}};
	const ctui_series s2[] = {{.name = "one",
				   .values = one,
				   .count = 1,
				   .color = CTUI_ICON_BLUE}};
	ctui_linechart c;
	ctui_linechart_init(&c, s1, 1);
	screen_clear();
	ctui_linechart_draw(&c, 0, 0, 30, 8);
	CHECK(screen_has_color(ctui_attr_icon(CTUI_ICON_BLUE, false)));
	ctui_linechart_init(&c, s2, 1);
	screen_clear();
	ctui_linechart_draw(&c, 0, 0, 30, 8);
	CHECK(screen_has_color(ctui_attr_icon(CTUI_ICON_BLUE, false)));
	CHECK(ctui_linechart_index_at(&c, 30, 8, 20) == 0);
}

static int cells_of(ctui_icon_color color)
{
	short want = (short)PAIR_NUMBER((chtype)ctui_attr_icon(color, false));
	int n = 0;
	for (int y = 0; y < LINES; y++)
		for (int x = 0; x < COLS; x++) {
			cchar_t cell;
			wchar_t wch[CCHARW_MAX];
			attr_t attrs;
			short pair;
			if (mvin_wch(y, x, &cell) == OK &&
			    getcchar(&cell, wch, &attrs, &pair, NULL) == OK &&
			    pair == want && wch[0] != L' ')
				n++;
		}
	return n;
}

static void test_modes(void)
{
	static const double v[] = {0, 10};
	ctui_series s[] = {
	    {.name = "a", .values = v, .count = 2, .color = CTUI_ICON_PURPLE}};
	CHECK(s[0].mode == CTUI_SERIES_LINE); /* zero is a line */
	ctui_linechart c;
	ctui_linechart_init(&c, s, 1);
	c.legend = false;

	screen_clear();
	ctui_linechart_draw(&c, 0, 0, 30, 8);
	int line = cells_of(CTUI_ICON_PURPLE);
	CHECK(line >= 12); /* spans the whole plot */

	s[0].mode = CTUI_SERIES_POINTS;
	screen_clear();
	ctui_linechart_draw(&c, 0, 0, 30, 8);
	int points = cells_of(CTUI_ICON_PURPLE);
	CHECK(points >= 2 && points <= 8 &&
	      points < line); /* two markers, nothing between */

	s[0].mode = CTUI_SERIES_BOTH;
	screen_clear();
	ctui_linechart_draw(&c, 0, 0, 30, 8);
	int both = cells_of(CTUI_ICON_PURPLE);
	CHECK(both >= line && both > points);

	static const double one[] = {5};
	ctui_series single[] = {{.name = "s",
				 .values = one,
				 .count = 1,
				 .color = CTUI_ICON_PURPLE,
				 .mode = CTUI_SERIES_POINTS}};
	ctui_linechart_init(&c, single, 1);
	c.legend = false;
	screen_clear();
	ctui_linechart_draw(&c, 0, 0, 30, 8);
	CHECK(cells_of(CTUI_ICON_PURPLE) >= 1);

	static const double gap[] = {0, NAN, 10};
	ctui_series gapped[] = {{.name = "g",
				 .values = gap,
				 .count = 3,
				 .color = CTUI_ICON_PURPLE,
				 .mode = CTUI_SERIES_POINTS}};
	ctui_linechart_init(&c, gapped, 1);
	c.legend = false;
	screen_clear();
	ctui_linechart_draw(&c, 0, 0, 30, 8); /* a NaN has no marker */
	CHECK(cells_of(CTUI_ICON_PURPLE) <= 8);
}

static void test_legend_positions(void)
{
	ctui_linechart c;
	ctui_linechart_init(&c, SERIES, 2);
	ctui_linechart_layout l;
	CHECK(c.legend_pos == CTUI_LEGEND_TOP); /* the default */

	c.legend_pos = CTUI_LEGEND_BOTTOM;
	CHECK(ctui_linechart_layout_for(&c, 40, 10, &l));
	CHECK(l.legend_row == 9 && l.plot.y == 0 && l.plot.height == 9 &&
	      l.legend_col == -1);

	static const char *const labels[] = {"mon", "sun"};
	c.x_labels = labels;
	c.x_label_count = 2;
	CHECK(ctui_linechart_layout_for(
	    &c, 40, 10, &l)); /* labels sit between plot and legend */
	CHECK(l.labels_row == 8 && l.legend_row == 9 && l.plot.height == 8);

	c.legend_pos = CTUI_LEGEND_NONE;
	CHECK(ctui_linechart_layout_for(&c, 40, 10, &l));
	CHECK(l.legend_row == -1 && l.legend_col == -1 && l.plot.y == 0 &&
	      l.plot.height == 9);

	c.legend_pos = CTUI_LEGEND_RIGHT;
	CHECK(ctui_linechart_layout_for(&c, 40, 10, &l));
	CHECK(l.legend_row == -1 && l.legend_col >= 0 &&
	      l.legend_width >= 6); /* "load" + marker */
	CHECK(l.plot.y == 0 && l.plot.height == 9);
	CHECK(l.plot.x + l.plot.width <
	      l.legend_col); /* a gap before the legend */
	CHECK(l.legend_col + l.legend_width == 40);

	CHECK(ctui_linechart_layout_for(
	    &c, 14, 10, &l)); /* too narrow for a side legend: dropped */
	CHECK(l.legend_col == -1 && l.plot.x + l.plot.width == 14);

	c.legend = false; /* the flag still turns every position off */
	CHECK(ctui_linechart_layout_for(&c, 40, 10, &l));
	CHECK(l.legend_col == -1 && l.plot.x + l.plot.width == 40);
}

static void test_titles(void)
{
	ctui_linechart c;
	ctui_linechart_init(&c, SERIES, 2);
	c.x_title = "time";
	c.y_title = "load";
	ctui_linechart_layout l;
	CHECK(ctui_linechart_layout_for(&c, 40, 12, &l));
	CHECK(l.legend_row == 0 && l.y_title_row == 1 && l.plot.y == 2);
	CHECK(l.x_title_row == 11 && l.plot.height == 9 && l.labels_row == -1);

	static const char *const labels[] = {"mon", "sun"};
	c.x_labels = labels;
	c.x_label_count = 2;
	c.legend_pos = CTUI_LEGEND_BOTTOM;
	CHECK(ctui_linechart_layout_for(&c, 40, 12, &l));
	CHECK(l.y_title_row == 0 && l.plot.y == 1);
	CHECK(l.labels_row == 9 && l.x_title_row == 10 && l.legend_row == 11 &&
	      l.plot.height == 8);

	CHECK(ctui_linechart_layout_for(&c, 40, 5,
					&l)); /* titles give way to the plot */
	CHECK(l.plot.height >= 3 && l.y_title_row == -1);

	ctui_linechart plain;
	ctui_linechart_init(&plain, SERIES, 2);
	CHECK(ctui_linechart_layout_for(&plain, 40, 12, &l));
	CHECK(l.x_title_row == -1 &&
	      l.y_title_row == -1); /* none unless asked */
	plain.x_title = "";
	CHECK(ctui_linechart_layout_for(&plain, 40, 12, &l));
	CHECK(l.x_title_row == -1);
}

static void test_index_with_layouts(void)
{
	ctui_linechart c;
	ctui_linechart_init(&c, SERIES, 2);
	c.legend_pos = CTUI_LEGEND_RIGHT;
	c.x_title = "t";
	ctui_linechart_layout l;
	CHECK(ctui_linechart_layout_for(&c, 50, 12, &l));
	CHECK(ctui_linechart_index_at(&c, 50, 12, l.plot.x) == 0);
	CHECK(ctui_linechart_index_at(&c, 50, 12,
				      l.plot.x + l.plot.width - 1) == 10);
	CHECK(ctui_linechart_index_at(&c, 50, 12, l.plot.x + l.plot.width) ==
	      -1); /* in the gap */
	CHECK(ctui_linechart_index_at(&c, 50, 12, l.legend_col) == -1);
	CHECK(ctui_linechart_index_at(&c, 50, 12, 49) == -1);
	int prev = 0;
	for (int x = l.plot.x; x < l.plot.x + l.plot.width; x++) {
		int i = ctui_linechart_index_at(&c, 50, 12, x);
		CHECK(i >= prev && i <= 10);
		prev = i;
	}
}

static void test_draw_layouts(void)
{
	ctui_linechart c;
	ctui_linechart_init(&c, SERIES, 2);
	c.x_title = "elapsed";
	c.y_title = "percent";
	c.legend_pos = CTUI_LEGEND_RIGHT;
	screen_clear();
	ctui_linechart_draw(&c, 0, 0, 50, 12);
	CHECK(screen_has_text("elapsed") && screen_has_text("percent"));
	CHECK(screen_has_text("load") && screen_has_text("idle"));
	CHECK(screen_has_color(ctui_attr_icon(CTUI_ICON_GREEN, false)));

	ctui_linechart_layout l;
	CHECK(ctui_linechart_layout_for(&c, 50, 12, &l));
	char row[128];
	mvinnstr(l.plot.y, l.legend_col, row,
		 20); /* first series on the first plot row */
	CHECK(strstr(row, "load") != NULL);
	mvinnstr(l.plot.y + 1, l.legend_col, row, 20);
	CHECK(strstr(row, "idle") != NULL);

	c.legend_pos = CTUI_LEGEND_BOTTOM;
	CHECK(ctui_linechart_layout_for(&c, 50, 12, &l));
	screen_clear();
	ctui_linechart_draw(&c, 0, 0, 50, 12);
	mvinnstr(l.legend_row, 0, row, 40);
	CHECK(strstr(row, "load") != NULL && strstr(row, "idle") != NULL);

	c.legend_pos = CTUI_LEGEND_NONE;
	screen_clear();
	ctui_linechart_draw(&c, 0, 0, 50, 12);
	CHECK(!screen_has_text("load") && !screen_has_text("idle"));
	CHECK(screen_has_text("elapsed"));
}

int main(void)
{
	screen_open(24, 80);
	RUN(test_points_and_axis);
	RUN(test_layout);
	RUN(test_index_at);
	RUN(test_draw);
	RUN(test_gaps_and_single_point);
	RUN(test_modes);
	RUN(test_legend_positions);
	RUN(test_titles);
	RUN(test_index_with_layouts);
	RUN(test_draw_layouts);
	endwin();
	return CHECK_DONE();
}
