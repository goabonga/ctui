/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */
#include "check.h"

#include <limits.h>

#include <ctui/layout.h>

#define MAXC 8

typedef struct row_case {
	const char *name;
	ctui_layout_flex flex;
	int spacing;
	int width;
	ctui_constraint cons[MAXC];
	int n;
	int want_x[MAXC], want_w[MAXC];
} row_case;

static const row_case ROWS[] = {
    {"lengths",
     CTUI_LAYOUT_START,
     0,
     20,
     {{CTUI_CONSTRAINT_LENGTH, 5, 1}, {CTUI_CONSTRAINT_LENGTH, 7, 1}},
     2,
     {0, 5},
     {5, 7}},
    {"length cut",
     CTUI_LAYOUT_START,
     0,
     10,
     {{CTUI_CONSTRAINT_LENGTH, 8, 1}, {CTUI_CONSTRAINT_LENGTH, 8, 1}},
     2,
     {0, 8},
     {8, 2}},
    {"percent",
     CTUI_LAYOUT_START,
     0,
     50,
     {{CTUI_CONSTRAINT_PERCENT, 30, 1}, {CTUI_CONSTRAINT_PERCENT, 50, 1}},
     2,
     {0, 15},
     {15, 25}},
    {"percent over 100",
     CTUI_LAYOUT_START,
     0,
     10,
     {{CTUI_CONSTRAINT_PERCENT, 300, 1}},
     1,
     {0},
     {10}},
    {"ratio",
     CTUI_LAYOUT_START,
     0,
     30,
     {{CTUI_CONSTRAINT_RATIO, 1, 3}, {CTUI_CONSTRAINT_RATIO, 1, 3}},
     2,
     {0, 10},
     {10, 10}},
    {"ratio zero den",
     CTUI_LAYOUT_START,
     0,
     30,
     {{CTUI_CONSTRAINT_RATIO, 1, 0}, {CTUI_CONSTRAINT_LENGTH, 4, 1}},
     2,
     {0, 0},
     {0, 4}},
    {"ratio num over den",
     CTUI_LAYOUT_START,
     0,
     30,
     {{CTUI_CONSTRAINT_RATIO, 5, 2}},
     1,
     {0},
     {30}},
    {"fill weights",
     CTUI_LAYOUT_START,
     0,
     30,
     {{CTUI_CONSTRAINT_FILL, 1, 1}, {CTUI_CONSTRAINT_FILL, 2, 1}},
     2,
     {0, 10},
     {10, 20}},
    {"fill remainder",
     CTUI_LAYOUT_START,
     0,
     10,
     {{CTUI_CONSTRAINT_FILL, 1, 1},
      {CTUI_CONSTRAINT_FILL, 1, 1},
      {CTUI_CONSTRAINT_FILL, 1, 1}},
     3,
     {0, 4, 7},
     {4, 3, 3}},
    {"fill weight 0 is 1",
     CTUI_LAYOUT_START,
     0,
     10,
     {{CTUI_CONSTRAINT_FILL, 0, 1}, {CTUI_CONSTRAINT_FILL, 1, 1}},
     2,
     {0, 5},
     {5, 5}},
    {"mixture",
     CTUI_LAYOUT_START,
     0,
     100,
     {{CTUI_CONSTRAINT_LENGTH, 10, 1},
      {CTUI_CONSTRAINT_PERCENT, 20, 1},
      {CTUI_CONSTRAINT_FILL, 1, 1},
      {CTUI_CONSTRAINT_FILL, 3, 1}},
     4,
     {0, 10, 30, 48},
     {10, 20, 18, 52}},
    {"min grows alone",
     CTUI_LAYOUT_START,
     0,
     20,
     {{CTUI_CONSTRAINT_MIN, 5, 1}, {CTUI_CONSTRAINT_LENGTH, 5, 1}},
     2,
     {0, 15},
     {15, 5}},
    {"min shares",
     CTUI_LAYOUT_START,
     0,
     21,
     {{CTUI_CONSTRAINT_MIN, 5, 1}, {CTUI_CONSTRAINT_MIN, 5, 1}},
     2,
     {0, 11},
     {11, 10}},
    {"min yields to fill",
     CTUI_LAYOUT_START,
     0,
     20,
     {{CTUI_CONSTRAINT_MIN, 5, 1}, {CTUI_CONSTRAINT_FILL, 1, 1}},
     2,
     {0, 5},
     {5, 15}},
    {"min too big gets 0",
     CTUI_LAYOUT_START,
     0,
     10,
     {{CTUI_CONSTRAINT_LENGTH, 8, 1},
      {CTUI_CONSTRAINT_MIN, 5, 1},
      {CTUI_CONSTRAINT_FILL, 1, 1}},
     3,
     {0, 8, 8},
     {8, 0, 2}},
    {"max caps",
     CTUI_LAYOUT_START,
     0,
     20,
     {{CTUI_CONSTRAINT_MAX, 6, 1}, {CTUI_CONSTRAINT_FILL, 1, 1}},
     2,
     {0, 6},
     {6, 14}},
    {"max short",
     CTUI_LAYOUT_START,
     0,
     4,
     {{CTUI_CONSTRAINT_MAX, 6, 1}, {CTUI_CONSTRAINT_FILL, 1, 1}},
     2,
     {0, 4},
     {4, 0}},
    {"length beats max",
     CTUI_LAYOUT_START,
     0,
     10,
     {{CTUI_CONSTRAINT_MAX, 8, 1}, {CTUI_CONSTRAINT_LENGTH, 8, 1}},
     2,
     {0, 2},
     {2, 8}},
    {"max beats percent",
     CTUI_LAYOUT_START,
     0,
     10,
     {{CTUI_CONSTRAINT_PERCENT, 100, 1}, {CTUI_CONSTRAINT_MAX, 8, 1}},
     2,
     {0, 2},
     {2, 8}},
    {"percent beats fill",
     CTUI_LAYOUT_START,
     0,
     10,
     {{CTUI_CONSTRAINT_FILL, 1, 1}, {CTUI_CONSTRAINT_PERCENT, 100, 1}},
     2,
     {0, 0},
     {0, 10}},
    {"over-constrained",
     CTUI_LAYOUT_START,
     0,
     10,
     {{CTUI_CONSTRAINT_LENGTH, 6, 1},
      {CTUI_CONSTRAINT_LENGTH, 6, 1},
      {CTUI_CONSTRAINT_LENGTH, 6, 1}},
     3,
     {0, 6, 10},
     {6, 4, 0}},
    {"spacing",
     CTUI_LAYOUT_START,
     2,
     20,
     {{CTUI_CONSTRAINT_LENGTH, 4, 1},
      {CTUI_CONSTRAINT_FILL, 1, 1},
      {CTUI_CONSTRAINT_LENGTH, 4, 1}},
     3,
     {0, 6, 16},
     {4, 8, 4}},
    {"spacing cut",
     CTUI_LAYOUT_START,
     9,
     4,
     {{CTUI_CONSTRAINT_FILL, 1, 1},
      {CTUI_CONSTRAINT_FILL, 1, 1},
      {CTUI_CONSTRAINT_FILL, 1, 1}},
     3,
     {0, 2, 4},
     {0, 0, 0}},
    {"negative spacing",
     CTUI_LAYOUT_START,
     -3,
     10,
     {{CTUI_CONSTRAINT_FILL, 1, 1}, {CTUI_CONSTRAINT_FILL, 1, 1}},
     2,
     {0, 5},
     {5, 5}},
    {"flex start",
     CTUI_LAYOUT_START,
     0,
     20,
     {{CTUI_CONSTRAINT_LENGTH, 4, 1}, {CTUI_CONSTRAINT_LENGTH, 4, 1}},
     2,
     {0, 4},
     {4, 4}},
    {"flex center",
     CTUI_LAYOUT_CENTER,
     0,
     20,
     {{CTUI_CONSTRAINT_LENGTH, 4, 1}, {CTUI_CONSTRAINT_LENGTH, 4, 1}},
     2,
     {6, 10},
     {4, 4}},
    {"flex end",
     CTUI_LAYOUT_END,
     0,
     20,
     {{CTUI_CONSTRAINT_LENGTH, 4, 1}, {CTUI_CONSTRAINT_LENGTH, 4, 1}},
     2,
     {12, 16},
     {4, 4}},
    {"flex between",
     CTUI_LAYOUT_SPACE_BETWEEN,
     0,
     20,
     {{CTUI_CONSTRAINT_LENGTH, 4, 1},
      {CTUI_CONSTRAINT_LENGTH, 4, 1},
      {CTUI_CONSTRAINT_LENGTH, 4, 1}},
     3,
     {0, 8, 16},
     {4, 4, 4}},
    {"flex between one",
     CTUI_LAYOUT_SPACE_BETWEEN,
     0,
     20,
     {{CTUI_CONSTRAINT_LENGTH, 4, 1}},
     1,
     {0},
     {4}},
    {"flex between odd",
     CTUI_LAYOUT_SPACE_BETWEEN,
     1,
     12,
     {{CTUI_CONSTRAINT_LENGTH, 2, 1},
      {CTUI_CONSTRAINT_LENGTH, 2, 1},
      {CTUI_CONSTRAINT_LENGTH, 2, 1}},
     3,
     {0, 5, 10},
     {2, 2, 2}},
    {"flex around",
     CTUI_LAYOUT_SPACE_AROUND,
     0,
     20,
     {{CTUI_CONSTRAINT_LENGTH, 4, 1}, {CTUI_CONSTRAINT_LENGTH, 4, 1}},
     2,
     {3, 13},
     {4, 4}},
    {"flex stretch",
     CTUI_LAYOUT_STRETCH,
     0,
     20,
     {{CTUI_CONSTRAINT_LENGTH, 4, 1}, {CTUI_CONSTRAINT_LENGTH, 4, 1}},
     2,
     {0, 4},
     {4, 16}},
    {"flex ignored with fill",
     CTUI_LAYOUT_END,
     0,
     20,
     {{CTUI_CONSTRAINT_LENGTH, 4, 1}, {CTUI_CONSTRAINT_FILL, 1, 1}},
     2,
     {0, 4},
     {4, 16}},
    {"zero width",
     CTUI_LAYOUT_START,
     1,
     0,
     {{CTUI_CONSTRAINT_LENGTH, 4, 1},
      {CTUI_CONSTRAINT_FILL, 1, 1},
      {CTUI_CONSTRAINT_MIN, 2, 1}},
     3,
     {0, 0, 0},
     {0, 0, 0}},
    {"huge lengths",
     CTUI_LAYOUT_START,
     0,
     INT_MAX,
     {{CTUI_CONSTRAINT_LENGTH, INT_MAX, 1},
      {CTUI_CONSTRAINT_LENGTH, INT_MAX, 1}},
     2,
     {0, INT_MAX},
     {INT_MAX, 0}},
    {"huge ratio",
     CTUI_LAYOUT_START,
     0,
     INT_MAX,
     {{CTUI_CONSTRAINT_RATIO, INT_MAX, INT_MAX}, {CTUI_CONSTRAINT_FILL, 1, 1}},
     2,
     {0, INT_MAX},
     {INT_MAX, 0}},
    {"huge fills",
     CTUI_LAYOUT_START,
     0,
     INT_MAX,
     {{CTUI_CONSTRAINT_FILL, INT_MAX, 1}, {CTUI_CONSTRAINT_FILL, INT_MAX, 1}},
     2,
     {0, 1073741824},
     {1073741824, 1073741823}},
    {"huge percent",
     CTUI_LAYOUT_START,
     0,
     INT_MAX,
     {{CTUI_CONSTRAINT_PERCENT, INT_MAX, 1}, {CTUI_CONSTRAINT_LENGTH, 1, 1}},
     2,
     {0, INT_MAX - 1},
     {INT_MAX - 1, 1}},
    {"negative values",
     CTUI_LAYOUT_START,
     0,
     10,
     {{CTUI_CONSTRAINT_LENGTH, -5, 1},
      {CTUI_CONSTRAINT_PERCENT, -5, 1},
      {CTUI_CONSTRAINT_MIN, -1, 1},
      {CTUI_CONSTRAINT_FILL, 1, 1}},
     4,
     {0, 0, 0, 0},
     {0, 0, 0, 10}},
    {"empty",
     CTUI_LAYOUT_START,
     0,
     10,
     {{CTUI_CONSTRAINT_LENGTH, 1, 1}},
     0,
     {0},
     {0}},
};

