/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * test_histogram.c - binning, bin rule, layout, hit testing and drawing of
 * the histogram.
 */
#include <limits.h>
#include <math.h>
#include <stdlib.h>

#include "check.h"
#include "screen.h"
#include <ctui/histogram.h>

static const double S[] = {0, 1, 1, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 9, 10};
#define N (sizeof S / sizeof S[0])

static void test_auto_bins(void)
{
	CHECK(ctui_histogram_auto_bins(0) == 1 &&
	      ctui_histogram_auto_bins(1) == 1);
	CHECK(ctui_histogram_auto_bins(2) == 2);
	CHECK(ctui_histogram_auto_bins(8) == 4); /* log2(8) + 1 */
	CHECK(ctui_histogram_auto_bins(9) == 5); /* ceil(log2(9)) + 1 */
	CHECK(ctui_histogram_auto_bins(100) == 8);
	CHECK(ctui_histogram_auto_bins(1000000) == 21);
	CHECK(ctui_histogram_auto_bins(SIZE_MAX) == 65);
}

static void test_range(void)
{
	double lo, hi;
	CHECK(ctui_histogram_range(S, N, &lo, &hi) && lo == 0 && hi == 10);
	static const double odd[] = {NAN, INFINITY, -INFINITY, 5, -2};
	CHECK(ctui_histogram_range(odd, 5, &lo, &hi) && lo == -2 && hi == 5);
	CHECK(!ctui_histogram_range(odd, 3, &lo, &hi) && lo == 0 && hi == 1);
	CHECK(!ctui_histogram_range(NULL, 0, &lo, &hi) && lo == 0 && hi == 1);
	CHECK(!ctui_histogram_range(NULL, 5, &lo, &hi));
}

static void test_bin(void)
{
	size_t c[5];
	/* [0, 10] in 5 bins of width 2: 0,1,1 | 2,2,2,3,3,3,3 | 4x5 | - | 9,10
	 */
	CHECK(ctui_histogram_bin(S, N, 5, 0, 10, false, c) == N);
	CHECK(c[0] == 3 && c[1] == 7 && c[2] == 5 && c[3] == 0 && c[4] == 2);

	/* outside samples: ignored, or clamped to the end bins */
	CHECK(ctui_histogram_bin(S, N, 5, 2, 8, false, c) == 12);
	CHECK(c[0] == 7 && c[1] == 5 && c[2] == 0 && c[3] == 0 && c[4] == 0);
	CHECK(ctui_histogram_bin(S, N, 5, 2, 8, true, c) == N);
	CHECK(c[0] == 10 && c[4] == 2);

	static const double odd[] = {NAN, INFINITY, -INFINITY, 1};
	CHECK(ctui_histogram_bin(odd, 4, 5, 0, 2, false, c) == 1 && c[2] == 1);
	CHECK(ctui_histogram_bin(odd, 4, 5, 0, 2, true, c) == 3);
	CHECK(c[0] == 1 && c[4] == 1 && c[2] == 1);

	/* empty range and degenerate inputs */
	CHECK(ctui_histogram_bin(S, N, 5, 3, 3, false, c) == 4 && c[0] == 4);
	CHECK(ctui_histogram_bin(S, N, 5, 4, 3, false, c) == 0);
	CHECK(ctui_histogram_bin(S, N, 0, 0, 10, false, c) == 0);
	CHECK(ctui_histogram_bin(S, N, 5, 0, 10, false, NULL) == 0);
	CHECK(ctui_histogram_bin(NULL, 0, 5, 0, 10, false, c) == 0 &&
	      c[0] == 0 && c[4] == 0);
	CHECK(ctui_histogram_bin(S, 0, 5, 0, 10, false, c) == 0);
	size_t one[1];
	CHECK(ctui_histogram_bin(S, N, 1, 0, 10, false, one) == N &&
	      one[0] == N);

	/* a range wider than a double: no overflow, samples land in the two
	 * ends */
	static const double huge[] = {-1.7e308, 0.5e308, 1.7e308, -0.5e308};
	CHECK(ctui_histogram_bin(huge, 4, 4, -1.7e308, 1.7e308, false, c) == 4);
	CHECK(c[0] == 1 && c[1] == 1 && c[2] == 1 && c[3] == 1);
	/* many bins */
	size_t *many = malloc(100000 * sizeof *many);
	CHECK(ctui_histogram_bin(S, N, 100000, 0, 10, false, many) == N);
	CHECK(many[0] == 1 && many[10000] == 2 && many[99999] == 1);
	free(many);
}

