/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * scrollview.c - layout, scrolling, pointer handling and drawing
 * (scrollview.h).
 */
#include <ctui/scrollview.h>

#include <curses.h>

#include <ctui/text.h>
#include <ctui/theme.h>

void ctui_scrollview_init(ctui_scrollview *v, size_t width, size_t height,
			  ctui_scrollview_draw_fn draw, void *user)
{
	*v = (ctui_scrollview){
	    .width = width, .height = height, .draw = draw, .user = user};
	v->layout.width = 1;
	v->layout.height = 1;
}

ctui_scrollview_layout ctui_scrollview_layout_for(size_t width, size_t height,
						  int area_width,
						  int area_height)
{
	ctui_scrollview_layout l = {0};
	if (area_width < 0)
		area_width = 0;
	if (area_height < 0)
		area_height = 0;
	l.width = area_width;
	l.height = area_height;
	/* A bar takes room, which may make the other one necessary. */
	for (int pass = 0; pass < 2; pass++) {
		if (!l.vbar && height > (size_t)l.height && area_width > 1) {
			l.vbar = true;
			l.width = area_width - 1;
		}
		if (!l.hbar && width > (size_t)l.width && area_height > 1) {
			l.hbar = true;
			l.height = area_height - 1;
		}
	}
	return l;
}

static size_t max_of(size_t total, int visible)
{
	return visible > 0 && total > (size_t)visible ? total - (size_t)visible
						      : 0;
}

size_t ctui_scrollview_max_top(const ctui_scrollview *v)
{
	return max_of(v->height, v->layout.height);
}

size_t ctui_scrollview_max_left(const ctui_scrollview *v)
{
	return max_of(v->width, v->layout.width);
}

static void clamp(ctui_scrollview *v)
{
	if (v->top > ctui_scrollview_max_top(v))
		v->top = ctui_scrollview_max_top(v);
	if (v->left > ctui_scrollview_max_left(v))
		v->left = ctui_scrollview_max_left(v);
}

void ctui_scrollview_resize(ctui_scrollview *v, size_t width, size_t height)
{
	v->width = width;
	v->height = height;
	if (v->area_width > 0 || v->area_height > 0) {
		v->layout = ctui_scrollview_layout_for(
		    width, height, v->area_width, v->area_height);
		if (v->layout.width < 1)
			v->layout.width = 1;
		if (v->layout.height < 1)
			v->layout.height = 1;
	}
	clamp(v);
}

bool ctui_scrollview_scroll_to(ctui_scrollview *v, size_t top, size_t left)
{
	size_t t = v->top, l = v->left;
	v->top = top;
	v->left = left;
	clamp(v);
	return v->top != t || v->left != l;
}

static size_t add(size_t base, long delta)
{
	if (delta < 0)
		return base > (size_t)-delta ? base - (size_t)-delta : 0;
	return base + (size_t)delta < base ? base : base + (size_t)delta;
}

bool ctui_scrollview_scroll_by(ctui_scrollview *v, long rows, long columns)
{
	return ctui_scrollview_scroll_to(v, add(v->top, rows),
					 add(v->left, columns));
}

bool ctui_scrollview_reveal(ctui_scrollview *v, size_t row, size_t column)
{
	size_t top = v->top, left = v->left;
	size_t h = (size_t)v->layout.height, w = (size_t)v->layout.width;
	if (row < top)
		top = row;
	else if (row >= top + h)
		top = row - h + 1;
	if (column < left)
		left = column;
	else if (column >= left + w)
		left = column - w + 1;
	return ctui_scrollview_scroll_to(v, top, left);
}

bool ctui_scrollview_key(ctui_scrollview *v, int key)
{
	long page_rows = v->layout.height > 1 ? v->layout.height : 1;
	long page_cols = v->layout.width > 1 ? v->layout.width : 1;
	switch (key) {
	case KEY_UP:
		return ctui_scrollview_scroll_by(v, -1, 0);
	case KEY_DOWN:
		return ctui_scrollview_scroll_by(v, 1, 0);
	case KEY_LEFT:
		return ctui_scrollview_scroll_by(v, 0, -1);
	case KEY_RIGHT:
		return ctui_scrollview_scroll_by(v, 0, 1);
	case KEY_PPAGE:
		return ctui_scrollview_scroll_by(v, -page_rows, 0);
	case KEY_NPAGE:
		return ctui_scrollview_scroll_by(v, page_rows, 0);
	case KEY_SLEFT:
		return ctui_scrollview_scroll_by(v, 0, -page_cols);
	case KEY_SRIGHT:
		return ctui_scrollview_scroll_by(v, 0, page_cols);
	case KEY_HOME:
		return ctui_scrollview_scroll_to(v, 0, v->left);
	case KEY_END:
		return ctui_scrollview_scroll_to(v, ctui_scrollview_max_top(v),
						 v->left);
	default:
		return false;
	}
}