static void test_rows(void)
{
	for (size_t k = 0; k < sizeof ROWS / sizeof ROWS[0]; k++) {
		const row_case *c = &ROWS[k];
		ctui_layout l = {.dir = CTUI_LAYOUT_ROW,
				 .flex = c->flex,
				 .spacing = c->spacing};
		ctui_rect area = {3, 0, 6, c->width}, out[MAXC] = {{0}};
		size_t got =
		    ctui_layout_split(&l, &area, c->cons, (size_t)c->n, out);
		if (got != (size_t)c->n) {
			fprintf(stderr, "case \"%s\": returned %zu\n", c->name,
				got);
			CHECK(got == (size_t)c->n);
			continue;
		}
		bool ok = true;
		long long end = area.x;
		for (int i = 0; i < c->n; i++) {
			ok = ok && (out[i].x == c->want_x[i] &&
				    out[i].width == c->want_w[i]);
			ok = ok && (out[i].y == 3 &&
				    out[i].height == 6); /* cross axis kept */
			ok = ok && (out[i].width >= 0 &&
				    out[i].x >= end); /* no overlap */
			end = (long long)out[i].x + out[i].width;
		}
		ok = ok && (end <= (long long)area.x + area.width);
		if (!ok) {
			fprintf(stderr, "case \"%s\":", c->name);
			for (int i = 0; i < c->n; i++)
				fprintf(stderr, " [%d+%d]", out[i].x,
					out[i].width);
			fprintf(stderr, "\n");
		}
		CHECK(ok);
	}
}

