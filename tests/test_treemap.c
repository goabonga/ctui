/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * test_treemap.c - squarified layout, ignored weights, hit testing and
 * drawing of the treemap.
 */
#include <limits.h>
#include <math.h>
#include <stdlib.h>

#include "check.h"
#include "screen.h"
#include <ctui/treemap.h>

static const ctui_treemap_item ITEMS[] = {{"src", 6},	{"docs", 6},
					  {"tests", 4}, {"demo", 3},
					  {"misc", 2},	{"tools", 1}};
#define N (sizeof ITEMS / sizeof ITEMS[0])

static long area_of(ctui_rect r)
{
	return (long)r.width * r.height;
}

/* Every cell of the area is covered by exactly one rectangle. */
static bool tiles(const ctui_rect *r, size_t n, ctui_rect area)
{
	long total = 0;
	for (size_t i = 0; i < n; i++) {
		if (r[i].width == 0 || r[i].height == 0)
			continue;
		if (r[i].x < area.x || r[i].y < area.y ||
		    r[i].x + r[i].width > area.x + area.width ||
		    r[i].y + r[i].height > area.y + area.height)
			return false;
		total += area_of(r[i]);
		for (size_t j = i + 1; j < n; j++)
			if (r[j].width && r[j].height &&
			    r[i].x < r[j].x + r[j].width &&
			    r[j].x < r[i].x + r[i].width &&
			    r[i].y < r[j].y + r[j].height &&
			    r[j].y < r[i].y + r[i].height)
				return false;
	}
	return total == area_of(area);
}

static void test_weight_ok(void)
{
	CHECK(ctui_treemap_weight_ok(1) && ctui_treemap_weight_ok(1e-300) &&
	      ctui_treemap_weight_ok(1e300));
	CHECK(!ctui_treemap_weight_ok(0) && !ctui_treemap_weight_ok(-1) &&
	      !ctui_treemap_weight_ok(NAN));
	CHECK(!ctui_treemap_weight_ok(INFINITY) &&
	      !ctui_treemap_weight_ok(-INFINITY));
}

static void test_layout(void)
{
	ctui_rect area = {.y = 2, .x = 3, .height = 12, .width = 40}, r[N];
	CHECK(ctui_treemap_squarify(ITEMS, N, area, r) == N);
	CHECK(tiles(r, N, area));
	/* the largest items get the largest areas, in order */
	CHECK(area_of(r[0]) >= area_of(r[2]) && area_of(r[2]) >= area_of(r[3]));
	CHECK(area_of(r[3]) >= area_of(r[4]) && area_of(r[4]) >= area_of(r[5]));
	/* areas follow the weights within rounding */
	CHECK_NEAR((double)area_of(r[0]) / (12 * 40), 6.0 / 22, 0.06);
	CHECK_NEAR((double)area_of(r[5]) / (12 * 40), 1.0 / 22, 0.04);

	/* deterministic: the same call gives the same rectangles */
	ctui_rect again[N];
	ctui_treemap_squarify(ITEMS, N, area, again);
	CHECK(memcmp(r, again, sizeof r) == 0);

	/* equal weights keep their order: the first one comes first */
	static const ctui_treemap_item same[] = {
	    {"a", 1}, {"b", 1}, {"c", 1}, {"d", 1}};
	ctui_rect q[4];
	CHECK(ctui_treemap_squarify(
		  same, 4, (ctui_rect){.height = 10, .width = 20}, q) == 4);
	CHECK(tiles(q, 4, (ctui_rect){.height = 10, .width = 20}));
	CHECK(q[0].y == 0 && q[0].x == 0);
	for (int i = 1; i < 4; i++)
		CHECK(area_of(q[i]) == area_of(q[0]));
}

