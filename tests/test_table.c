/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */
#include "check.h"
#include "screen.h"

#include <curses.h>
#include <stdlib.h>

#include <ctui/table.h>
#include <ctui/theme.h>

static const ctui_table_column TCOLS[] = {
    {"Name", 0, 2, CTUI_TABLE_LEFT, false},
    {"Qty", 6, 0, CTUI_TABLE_RIGHT, true},
    {"Tag", 0, 1, CTUI_TABLE_CENTER, false},
};
#define NC (sizeof TCOLS / sizeof TCOLS[0])

static const char *NAMES[] = {"pear", "apple", "fig",  "kiwi",
			      "plum", "date",  "lime", "nut"};
static const char *QTYS[] = {"10", "9", "100", "-3", "x", "9", "2.5", "100"};
#define NR (sizeof NAMES / sizeof NAMES[0])

static const char *cell(void *ctx, size_t row, size_t col)
{
	(void)ctx;
	if (col == 0)
		return NAMES[row];
	if (col == 1)
		return QTYS[row];
	return "t";
}

static void test_layout(void)
{
	ctui_table_span s[NC];
	CHECK(ctui_table_layout(TCOLS, NC, 40, s) == 3);
	/* 40 - 2 separators - 6 fixed = 32 shared 2:1 -> 21 and 11 */
	CHECK(s[0].x == 0 && s[0].width == 21);
	CHECK(s[1].x == 22 && s[1].width == 6);
	CHECK(s[2].x == 29 && s[2].width == 11);
	CHECK(s[2].x + s[2].width == 40);
	/* too narrow: columns are cut, then dropped */
	CHECK(ctui_table_layout(TCOLS, NC, 10, s) == 3);
	CHECK(s[0].width == 1 && s[1].x == 2 && s[1].width == 6 &&
	      s[2].x == 9 && s[2].width == 1);
	CHECK(ctui_table_layout(TCOLS, NC, 7, s) == 1);
	CHECK(s[0].width == 0 && s[1].x == 1 && s[1].width == 6 &&
	      s[2].width == 0);
	CHECK(ctui_table_layout(TCOLS, NC, 0, s) == 0);
}

static void test_keys(void)
{
	ctui_table t;
	ctui_table_init(&t, TCOLS, NC, NR, cell, NULL);
	CHECK(t.sort_col == -1 && t.selected == 0);
	CHECK(ctui_table_key(&t, KEY_DOWN, 5) == CTUI_TABLE_MOVED &&
	      t.selected == 1);
	CHECK(ctui_table_key(&t, 'j', 5) == CTUI_TABLE_MOVED &&
	      t.selected == 2);
	CHECK(ctui_table_key(&t, 'k', 5) == CTUI_TABLE_MOVED &&
	      t.selected == 1);
	CHECK(ctui_table_key(&t, KEY_UP, 5) == CTUI_TABLE_MOVED &&
	      ctui_table_key(&t, KEY_UP, 5) == CTUI_TABLE_NONE);
	CHECK(ctui_table_key(&t, KEY_NPAGE, 5) == CTUI_TABLE_MOVED &&
	      t.selected == 4);
	CHECK(t.first == 1); /* 4 body rows: 1..4 */
	CHECK(ctui_table_key(&t, KEY_END, 5) == CTUI_TABLE_MOVED &&
	      t.selected == NR - 1 && t.first == NR - 4);
	CHECK(ctui_table_key(&t, KEY_DOWN, 5) == CTUI_TABLE_NONE);
	CHECK(ctui_table_key(&t, KEY_PPAGE, 5) == CTUI_TABLE_MOVED &&
	      t.selected == NR - 5);
	CHECK(ctui_table_key(&t, KEY_HOME, 5) == CTUI_TABLE_MOVED &&
	      t.selected == 0 && t.first == 0);
	CHECK(ctui_table_key(&t, '\n', 5) == CTUI_TABLE_ACTIVATED);
	CHECK(ctui_table_key(&t, 'z', 5) == CTUI_TABLE_NONE);
	ctui_table_set_rows(&t, 0);
	CHECK(ctui_table_key(&t, KEY_DOWN, 5) == CTUI_TABLE_NONE);
}

