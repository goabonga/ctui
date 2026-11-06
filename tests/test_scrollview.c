/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */
#include "check.h"

#include <curses.h>

#include "screen.h"
#include <ctui/scrollview.h>

static void test_layout(void)
{
	ctui_scrollview_layout l = ctui_scrollview_layout_for(10, 5, 20, 10);
	CHECK(!l.vbar && !l.hbar && l.width == 20 && l.height == 10);
	l = ctui_scrollview_layout_for(10, 50, 20, 10);
	CHECK(l.vbar && !l.hbar && l.width == 19 && l.height == 10);
	l = ctui_scrollview_layout_for(100, 5, 20, 10);
	CHECK(!l.vbar && l.hbar && l.width == 20 && l.height == 9);
	l = ctui_scrollview_layout_for(100, 50, 20, 10);
	CHECK(l.vbar && l.hbar && l.width == 19 && l.height == 9);
	/* the horizontal bar takes a row that makes the vertical one needed */
	l = ctui_scrollview_layout_for(30, 10, 20, 10);
	CHECK(l.vbar && l.hbar && l.width == 19 && l.height == 9);
	/* a vertical bar taking a column makes the horizontal one needed */
	l = ctui_scrollview_layout_for(20, 50, 20, 10);
	CHECK(l.vbar && l.hbar && l.width == 19 && l.height == 9);
	l = ctui_scrollview_layout_for(5, 5, 0, 0);
	CHECK(!l.vbar && !l.hbar && l.width == 0 && l.height == 0);
	l = ctui_scrollview_layout_for(5, 5, -3, 1);
	CHECK(l.width == 0 && l.height == 1 && !l.hbar);
}

struct trace {
	int calls, y, x, w, h;
	size_t oy, ox;
};

static void record(void *user, int y, int x, int w, int h, size_t oy, size_t ox)
{
	struct trace *t = user;
	t->calls++;
	t->y = y;
	t->x = x;
	t->w = w;
	t->h = h;
	t->oy = oy;
	t->ox = ox;
}

static void test_scroll(void)
{
	screen_open(24, 80);
	struct trace t = {0};
	ctui_scrollview v;
	ctui_scrollview_init(&v, 100, 50, record, &t);
	screen_clear();
	ctui_scrollview_draw(&v, 2, 3, 20, 10);
	CHECK(t.calls == 1 && t.y == 2 && t.x == 3 && t.w == 19 && t.h == 9 &&
	      t.oy == 0 && t.ox == 0);
	CHECK(ctui_scrollview_max_top(&v) == 41 &&
	      ctui_scrollview_max_left(&v) == 81);
	CHECK(ctui_scrollview_scroll_to(&v, 10, 20) && v.top == 10 &&
	      v.left == 20);
	CHECK(!ctui_scrollview_scroll_to(&v, 10, 20));
	CHECK(ctui_scrollview_scroll_to(&v, 999, 999) && v.top == 41 &&
	      v.left == 81);
	ctui_scrollview_draw(&v, 2, 3, 20, 10);
	CHECK(t.oy == 41 && t.ox == 81);
	CHECK(ctui_scrollview_scroll_by(&v, -100, -5) && v.top == 0 &&
	      v.left == 76);
	CHECK(!ctui_scrollview_scroll_by(&v, 0, 0));
	/* reveal moves the least */
	ctui_scrollview_scroll_to(&v, 0, 0);
	CHECK(!ctui_scrollview_reveal(&v, 3, 4));
	CHECK(ctui_scrollview_reveal(&v, 20, 30) && v.top == 12 &&
	      v.left == 12);
	CHECK(ctui_scrollview_reveal(&v, 5, 5) && v.top == 5 && v.left == 5);
	/* shrinking the area clamps the offsets */
	ctui_scrollview_scroll_to(&v, 41, 81);
	ctui_scrollview_resize(&v, 30, 12);
	CHECK(v.top == 3 && v.left == 11 && v.width == 30 && v.height == 12);
	ctui_scrollview_resize(&v, 5, 5);
	CHECK(v.top == 0 && v.left == 0);
}

static void test_keys(void)
{
	screen_open(24, 80);
	ctui_scrollview v;
	ctui_scrollview_init(&v, 100, 50, NULL, NULL);
	ctui_scrollview_draw(&v, 0, 0, 20, 10); /* content 19 x 9 */
	CHECK(!ctui_scrollview_key(&v, KEY_UP) &&
	      !ctui_scrollview_key(&v, KEY_LEFT));
	CHECK(ctui_scrollview_key(&v, KEY_DOWN) && v.top == 1);
	CHECK(ctui_scrollview_key(&v, KEY_RIGHT) && v.left == 1);
	CHECK(ctui_scrollview_key(&v, KEY_NPAGE) && v.top == 10);
	CHECK(ctui_scrollview_key(&v, KEY_SRIGHT) && v.left == 20);
	CHECK(ctui_scrollview_key(&v, KEY_SLEFT) && v.left == 1);
	CHECK(ctui_scrollview_key(&v, KEY_PPAGE) && v.top == 1);
	CHECK(ctui_scrollview_key(&v, KEY_END) && v.top == 41);
	CHECK(!ctui_scrollview_key(&v, KEY_DOWN));
	CHECK(ctui_scrollview_key(&v, KEY_HOME) && v.top == 0);
	CHECK(!ctui_scrollview_key(&v, 'x'));
}