static void test_layout(void)
{
	ctui_histogram h;
	ctui_histogram_init(&h, S, N);
	ctui_histogram_layout l;
	CHECK(!ctui_histogram_layout_for(&h, 5, 10, &l));
	CHECK(!ctui_histogram_layout_for(&h, 40, 2, &l));
	CHECK(ctui_histogram_layout_for(&h, 40, 12, &l));
	CHECK(l.bins == ctui_histogram_auto_bins(N) && l.bins == 6);
	CHECK(l.lo == 0 && l.hi >= 10 && l.labels_row == 11 &&
	      l.plot.height == 11);
	CHECK(l.plot.x == l.label_width + 1 && l.plot.x + l.plot.width == 40);
	CHECK(l.tallest > 0 && l.counts.max >= (double)l.tallest);

	h.bins = 50; /* more bins than columns: one per column at most */
	CHECK(ctui_histogram_layout_for(&h, 20, 8, &l) &&
	      l.bins == (size_t)l.plot.width);
	h.bins = 5;
	CHECK(ctui_histogram_layout_for(&h, 40, 12, &l) && l.bins == 5);
	h.bins = 1000000;
	CHECK(ctui_histogram_layout_for(&h, INT_MAX, 12, &l) &&
	      l.bins == CTUI_HISTOGRAM_MAX_BINS);

	h.ranged = true;
	h.min = 2;
	h.max = 6;
	CHECK(ctui_histogram_layout_for(&h, 40, 12, &l) && l.lo == 2 &&
	      l.hi == 6);
	h.max = 1; /* unusable: the data's range */
	CHECK(ctui_histogram_layout_for(&h, 40, 12, &l) && l.lo == 0);
	h.min = NAN;
	CHECK(ctui_histogram_layout_for(&h, 40, 12, &l) && l.lo == 0);

	static const double one[] = {7, 7, 7};
	ctui_histogram_init(&h, one, 3); /* one value: the range is widened */
	CHECK(ctui_histogram_layout_for(&h, 40, 12, &l) && l.hi > l.lo &&
	      l.lo < 7 && l.hi > 7);
	CHECK(l.tallest == 3);

	ctui_histogram_init(&h, NULL, 0); /* no data: still a layout */
	CHECK(ctui_histogram_layout_for(&h, 40, 12, &l) && l.tallest == 0 &&
	      l.bins == 1);
	static const double nans[] = {NAN, NAN};
	ctui_histogram_init(&h, nans, 2);
	CHECK(ctui_histogram_layout_for(&h, 40, 12, &l) && l.tallest == 0);
	ctui_histogram_init(&h, S, N);
	CHECK(ctui_histogram_layout_for(&h, INT_MAX, INT_MAX, &l));
	CHECK(ctui_histogram_layout_for(&h, 40, 4, &l) && l.labels_row == -1);
}

static void test_hit(void)
{
	ctui_histogram h;
	ctui_histogram_init(&h, S, N);
	h.bins = 5;
	h.ranged = true;
	h.min = 0;
	h.max = 10;
	ctui_histogram_layout l;
	CHECK(ctui_histogram_layout_for(&h, 41, 12, &l) && l.bins == 5);
	size_t bin = 99, count = 99;
	double from = -1, to = -1;
	int bottom = l.plot.y + l.plot.height - 1;

	CHECK(ctui_histogram_hit(&h, 41, 12, bottom, l.plot.x, &bin, &count,
				 &from, &to));
	CHECK(bin == 0 && count == 3 && from == 0 && to == 2);
	/* any row of the column counts, even above the bar */
	CHECK(ctui_histogram_hit(&h, 41, 12, 0, l.plot.x + 8, &bin, &count,
				 &from, &to));
	CHECK(bin == 1 && count == 7 && from == 2 && to == 4);
	CHECK(ctui_histogram_hit(&h, 41, 12, bottom,
				 l.plot.x + l.plot.width - 1, &bin, &count,
				 &from, &to));
	CHECK(bin == 4 && count == 2 && from == 8 && to == 10);
	CHECK(ctui_histogram_hit(&h, 41, 12, bottom, l.plot.x, NULL, NULL, NULL,
				 NULL));

	/* every column belongs to the bin whose bar is drawn there */
	size_t prev = 0;
	for (int x = l.plot.x; x < l.plot.x + l.plot.width; x++) {
		CHECK(ctui_histogram_hit(&h, 41, 12, bottom, x, &bin, NULL,
					 NULL, NULL));
		CHECK(bin >= prev && bin < 5);
		prev = bin;
	}

	CHECK(!ctui_histogram_hit(&h, 41, 12, bottom, l.plot.x - 1, &bin,
				  &count, &from, &to));
	CHECK(!ctui_histogram_hit(&h, 41, 12, bottom, 0, &bin, &count, &from,
				  &to));
	CHECK(!ctui_histogram_hit(&h, 41, 12, bottom, 41, &bin, &count, &from,
				  &to));
	CHECK(!ctui_histogram_hit(&h, 41, 12, bottom + 1, l.plot.x, &bin,
				  &count, &from, &to));
	CHECK(!ctui_histogram_hit(&h, 41, 12, -1, l.plot.x, &bin, &count, &from,
				  &to));
	CHECK(!ctui_histogram_hit(&h, 5, 12, 0, 0, &bin, &count, &from,
				  &to)); /* too small */

	/* the empty bin */
	CHECK(ctui_histogram_hit(&h, 41, 12, bottom,
				 l.plot.x + 3 * l.plot.width / 5 + 1, &bin,
				 &count, NULL, NULL));
	CHECK(bin == 3 && count == 0);
	/* huge widths: no overflow in the column to bin mapping */
	CHECK(ctui_histogram_hit(&h, INT_MAX, 12, 0, INT_MAX - 1, &bin, &count,
				 &from, &to));
	CHECK(bin == 4 && to == 10);
}

