/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * test_heatmap.c - range, steps, layout, hit testing, keys and drawing of
 * the heatmap.
 */
#include <limits.h>
#include <math.h>

#include "check.h"
#include "screen.h"
#include <ctui/heatmap.h>

static const double V[] = {0, 1, 2, 3, 4, 5, NAN, 7, 10};
static const char *const DAYS[] = {"mon", "tue", "wed"};
static const char *const HEADS[] = {"am", "noon", "pm"};

static void fixture(ctui_heatmap *h)
{
	ctui_heatmap_init(h, V, 3, 3);
	h->row_labels = DAYS;
	h->col_labels = HEADS;
}

static void test_range_and_step(void)
{
	ctui_heatmap h;
	fixture(&h);
	double lo, hi;
	CHECK(ctui_heatmap_range(&h, &lo, &hi) && lo == 0 && hi == 10);
	h.fixed = true;
	h.min = -5;
	h.max = 5;
	CHECK(ctui_heatmap_range(&h, &lo, &hi) && lo == -5 && hi == 5);

	static const double odd[] = {NAN, INFINITY, -INFINITY};
	ctui_heatmap o;
	ctui_heatmap_init(&o, odd, 1, 3);
	CHECK(!ctui_heatmap_range(&o, &lo, &hi) && lo == 0 &&
	      hi == 1); /* nothing finite */
	ctui_heatmap_init(&o, NULL, 0, 0);
	CHECK(!ctui_heatmap_range(&o, &lo, &hi));
	ctui_heatmap_init(&o, V, SIZE_MAX, 4); /* size overflow */
	CHECK(!ctui_heatmap_range(&o, &lo, &hi));

	CHECK(ctui_heatmap_step(NAN, 0, 10, 6) == -1);
	CHECK(ctui_heatmap_step(0, 0, 10, 6) == 0);
	CHECK(ctui_heatmap_step(10, 0, 10, 6) == 5);
	CHECK(ctui_heatmap_step(5, 0, 10, 6) == 3);
	CHECK(ctui_heatmap_step(-3, 0, 10, 6) == 0);
	CHECK(ctui_heatmap_step(99, 0, 10, 6) == 5);
	CHECK(ctui_heatmap_step(INFINITY, 0, 10, 6) == 5);
	CHECK(ctui_heatmap_step(-INFINITY, 0, 10, 6) == 0);
	CHECK(ctui_heatmap_step(3, 4, 4, 6) == 0);  /* empty range */
	CHECK(ctui_heatmap_step(3, 5, 4, 6) == 0);  /* reversed range */
	CHECK(ctui_heatmap_step(3, 0, 10, 0) == 0); /* no steps */
}

static void test_layout(void)
{
	ctui_heatmap h;
	fixture(&h);
	ctui_heatmap_layout l;
	CHECK(ctui_heatmap_layout_for(&h, 40, 12, &l));
	CHECK(l.header_row == 0 && l.legend_row == 11 && l.label_width == 4);
	CHECK(l.grid.y == 1 && l.grid.x == 4);
	CHECK(l.cell_w == 8 && l.cell_h == 3 && l.grid.width == 24 &&
	      l.grid.height == 9);

	CHECK(
	    ctui_heatmap_layout_for(&h, 5, 5, &l)); /* one character per cell */
	CHECK(
	    !ctui_heatmap_layout_for(&h, 4, 12, &l)); /* labels leave no room */
	CHECK(!ctui_heatmap_layout_for(
	    &h, 40, 3, &l)); /* header and legend eat the rows */
	h.legend = false;
	h.col_labels = NULL;
	h.row_labels = NULL;
	CHECK(ctui_heatmap_layout_for(&h, 3, 3, &l) && l.cell_w == 1 &&
	      l.cell_h == 1);
	CHECK(l.header_row == -1 && l.legend_row == -1 && l.label_width == 0);
	CHECK(!ctui_heatmap_layout_for(&h, 0, 3, &l));
	CHECK(!ctui_heatmap_layout_for(&h, 3, 0, &l));
	CHECK(ctui_heatmap_layout_for(&h, INT_MAX, INT_MAX, &l) &&
	      l.cell_w == 8 && l.cell_h == 4);

	ctui_heatmap e;
	ctui_heatmap_init(&e, V, 0, 3); /* empty */
	CHECK(!ctui_heatmap_layout_for(&e, 40, 12, &l));
	ctui_heatmap_init(&e, V, 3, 0);
	CHECK(!ctui_heatmap_layout_for(&e, 40, 12, &l));
	ctui_heatmap_init(&e, NULL, 3, 3);
	CHECK(!ctui_heatmap_layout_for(&e, 40, 12, &l));
	ctui_heatmap_init(&e, V, (size_t)INT_MAX + 1, 3); /* huge */
	CHECK(!ctui_heatmap_layout_for(&e, INT_MAX, INT_MAX, &l));
	ctui_heatmap_init(&e, V, 3, (size_t)INT_MAX + 1);
	CHECK(!ctui_heatmap_layout_for(&e, INT_MAX, INT_MAX, &l));
}