static void test_column(void)
{
	ctui_layout l = {.dir = CTUI_LAYOUT_COLUMN, .spacing = 1};
	ctui_rect area = {2, 5, 21, 30}, out[3];
	ctui_constraint cons[] = {ctui_con_len(3), ctui_con_fill(1),
				  ctui_con_pct(50)};
	CHECK(ctui_layout_split(&l, &area, cons, 3, out) == 3);
	/* 21 - 2 gaps = 19: 3, then 50% = 9 (of 19), fill gets 7 */
	CHECK(out[0].y == 2 && out[0].height == 3 && out[0].x == 5 &&
	      out[0].width == 30);
	CHECK(out[1].y == 6 && out[1].height == 7 && out[1].width == 30);
	CHECK(out[2].y == 14 && out[2].height == 9);
}

static void test_margin(void)
{
	ctui_layout l = {.dir = CTUI_LAYOUT_ROW, .margin = {1, 2, 3, 4}};
	ctui_rect area = {10, 20, 10, 30}, out[2];
	ctui_constraint cons[] = {ctui_con_fill(1), ctui_con_fill(1)};
	CHECK(ctui_layout_split(&l, &area, cons, 2, out) == 2);
	CHECK(out[0].y == 11 && out[0].height == 6 && out[0].x == 24 &&
	      out[0].width == 12);
	CHECK(out[1].x == 36 && out[1].width == 12);
	/* a margin larger than the area leaves empty rectangles inside it */
	l.margin = ctui_insets_all(100);
	CHECK(ctui_layout_split(&l, &area, cons, 2, out) == 2);
	CHECK(out[0].width == 0 && out[0].height == 0 && out[1].width == 0);
	CHECK(out[0].x >= 20 && out[0].x <= 50 && out[0].y >= 10 &&
	      out[0].y <= 20);
}