ctui_scrollview_part ctui_scrollview_hit(const ctui_scrollview *v, int y, int x)
{
	const ctui_scrollview_layout *l = &v->layout;
	if (y < v->y || x < v->x || y >= v->y + v->area_height ||
	    x >= v->x + v->area_width)
		return CTUI_SCROLLVIEW_NONE;
	bool right = l->vbar && x == v->x + l->width,
	     below = l->hbar && y == v->y + l->height;
	if (right && below)
		return CTUI_SCROLLVIEW_CORNER;
	if (right)
		return CTUI_SCROLLVIEW_VBAR;
	if (below)
		return CTUI_SCROLLVIEW_HBAR;
	return CTUI_SCROLLVIEW_CONTENT;
}

bool ctui_scrollview_cell_at(const ctui_scrollview *v, int y, int x,
			     size_t *row, size_t *column)
{
	if (ctui_scrollview_hit(v, y, x) != CTUI_SCROLLVIEW_CONTENT)
		return false;
	size_t r = v->top + (size_t)(y - v->y),
	       c = v->left + (size_t)(x - v->x);
	if (r >= v->height || c >= v->width)
		return false;
	if (row)
		*row = r;
	if (column)
		*column = c;
	return true;
}

static void apply_bars(ctui_scrollview *v)
{
	v->top = v->vbar.offset;
	v->left = v->hbar.offset;
	clamp(v);
}

bool ctui_scrollview_press(ctui_scrollview *v, int y, int x)
{
	if (ctui_scrollbar_press(&v->vbar, y, x) ||
	    ctui_scrollbar_press(&v->hbar, y, x)) {
		apply_bars(v);
		return true;
	}
	return false;
}

bool ctui_scrollview_drag(ctui_scrollview *v, int y, int x)
{
	if (v->vbar.dragging) {
		if (ctui_scrollbar_drag(&v->vbar, y))
			apply_bars(v);
		return true;
	}
	if (v->hbar.dragging) {
		if (ctui_scrollbar_drag(&v->hbar, x))
			apply_bars(v);
		return true;
	}
	return false;
}

bool ctui_scrollview_release(ctui_scrollview *v)
{
	bool a = ctui_scrollbar_release(&v->vbar),
	     b = ctui_scrollbar_release(&v->hbar);
	return a || b;
}

bool ctui_scrollview_wheel(ctui_scrollview *v, int y, int x, int rows,
			   int columns)
{
	if (ctui_scrollview_hit(v, y, x) == CTUI_SCROLLVIEW_NONE)
		return false;
	ctui_scrollview_scroll_by(v, rows, columns);
	return true;
}

static void scrollview_draw_impl(ctui_scrollview *v, int y, int x, int width,
				 int height)
{
	if (width < 0)
		width = 0;
	if (height < 0)
		height = 0;
	v->y = y;
	v->x = x;
	v->area_width = width;
	v->area_height = height;
	v->layout =
	    ctui_scrollview_layout_for(v->width, v->height, width, height);
	ctui_scrollview_layout *l = &v->layout;
	clamp(v);
	int body = ctui_attr(CTUI_PAIR_BODY, false);
	for (int r = 0; r < height; r++)
		ctui_fill(y + r, x, width, body);
	if (l->width > 0 && l->height > 0 && v->draw)
		v->draw(v->user, y, x, l->width, l->height, v->top, v->left);
	ctui_scrollbar_set(&v->vbar, v->height, (size_t)l->height, v->top);
	ctui_scrollbar_place(&v->vbar, y, x + l->width, l->height);
	ctui_scrollbar_set(&v->hbar, v->width, (size_t)l->width, v->left);
	ctui_scrollbar_place_horizontal(&v->hbar, y + l->height, x, l->width);
	if (l->vbar)
		ctui_scrollbar_draw(&v->vbar, false);
	if (l->hbar)
		ctui_scrollbar_draw(&v->hbar, false);
	/* Page sizes for the keys must not be zero. */
	if (l->width < 1)
		l->width = 1;
	if (l->height < 1)
		l->height = 1;
}

void ctui_scrollview_draw(ctui_scrollview *v, int y, int x, int width,
			  int height)
{
	ctui_style_begin(v->style);
	scrollview_draw_impl(v, y, x, width, height);
	ctui_style_end();
}