static void test_aspect(void)
{
	/* two equal items in a wide area sit side by side: cells are twice as
	 * tall as wide */
	static const ctui_treemap_item two[] = {{"a", 1}, {"b", 1}};
	ctui_rect r[2];
	CHECK(ctui_treemap_squarify(
		  two, 2, (ctui_rect){.height = 10, .width = 40}, r) == 2);
	CHECK(r[0].height == 10 && r[1].height == 10 && r[0].width == 20 &&
	      r[1].x == 20);
	/* a single item takes everything */
	CHECK(ctui_treemap_squarify(
		  two, 1, (ctui_rect){.y = 1, .x = 2, .height = 5, .width = 7},
		  r) == 1);
	CHECK(r[0].y == 1 && r[0].x == 2 && r[0].height == 5 &&
	      r[0].width == 7);
}

static void test_ignored(void)
{
	static const ctui_treemap_item bad[] = {
	    {"nan", NAN}, {"neg", -3}, {"zero", 0},	   {"inf", INFINITY},
	    {"one", 1},	  {NULL, 3},   {"ninf", -INFINITY}};
	ctui_rect area = {.height = 8, .width = 20}, r[7];
	CHECK(ctui_treemap_squarify(bad, 7, area, r) == 2);
	CHECK(r[0].width == 0 && r[1].height == 0 && r[2].width == 0 &&
	      r[3].width == 0);
	CHECK(r[6].width == 0);
	CHECK(tiles(r, 7, area));
	CHECK(area_of(r[5]) > area_of(r[4]));

	static const ctui_treemap_item none[] = {{"a", 0}, {"b", NAN}};
	CHECK(ctui_treemap_squarify(none, 2, area, r) == 0 && r[0].width == 0 &&
	      r[1].height == 0);
	CHECK(ctui_treemap_squarify(NULL, 0, area, r) == 0);
	CHECK(ctui_treemap_squarify(ITEMS, 0, area, r) == 0);
	CHECK(ctui_treemap_squarify(
		  ITEMS, N, (ctui_rect){.height = 0, .width = 10}, r) == 0);
	CHECK(ctui_treemap_squarify(
		  ITEMS, N, (ctui_rect){.height = 10, .width = -1}, r) == 0);
	CHECK(ctui_treemap_squarify(ITEMS, N, area, NULL) == 0);
}

static void test_extreme(void)
{
	/* huge and tiny weights together: no overflow, the light ones vanish */
	static const ctui_treemap_item wide[] = {
	    {"big", 1e300}, {"huge", 1.5e308}, {"tiny", 1e-300}};
	ctui_rect area = {.height = 10, .width = 40}, r[3];
	CHECK(ctui_treemap_squarify(wide, 3, area, r) == 1);
	CHECK(r[0].width == 0 && r[2].width == 0 && r[1].width == 40 &&
	      tiles(r, 3, area));
	static const ctui_treemap_item denorm[] = {{"a", 4.9e-324},
						   {"b", 1e308}};
	CHECK(ctui_treemap_squarify(denorm, 2, area, r) == 1);
	CHECK(tiles(r, 2, area));

	/* huge areas */
	ctui_rect big = {.y = 0, .x = 0, .height = INT_MAX, .width = INT_MAX},
		  h[N];
	CHECK(ctui_treemap_squarify(ITEMS, N, big, h) == N);
	CHECK(tiles(h, N, big));
	big.x = 1; /* x + width would overflow int */
	CHECK(ctui_treemap_squarify(ITEMS, N, big, h) == 0);
	big = (ctui_rect){
	    .y = INT_MAX - 5, .x = INT_MAX - 7, .height = 5, .width = 7};
	CHECK(ctui_treemap_squarify(ITEMS, N, big, h) > 0 && tiles(h, N, big));

	/* many items in a small area: everything is inside, nothing overlaps */
	enum { MANY = 500 };
	ctui_treemap_item *items = malloc(MANY * sizeof *items);
	ctui_rect *rects = malloc(MANY * sizeof *rects);
	for (int i = 0; i < MANY; i++)
		items[i] =
		    (ctui_treemap_item){.label = "x", .weight = 1 + i % 7};
	ctui_rect small = {.height = 6, .width = 30};
	size_t placed = ctui_treemap_squarify(items, MANY, small, rects);
	CHECK(placed > 0 && placed <= 180 && tiles(rects, MANY, small));
	free(items);
	free(rects);
}

