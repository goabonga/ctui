/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * list.c - selectable lists and popup menus (list.h).
 */
#include <ctui/list.h>

#include <curses.h>
#include <string.h>

#include <ctui/frame.h>
#include <ctui/text.h>
#include <ctui/theme.h>

bool ctui_list_selectable(const ctui_list *list, size_t i)
{
	return i < list->count && list->items[i].text &&
	       !list->items[i].disabled;
}

const ctui_menu_link *ctui_list_submenu(const ctui_list *list, size_t i)
{
	if (!ctui_list_selectable(list, i))
		return NULL;
	for (size_t k = 0; k < list->link_count; k++)
		if (list->links[k].item == &list->items[i] &&
		    list->links[k].items && list->links[k].count > 0)
			return &list->links[k];
	return NULL;
}

void ctui_list_init(ctui_list *list, const ctui_list_item *items, size_t count,
		    bool wrap)
{
	*list = (ctui_list){.items = items, .count = count, .wrap = wrap};
	for (size_t i = 0; i < count; i++)
		if (ctui_list_selectable(list, i)) {
			list->selected = i;
			break;
		}
}

bool ctui_list_select(ctui_list *list, size_t i)
{
	if (!ctui_list_selectable(list, i))
		return false;
	list->selected = i;
	return true;
}

/* Step from the cursor by direction (+1/-1) to the next selectable item. */
static bool step(ctui_list *list, int direction)
{
	size_t n = list->count;
	for (size_t k = 1; k <= n; k++) {
		size_t next;
		if (direction > 0) {
			if (!list->wrap && list->selected + k >= n)
				return false;
			next = (list->selected + k) % n;
		} else {
			if (!list->wrap && k > list->selected)
				return false;
			next = (list->selected + n - k % n) % n;
		}
		if (ctui_list_selectable(list, next)) {
			list->selected = next;
			return true;
		}
	}
	return false;
}

/* Nearest selectable item at or after i (direction > 0) or before it. */
static bool seek(ctui_list *list, size_t i, int direction)
{
	for (size_t k = 0; k < list->count; k++) {
		if (ctui_list_selectable(list, i)) {
			list->selected = i;
			return true;
		}
		if (direction > 0 ? ++i >= list->count : i-- == 0)
			return false;
	}
	return false;
}

ctui_list_event ctui_list_key(ctui_list *list, int key, size_t page)
{
	if (list->count == 0)
		return CTUI_LIST_NONE;
	size_t before = list->selected;
	switch (key) {
	case '\n':
	case KEY_ENTER:
	case ' ':
		return ctui_list_selectable(list, list->selected)
			   ? CTUI_LIST_ACTIVATED
			   : CTUI_LIST_NONE;
	case KEY_DOWN:
	case 'j':
		step(list, 1);
		break;
	case KEY_UP:
	case 'k':
		step(list, -1);
		break;
	case KEY_HOME:
		seek(list, 0, 1);
		break;
	case KEY_END:
		seek(list, list->count - 1, -1);
		break;
	case KEY_NPAGE: {
		size_t target = list->selected + (page ? page : 1);
		seek(list, target < list->count ? target : list->count - 1, -1);
		break;
	}
	case KEY_PPAGE: {
		size_t jump = page ? page : 1;
		seek(list, list->selected > jump ? list->selected - jump : 0,
		     1);
		break;
	}
	default:
		break;
	}
	return list->selected != before ? CTUI_LIST_MOVED : CTUI_LIST_NONE;
}

static bool has_detail(const ctui_list *list)
{
	for (size_t i = 0; i < list->count; i++)
		if (list->items[i].detail)
			return true;
	return false;
}

static int stride(const ctui_list *list, int height)
{
	return has_detail(list) && height >= 6 ? 2 : 1;
}

size_t ctui_list_visible(const ctui_list *list, int height)
{
	if (height < 1)
		return 1;
	return (size_t)(height / stride(list, height));
}

void ctui_list_scroll(ctui_list *list, int height)
{
	size_t visible = ctui_list_visible(list, height);
	if (list->selected < list->first)
		list->first = list->selected;
	else if (list->selected >= list->first + visible)
		list->first = list->selected - visible + 1;
}