static void test_nested(void)
{
	ctui_layout col = {.dir = CTUI_LAYOUT_COLUMN},
		    row = {.dir = CTUI_LAYOUT_ROW};
	ctui_rect area = {0, 0, 20, 40}, rows[2], cells[2];
	ctui_constraint a[] = {ctui_con_len(3), ctui_con_fill(1)},
			b[] = {ctui_con_pct(25), ctui_con_fill(1)};
	ctui_layout_split(&col, &area, a, 2, rows);
	ctui_layout_split(&row, &rows[1], b, 2, cells);
	CHECK(rows[1].y == 3 && rows[1].height == 17);
	CHECK(cells[0].y == 3 && cells[0].height == 17 && cells[0].width == 10);
	CHECK(cells[1].x == 10 && cells[1].width == 30);
}

static void test_args(void)
{
	ctui_layout l = {0};
	ctui_rect area = {0, 0, 5, 5}, out[1];
	ctui_constraint c = ctui_con_fill(1);
	CHECK(ctui_layout_split(NULL, &area, &c, 1, out) == 0);
	CHECK(ctui_layout_split(&l, NULL, &c, 1, out) == 0);
	CHECK(ctui_layout_split(&l, &area, &c, 1, NULL) == 0);
	CHECK(ctui_layout_split(&l, &area, NULL, 1, out) == 0);
	CHECK(ctui_layout_split(&l, &area, NULL, 0, out) == 0);
	ctui_rect neg = {0, 0, -3, -4};
	CHECK(ctui_layout_split(&l, &neg, &c, 1, out) == 1 &&
	      out[0].width == 0 && out[0].height == 0);
}