static void test_hit(void)
{
	ctui_heatmap h;
	fixture(&h);
	size_t r, c;
	double v;
	/* grid at (1, 4), cells 8 x 3 */
	CHECK(ctui_heatmap_hit(&h, 40, 12, 1, 4, &r, &c, &v) && r == 0 &&
	      c == 0 && v == 0);
	CHECK(ctui_heatmap_hit(&h, 40, 12, 3, 11, &r, &c, &v) && r == 0 &&
	      c == 0);
	CHECK(ctui_heatmap_hit(&h, 40, 12, 4, 12, &r, &c, &v) && r == 1 &&
	      c == 1 && v == 4);
	CHECK(ctui_heatmap_hit(&h, 40, 12, 9, 27, &r, &c, &v) && r == 2 &&
	      c == 2 && v == 10);
	CHECK(ctui_heatmap_hit(&h, 40, 12, 5, 20, &r, &c, &v) && r == 1 &&
	      c == 2 && v == 5);
	CHECK(ctui_heatmap_hit(&h, 40, 12, 7, 4, &r, &c, &v) && r == 2 &&
	      c == 0 && isnan(v)); /* empty cell */
	CHECK(ctui_heatmap_hit(&h, 40, 12, 6, 12, &r, &c, &v) && r == 1 &&
	      c == 1);
	CHECK(ctui_heatmap_hit(&h, 40, 12, 4, 20, NULL, NULL, NULL));
	CHECK(!ctui_heatmap_hit(&h, 40, 12, 0, 4, &r, &c, &v)); /* header */
	CHECK(!ctui_heatmap_hit(&h, 40, 12, 1, 3, &r, &c, &v)); /* labels */
	CHECK(!ctui_heatmap_hit(&h, 40, 12, 1, 28, &r, &c,
				&v)); /* right of the grid */
	CHECK(!ctui_heatmap_hit(&h, 40, 12, 10, 4, &r, &c,
				&v)); /* under the grid */
	CHECK(!ctui_heatmap_hit(&h, 40, 12, -1, -1, &r, &c, &v));
	CHECK(!ctui_heatmap_hit(&h, 2, 2, 0, 0, &r, &c, &v)); /* too small */

	ctui_heatmap n;
	static const double nan_only[] = {NAN};
	ctui_heatmap_init(&n, nan_only, 1, 1);
	CHECK(ctui_heatmap_hit(&n, 10, 10, 0, 0, &r, &c, &v) && isnan(v));
}