void ctui_list_scroll_to(ctui_list *list, int height, size_t first)
{
	size_t visible = ctui_list_visible(list, height);
	size_t max = list->count > visible ? list->count - visible : 0;
	if (first > max)
		first = max;
	list->first = first;
	size_t last = first + visible < list->count ? first + visible - 1
						    : list->count - 1;
	if (!list->count || (list->selected >= first && list->selected <= last))
		return;
	bool above = list->selected < first;
	size_t from = above ? first : last;
	for (size_t k = 0; k < visible; k++) {
		size_t i = above ? from + k : from - k;
		if (i < list->count && i >= first && i <= last &&
		    ctui_list_selectable(list, i)) {
			list->selected = i;
			return;
		}
	}
}

int ctui_list_hit(const ctui_list *list, int height, int row)
{
	if (row < 0 || row >= height)
		return -1;
	size_t i = list->first + (size_t)(row / stride(list, height));
	return i < list->count ? (int)i : -1;
}

static void list_draw_impl(const ctui_list *list, int y, int x, int width,
			   int height)
{
	int s = stride(list, height);
	size_t visible = ctui_list_visible(list, height);
	for (size_t k = 0; k < visible && list->first + k < list->count; k++) {
		size_t i = list->first + k;
		const ctui_list_item *item = &list->items[i];
		int row = y + (int)k * s;
		bool selected = i == list->selected;
		int attr = selected ? ctui_attr(CTUI_PAIR_SELECTION, true)
				    : ctui_attr(CTUI_PAIR_BODY, false);
		if (!item->text) {
			ctui_separator(row, x + 2, width - 4,
				       ctui_attr(CTUI_PAIR_MUTED, false));
			continue;
		}
		if (item->disabled)
			attr = ctui_attr(CTUI_PAIR_MUTED, false);
		ctui_fill(row, x, width, attr);
		ctui_put(row, x + 2, width - 4, item->text, attr);
		if (s > 1 && item->detail)
			ctui_put(row + 1, x + 2, width - 4, item->detail,
				 ctui_attr(CTUI_PAIR_MUTED, false));
	}
}

void ctui_list_draw(const ctui_list *list, int y, int x, int width, int height)
{
	ctui_style_begin(list->style);
	list_draw_impl(list, y, x, width, height);
	ctui_style_end();
}

int ctui_menu_width(const ctui_list *list, int min_width)
{
	int width = min_width;
	for (size_t i = 0; i < list->count; i++) {
		if (!list->items[i].text)
			continue;
		int w = (int)ctui_utf8_columns(list->items[i].text) + 4;
		if (ctui_list_submenu(list, i))
			w += 2;
		if (w > width)
			width = w;
	}
	return width;
}

int ctui_menu_height(const ctui_list *list)
{
	return (int)list->count + 2;
}

static void menu_draw_impl(const ctui_list *list, int y, int x, int width)
{
	int dialog = ctui_attr(CTUI_PAIR_DIALOG, false);
	ctui_fill(y, x, width, dialog);
	ctui_fill(y + (int)list->count + 1, x, width, dialog);
	for (size_t i = 0; i < list->count; i++) {
		const ctui_list_item *item = &list->items[i];
		int row = y + 1 + (int)i;
		ctui_fill(row, x, width, dialog);
		if (!item->text) {
			ctui_separator(
			    row, x + 2, width - 4,
			    ctui_attr(CTUI_PAIR_DIALOG_BORDER, false));
			continue;
		}
		int attr = i == list->selected && !item->disabled
			       ? ctui_attr(CTUI_PAIR_SELECTION, true)
			   : item->disabled
			       ? (int)((unsigned)dialog | (unsigned)A_DIM)
			       : dialog;
		ctui_fill(row, x, width, attr);
		ctui_put(row, x + 2, width - 4, item->text, attr);
		if (ctui_list_submenu(list, i))
			ctui_put(row, x + width - 2, 1,
				 ctui_theme_glyphs()->submenu, attr);
	}
}

void ctui_menu_draw(const ctui_list *list, int y, int x, int width)
{
	ctui_style_begin(list->style);
	menu_draw_impl(list, y, x, width);
	ctui_style_end();
}