static void test_hit_and_click(void)
{
	ctui_table t;
	ctui_table_init(&t, TCOLS, NC, NR, cell, NULL);
	size_t row = 99, col = 99;
	/* 5 rows high: 4 body rows, 8 rows -> scrollbar in the last column */
	CHECK(ctui_table_has_scrollbar(&t, 40, 5));
	CHECK(!ctui_table_has_scrollbar(&t, 40, 12));
	CHECK(ctui_table_hit(&t, 40, 5, 0, 0, &row, &col) ==
		  CTUI_TABLE_HEADER &&
	      col == 0);
	CHECK(ctui_table_hit(&t, 40, 12, 0, 30, &row, &col) ==
		  CTUI_TABLE_HEADER &&
	      col == 2);
	CHECK(ctui_table_hit(&t, 40, 12, 3, 22, &row, &col) ==
		  CTUI_TABLE_CELL &&
	      row == 2 && col == 1);
	/* the separator belongs to the column on its left */
	CHECK(ctui_table_hit(&t, 40, 12, 3, 21, &row, &col) ==
		  CTUI_TABLE_CELL &&
	      col == 0);
	CHECK(ctui_table_hit(&t, 40, 5, 2, 39, NULL, NULL) ==
	      CTUI_TABLE_SCROLLBAR);
	CHECK(ctui_table_hit(&t, 40, 12, 8, 3, NULL, NULL) == CTUI_TABLE_CELL);
	CHECK(ctui_table_hit(&t, 40, 12, 9, 3, NULL, NULL) ==
	      CTUI_TABLE_OUTSIDE); /* only 8 rows */
	CHECK(ctui_table_hit(&t, 40, 12, -1, 3, NULL, NULL) ==
	      CTUI_TABLE_OUTSIDE);
	CHECK(ctui_table_hit(&t, 40, 12, 3, 40, NULL, NULL) ==
	      CTUI_TABLE_OUTSIDE);

	CHECK(ctui_table_press(&t, 40, 12, 4, 2) == CTUI_TABLE_MOVED &&
	      t.selected == 3);
	CHECK(ctui_table_press(&t, 40, 12, 4, 2) == CTUI_TABLE_NONE);
	CHECK(ctui_table_press(&t, 40, 12, 0, 23) == CTUI_TABLE_SORTED);
	CHECK(t.sort_col == 1 && t.sort_asc);
	CHECK(ctui_table_press(&t, 40, 12, 0, 23) == CTUI_TABLE_SORTED &&
	      t.sort_col == 1 && !t.sort_asc);
	CHECK(ctui_table_press(&t, 40, 12, 0, 2) == CTUI_TABLE_SORTED &&
	      t.sort_col == 0 && t.sort_asc);
}

static void test_scrollbar_and_wheel(void)
{
	ctui_table t;
	ctui_table_init(&t, TCOLS, NC, NR, cell, NULL);
	CHECK(ctui_table_wheel(&t, 5, 3) && t.first == 3);
	CHECK(ctui_table_wheel(&t, 5, 3) && t.first == NR - 4);
	CHECK(!ctui_table_wheel(&t, 5, 3));
	CHECK(ctui_table_wheel(&t, 5, -20) && t.first == 0 &&
	      !ctui_table_wheel(&t, 5, -1));
	CHECK(t.selected == 0); /* the wheel leaves the selection alone */
	/* press the track below the thumb: a page down */
	CHECK(ctui_table_press(&t, 40, 5, 4, 39) == CTUI_TABLE_SCROLLED &&
	      t.first == 4);
	/* grab the thumb, drag to the top, release */
	CHECK(ctui_table_press(&t, 40, 5, 4, 39) == CTUI_TABLE_SCROLLED &&
	      t.bar.dragging);
	CHECK(ctui_table_drag(&t, 40, 5, 1) && t.first == 0);
	CHECK(ctui_table_release(&t) && !ctui_table_drag(&t, 40, 5, 4));
}

static size_t *sorted(const ctui_table *t, size_t col, bool asc)
{
	size_t *p = ctui_table_sort_indices(t, col, asc, NULL, NULL);
	CHECK(p != NULL);
	return p;
}