static void test_shrink(void)
{
	ctui_rect r = {5, 10, 8, 20};
	ctui_rect s = ctui_layout_shrink(r, (ctui_insets){1, 2, 3, 4});
	CHECK(s.y == 6 && s.x == 14 && s.height == 4 && s.width == 14);
	s = ctui_layout_shrink(r, ctui_insets_all(0));
	CHECK(s.y == 5 && s.x == 10 && s.height == 8 && s.width == 20);
	s = ctui_layout_shrink(r, ctui_insets_all(-4));
	CHECK(s.y == 5 && s.height == 8 && s.width == 20);
	s = ctui_layout_shrink(r, ctui_insets_all(12));
	CHECK(s.height == 0 && s.width == 0 && s.y >= 5 && s.y <= 13 &&
	      s.x >= 10 && s.x <= 30);
	s = ctui_layout_shrink(r, ctui_insets_all(INT_MAX));
	CHECK(s.height == 0 && s.width == 0);
	s = ctui_layout_shrink((ctui_rect){0, 0, INT_MAX, INT_MAX},
			       ctui_insets_all(1));
	CHECK(s.height == INT_MAX - 2 && s.width == INT_MAX - 2);
}

static void test_center(void)
{
	ctui_rect r = ctui_layout_center((ctui_rect){2, 4, 10, 30}, 4, 11);
	CHECK(r.y == 5 && r.x == 13 && r.height == 4 && r.width == 11);
	r = ctui_layout_center((ctui_rect){2, 4, 10, 30}, 40, 50);
	CHECK(r.y == 2 && r.x == 4 && r.height == 10 && r.width == 30);
	r = ctui_layout_center((ctui_rect){2, 4, 10, 30}, -1, -1);
	CHECK(r.height == 0 && r.width == 0);
	r = ctui_layout_center((ctui_rect){0, 0, 0, 0}, 3, 3);
	CHECK(r.height == 0 && r.width == 0);
	r = ctui_layout_center((ctui_rect){0, 0, INT_MAX, INT_MAX}, 1, 1);
	CHECK(r.y == INT_MAX / 2 && r.x == INT_MAX / 2);
}

int main(void)
{
	RUN(test_rows);
	RUN(test_column);
	RUN(test_margin);
	RUN(test_nested);
	RUN(test_args);
	RUN(test_shrink);
	RUN(test_center);
	return CHECK_DONE();
}