static void test_draw(void)
{
	ctui_histogram h;
	ctui_histogram_init(&h, S, N);
	h.bins = 5;
	screen_clear();
	ctui_histogram_draw(&h, 1, 2, 44, 14);
	CHECK(screen_has_text("\xe2\x96\x88")); /* a full block */
	CHECK(screen_has_color(ctui_attr_icon(CTUI_ICON_BLUE, false)));
	CHECK(screen_has_text("10")); /* the end of the value axis */
	CHECK(screen_has_text("0"));

	h.color = CTUI_ICON_ORANGE;
	screen_clear();
	ctui_histogram_draw(&h, 1, 2, 44, 14);
	CHECK(screen_has_color(ctui_attr_icon(CTUI_ICON_ORANGE, false)));

	/* eighths: one sample out of 8 is not a full cell */
	static const double eight[] = {1, 1, 1, 1, 1, 1, 1, 1, 5};
	ctui_histogram f;
	ctui_histogram_init(&f, eight, 9);
	f.bins = 2;
	f.ranged = true;
	f.min = 0;
	f.max = 10;
	screen_clear();
	ctui_histogram_draw(&f, 0, 0, 30, 10);
	CHECK(screen_has_text("\xe2\x96\x81") ||
	      screen_has_text("\xe2\x96\x82") ||
	      screen_has_text("\xe2\x96\x83") ||
	      screen_has_text("\xe2\x96\x84") ||
	      screen_has_text("\xe2\x96\x85") ||
	      screen_has_text("\xe2\x96\x86") ||
	      screen_has_text("\xe2\x96\x87"));

	ctui_style style = {0};
	style.attr[CTUI_PAIR_MUTED] = ctui_attr_color(1, 2);
	h.style = &style;
	screen_clear();
	ctui_histogram_draw(&h, 1, 2, 44, 14);
	CHECK(screen_has_color(style.attr[CTUI_PAIR_MUTED]));

	screen_clear();
	ctui_histogram_draw(&h, 1, 2, 4, 2); /* too small */
	CHECK(!screen_has_text("\xe2\x96\x88"));

	/* no data, NaN, a single value, huge areas and partly off-screen: no
	 * crash */
	ctui_histogram e;
	ctui_histogram_init(&e, NULL, 0);
	ctui_histogram_draw(&e, 0, 0, 30, 10);
	static const double nans[] = {NAN, NAN, INFINITY};
	ctui_histogram_init(&e, nans, 3);
	ctui_histogram_draw(&e, 0, 0, 30, 10);
	static const double one[] = {3};
	ctui_histogram_init(&e, one, 1);
	screen_clear();
	ctui_histogram_draw(&e, 0, 0, 30, 10);
	CHECK(screen_has_text("\xe2\x96\x88"));
	ctui_histogram_init(&e, S, N);
	ctui_histogram_draw(&e, 0, 0, INT_MAX, INT_MAX);
	ctui_histogram_draw(&e, -7, -9, 30, 10);
	ctui_histogram_draw(&e, 1000, 1000, 30, 10);
}

int main(void)
{
	screen_open(24, 80);
	RUN(test_auto_bins);
	RUN(test_range);
	RUN(test_bin);
	RUN(test_layout);
	RUN(test_hit);
	RUN(test_draw);
	setenv("CTUI_ASCII", "1", 1);
	ctui_theme_init();
	{
		ctui_histogram h;
		ctui_histogram_init(&h, S, N);
		screen_clear();
		ctui_histogram_draw(&h, 1, 2, 44, 14);
		CHECK(screen_has_text("#") && !screen_has_text("\xe2\x96\x88"));
	}
	endwin();
	return CHECK_DONE();
}