static void test_keys(void)
{
	ctui_heatmap h;
	fixture(&h);
	CHECK(ctui_heatmap_key(&h, KEY_UP) && h.cursor_row == 0);
	CHECK(ctui_heatmap_key(&h, KEY_LEFT) && h.cursor_col == 0);
	CHECK(ctui_heatmap_key(&h, KEY_DOWN) && h.cursor_row == 1);
	CHECK(ctui_heatmap_key(&h, KEY_RIGHT) && h.cursor_col == 1);
	ctui_heatmap_key(&h, KEY_DOWN);
	ctui_heatmap_key(&h, KEY_DOWN);
	ctui_heatmap_key(&h, KEY_RIGHT);
	ctui_heatmap_key(&h, KEY_RIGHT);
	CHECK(h.cursor_row == 2 && h.cursor_col == 2); /* clamped */
	CHECK(ctui_heatmap_key(&h, KEY_HOME) && h.cursor_row == 0 &&
	      h.cursor_col == 0);
	CHECK(ctui_heatmap_key(&h, KEY_END) && h.cursor_row == 2 &&
	      h.cursor_col == 2);
	CHECK(!ctui_heatmap_key(&h, 'x'));

	ctui_heatmap e;
	ctui_heatmap_init(&e, NULL, 0,
			  0); /* no data: keys do nothing, nothing breaks */
	CHECK(ctui_heatmap_key(&e, KEY_END) && e.cursor_row == 0 &&
	      e.cursor_col == 0);
	CHECK(ctui_heatmap_key(&e, KEY_DOWN) &&
	      ctui_heatmap_key(&e, KEY_RIGHT));
	CHECK(e.cursor_row == 0 && e.cursor_col == 0);
}

static void test_draw(void)
{
	ctui_heatmap h;
	fixture(&h);
	screen_clear();
	ctui_heatmap_draw(&h, 1, 2, 40, 12);
	CHECK(screen_has_text("mon") && screen_has_text("wed"));
	CHECK(screen_has_text("noon"));
	CHECK(screen_has_text("0 ") && screen_has_text("10")); /* legend */
	CHECK(
	    screen_has_color(ctui_attr_color(-1, ctui_theme_palette()->info)));
	CHECK(screen_has_color(ctui_attr_color(-1, ctui_theme_palette()->red)));
	CHECK(screen_has_color(
	    ctui_attr(CTUI_PAIR_PLAYHEAD, false))); /* the cursor */

	h.show_cursor = false;
	screen_clear();
	ctui_heatmap_draw(&h, 1, 2, 40, 12);
	CHECK(!screen_has_color(ctui_attr(CTUI_PAIR_PLAYHEAD, false)));

	h.legend = false;
	h.fixed = true;
	h.min = 0;
	h.max = 100;
	screen_clear();
	ctui_heatmap_draw(&h, 1, 2, 40, 12);
	CHECK(!screen_has_text("100"));

	ctui_style style = {0};
	style.attr[CTUI_PAIR_MUTED] = ctui_attr_color(1, 2);
	h.style = &style;
	screen_clear();
	ctui_heatmap_draw(&h, 1, 2, 40, 12);
	CHECK(screen_has_color(style.attr[CTUI_PAIR_MUTED]));

	screen_clear();
	ctui_heatmap_draw(&h, 1, 2, 2, 2); /* too small: nothing, no crash */
	CHECK(!screen_has_text("mon"));

	static const double odd[] = {NAN, INFINITY, -INFINITY, 1e300};
	ctui_heatmap o;
	ctui_heatmap_init(&o, odd, 2, 2);
	screen_clear();
	ctui_heatmap_draw(&o, 0, 0, 20, 8);
	CHECK(!screen_has_text("mon"));
}

int main(void)
{
	screen_open(24, 80);
	RUN(test_range_and_step);
	RUN(test_layout);
	RUN(test_hit);
	RUN(test_keys);
	RUN(test_draw);
	setenv("CTUI_ASCII", "1", 1);
	ctui_theme_init();
	{
		ctui_heatmap h;
		fixture(&h);
		screen_clear();
		ctui_heatmap_draw(&h, 1, 2, 40, 12);
		CHECK(screen_has_text("@") && screen_has_text("."));
		CHECK(!screen_has_color(
		    ctui_attr_color(-1, ctui_theme_palette()->red)));
	}
	endwin();
	return CHECK_DONE();
}