static void test_hit(void)
{
	ctui_treemap t;
	ctui_treemap_init(&t, ITEMS, N);
	ctui_rect r[N];
	ctui_rect area = {.height = 12, .width = 40};
	ctui_treemap_squarify(ITEMS, N, area, r);
	for (size_t i = 0; i < N; i++) {
		CHECK(ctui_treemap_hit(&t, 40, 12, r[i].y, r[i].x) == (long)i);
		CHECK(ctui_treemap_hit(&t, 40, 12, r[i].y + r[i].height - 1,
				       r[i].x + r[i].width - 1) == (long)i);
	}
	CHECK(ctui_treemap_hit(&t, 40, 12, -1, 0) == -1);
	CHECK(ctui_treemap_hit(&t, 40, 12, 0, 40) == -1);
	CHECK(ctui_treemap_hit(&t, 40, 12, 12, 0) == -1);
	CHECK(ctui_treemap_hit(&t, 0, 12, 0, 0) == -1);
	ctui_treemap e;
	ctui_treemap_init(&e, NULL, 0);
	CHECK(ctui_treemap_hit(&e, 40, 12, 0, 0) == -1);
	static const ctui_treemap_item bad[] = {{"a", NAN}, {"b", 2}};
	ctui_treemap_init(&e, bad, 2);
	CHECK(ctui_treemap_hit(&e, 10, 4, 2, 5) == 1);
}

static void test_draw(void)
{
	ctui_treemap t;
	ctui_treemap_init(&t, ITEMS, N);
	screen_clear();
	ctui_treemap_draw(&t, 1, 2, 60, 18);
	CHECK(screen_has_text("src") && screen_has_text("docs"));
	CHECK(screen_has_text("6")); /* a weight */
	const ctui_palette *p = ctui_theme_palette();
	CHECK(screen_has_color(ctui_attr_color(p->black, p->icon[0])));
	CHECK(screen_has_color(ctui_attr_color(p->black, p->icon[1])));

	t.show_weights = false;
	screen_clear();
	ctui_treemap_draw(&t, 1, 2, 60, 18);
	CHECK(screen_has_text("src") && !screen_has_text("src 6"));

	/* labels are clipped to their rectangle */
	static const ctui_treemap_item longs[] = {
	    {"a very long label that cannot fit", 1},
	    {"another very long label", 1}};
	ctui_treemap l;
	ctui_treemap_init(&l, longs, 2);
	screen_clear();
	ctui_treemap_draw(&l, 0, 0, 20, 6);
	CHECK(!screen_has_text("cannot"));
	CHECK(screen_has_text("a very"));

	ctui_style style = {0};
	style.attr[CTUI_PAIR_BODY] = ctui_attr_color(1, 2);
	ctui_treemap_init(&l, ITEMS, N);
	l.style = &style;
	screen_clear();
	ctui_treemap_draw(&l, 0, 0, 60, 18);
	CHECK(screen_has_text("src"));

	screen_clear();
	ctui_treemap_draw(&l, 0, 0, 0, 5); /* nothing to draw */
	ctui_treemap_draw(&l, 0, 0, INT_MAX,
			  INT_MAX); /* huge: only the screen is visited */
	CHECK(screen_has_text("src"));
	ctui_treemap_draw(&l, -5, -5, 3, 3);
	ctui_treemap_init(&l, NULL, 0);
	ctui_treemap_draw(&l, 0, 0, 10, 10);
}

int main(void)
{
	screen_open(24, 80);
	RUN(test_weight_ok);
	RUN(test_layout);
	RUN(test_aspect);
	RUN(test_ignored);
	RUN(test_extreme);
	RUN(test_hit);
	RUN(test_draw);
	setenv("CTUI_ASCII", "1", 1);
	ctui_theme_init();
	{
		ctui_treemap t;
		ctui_treemap_init(&t, ITEMS, N);
		screen_clear();
		ctui_treemap_draw(&t, 1, 2, 60, 18);
		CHECK(screen_has_text("src") && screen_has_text("+--"));
	}
	endwin();
	return CHECK_DONE();
}