static int by_length(void *ctx, size_t a, size_t b)
{
	(void)ctx;
	return (int)strlen(NAMES[a]) - (int)strlen(NAMES[b]);
}

static void test_sort(void)
{
	ctui_table t;
	ctui_table_init(&t, TCOLS, NC, NR, cell, NULL);
	size_t *p = sorted(&t, 0, true); /* text */
	CHECK(p && p[0] == 1 && p[1] == 5 && p[2] == 2 && p[7] == 4);
	free(p);
	/* numeric: "x" first, then -3 2.5 9 9 10 100 100; ties keep their order
	 */
	p = sorted(&t, 1, true);
	size_t up[] = {4, 3, 6, 1, 5, 0, 2, 7};
	for (size_t i = 0; p && i < NR; i++)
		CHECK(p[i] == up[i]);
	free(p);
	/* descending is stable too: equal values stay in row order */
	p = sorted(&t, 1, false);
	size_t down[] = {2, 7, 0, 1, 5, 6, 3, 4};
	for (size_t i = 0; p && i < NR; i++)
		CHECK(p[i] == down[i]);
	free(p);
	p = ctui_table_sort_indices(&t, 0, true, by_length, NULL);
	CHECK(p && p[0] == 2 && p[1] == 7 && p[2] == 0 &&
	      p[3] == 3); /* fig, nut, pear, kiwi: stable */
	free(p);
	CHECK(ctui_table_sort_indices(&t, 9, true, NULL, NULL) == NULL);
	ctui_table_set_rows(&t, 0);
	p = ctui_table_sort_indices(&t, 0, true, NULL, NULL);
	CHECK(p != NULL);
	free(p);
}

static const char *WIDE[] = {"\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e",
			     "a very long cell text", "\x1b[31mred"};

static const char *wide_cell(void *ctx, size_t row, size_t col)
{
	(void)ctx;
	(void)col;
	return WIDE[row];
}

static void test_draw(void)
{
	screen_open(24, 80);
	ctui_table t;
	ctui_table_init(&t, TCOLS, NC, NR, cell, NULL);
	screen_clear();
	ctui_table_draw(&t, 0, 0, 40, 12);
	CHECK(screen_has_text("Name") && screen_has_text("pear") &&
	      screen_has_text("nut"));
	CHECK(screen_has_color(ctui_attr(CTUI_PAIR_SELECTION, true)));
	CHECK(screen_has_text(ctui_theme_glyphs()->box_v));
	t.sort_col = 1;
	screen_clear();
	ctui_table_draw(&t, 0, 0, 40, 12);
	CHECK(screen_has_text("Qty \xe2\x96\xb2"));
	t.sort_asc = false;
	screen_clear();
	ctui_table_draw(&t, 0, 0, 40, 12);
	CHECK(screen_has_text("Qty \xe2\x96\xbc"));

	/* a short window scrolls and shows the scrollbar, not the hidden rows
	 */
	t.first = 0;
	screen_clear();
	ctui_table_draw(&t, 0, 0, 40, 4);
	CHECK(screen_has_text("pear") && !screen_has_text("kiwi"));
	CHECK(screen_has_text(ctui_theme_glyphs()->scroll_thumb));

	/* wide, long and hostile text: cut with the ellipsis, no escape reaches
	 * the screen */
	static const ctui_table_column one[] = {
	    {"T", 0, 1, CTUI_TABLE_RIGHT, false}};
	ctui_table w;
	ctui_table_init(&w, one, 1, 3, wide_cell, NULL);
	screen_clear();
	ctui_table_draw(&w, 0, 0, 10, 5);
	CHECK(screen_has_text("\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e"));
	CHECK(screen_has_text(ctui_theme_glyphs()->ellipsis));
	CHECK(screen_has_text("?[31mred") && !screen_has_text("\x1b"));
	endwin();
}

int main(void)
{
	RUN(test_layout);
	RUN(test_keys);
	RUN(test_hit_and_click);
	RUN(test_scrollbar_and_wheel);
	RUN(test_sort);
	RUN(test_draw);
	return CHECK_DONE();
}