static void test_hit(void)
{
	screen_open(24, 80);
	ctui_scrollview v;
	ctui_scrollview_init(&v, 100, 50, NULL, NULL);
	ctui_scrollview_draw(
	    &v, 2, 3, 20, 10); /* content 19 x 9, bars at column 22, row 11 */
	CHECK(ctui_scrollview_hit(&v, 2, 3) == CTUI_SCROLLVIEW_CONTENT);
	CHECK(ctui_scrollview_hit(&v, 10, 21) == CTUI_SCROLLVIEW_CONTENT);
	CHECK(ctui_scrollview_hit(&v, 5, 22) == CTUI_SCROLLVIEW_VBAR);
	CHECK(ctui_scrollview_hit(&v, 11, 5) == CTUI_SCROLLVIEW_HBAR);
	CHECK(ctui_scrollview_hit(&v, 11, 22) == CTUI_SCROLLVIEW_CORNER);
	CHECK(ctui_scrollview_hit(&v, 1, 3) == CTUI_SCROLLVIEW_NONE);
	CHECK(ctui_scrollview_hit(&v, 2, 23) == CTUI_SCROLLVIEW_NONE);
	CHECK(ctui_scrollview_hit(&v, 12, 3) == CTUI_SCROLLVIEW_NONE);
	ctui_scrollview_scroll_to(&v, 7, 9);
	size_t row = 0, col = 0;
	CHECK(ctui_scrollview_cell_at(&v, 4, 5, &row, &col) && row == 9 &&
	      col == 11);
	CHECK(!ctui_scrollview_cell_at(&v, 5, 22, &row, &col));
	ctui_scrollview_scroll_to(&v, 999, 999);
	ctui_scrollview_resize(&v, 85, 45);
	ctui_scrollview_draw(&v, 2, 3, 20, 10);
	CHECK(!ctui_scrollview_cell_at(&v, 20, 5, &row, &col));
	/* a small area: no bars, cells past the area are not cells */
	ctui_scrollview_resize(&v, 4, 3);
	ctui_scrollview_draw(&v, 0, 0, 20, 10);
	CHECK(ctui_scrollview_hit(&v, 9, 19) == CTUI_SCROLLVIEW_CONTENT);
	CHECK(ctui_scrollview_cell_at(&v, 2, 3, &row, &col) && row == 2 &&
	      col == 3);
	CHECK(!ctui_scrollview_cell_at(&v, 3, 3, &row, &col) &&
	      !ctui_scrollview_cell_at(&v, 2, 4, &row, &col));
}

static void test_mouse(void)
{
	screen_open(24, 80);
	ctui_scrollview v;
	ctui_scrollview_init(&v, 100, 50, NULL, NULL);
	ctui_scrollview_draw(
	    &v, 0, 0, 20,
	    10); /* vbar column 19 rows 0..8, hbar row 9 columns 0..18 */
	CHECK(!ctui_scrollview_press(&v, 3, 3)); /* content */
	CHECK(ctui_scrollview_press(&v, 8, 19) &&
	      v.top == 9); /* the track under the thumb: a page */
	CHECK(ctui_scrollview_press(&v, 9, 18) && v.left == 19);
	ctui_scrollview_draw(&v, 0, 0, 20, 10);
	int start, length;
	ctui_scrollbar_thumb(v.vbar.total, v.vbar.visible, v.vbar.offset,
			     v.vbar.height, &start, &length);
	CHECK(ctui_scrollview_press(&v, start, 19) && v.vbar.dragging);
	CHECK(ctui_scrollview_drag(&v, 8, 19) &&
	      v.top == ctui_scrollview_max_top(&v));
	CHECK(ctui_scrollview_release(&v) && !ctui_scrollview_release(&v));
	CHECK(!ctui_scrollview_drag(&v, 8, 19));
	ctui_scrollview_draw(&v, 0, 0, 20, 10);
	ctui_scrollbar_thumb(v.hbar.total, v.hbar.visible, v.hbar.offset,
			     v.hbar.height, &start, &length);
	CHECK(ctui_scrollview_press(&v, 9, start) && v.hbar.dragging);
	CHECK(ctui_scrollview_drag(&v, 9, 0) && v.left == 0);
	ctui_scrollview_release(&v);
	CHECK(ctui_scrollview_wheel(&v, 3, 3, 3, 0) &&
	      v.top == ctui_scrollview_max_top(&v));
	ctui_scrollview_scroll_to(&v, 10, 10);
	CHECK(ctui_scrollview_wheel(&v, 3, 3, -3, 2) && v.top == 7 &&
	      v.left == 12);
	CHECK(!ctui_scrollview_wheel(&v, 20, 3, 3, 0) && v.top == 7);
}

static void test_draw(void)
{
	screen_open(24, 80);
	ctui_scrollview v;
	ctui_scrollview_init(&v, 100, 50, NULL, NULL);
	screen_clear();
	ctui_scrollview_draw(&v, 0, 0, 20, 10);
	CHECK(screen_has_color(
	    ctui_attr(CTUI_PAIR_BORDER_SELECTED, true))); /* a thumb */
	/* a style names the thumb glyph of this instance */
	ctui_style own = {0};
	own.glyphs.scroll_thumb = "#";
	v.style = &own;
	screen_clear();
	ctui_scrollview_draw(&v, 0, 0, 20, 10);
	CHECK(screen_has_text("#"));
	/* nothing to scroll: no bar, nothing taken */
	ctui_scrollview_resize(&v, 3, 3);
	v.style = NULL;
	screen_clear();
	ctui_scrollview_draw(&v, 0, 0, 20, 10);
	CHECK(!ctui_scrollview_press(&v, 5, 19) && v.layout.width == 20 &&
	      !v.layout.vbar);
}

int main(void)
{
	RUN(test_layout);
	RUN(test_scroll);
	RUN(test_keys);
	RUN(test_hit);
	RUN(test_mouse);
	RUN(test_draw);
	return CHECK_DONE();
}
