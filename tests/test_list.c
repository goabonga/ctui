/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */
#include "check.h"
#include "screen.h"

#include <curses.h>

#include <ctui/list.h>

static const ctui_list_item ITEMS[] = {
    {"zero", NULL, true},   /* 0 disabled */
    {"one", NULL, false},   /* 1 */
    {NULL, NULL, false},    /* 2 separator */
    {"three", "d3", false}, /* 3 */
    {"four", NULL, true},   /* 4 disabled */
    {"five", NULL, false},  /* 5 */
};
#define N (sizeof ITEMS / sizeof ITEMS[0])

static void test_init_skips_disabled(void)
{
	ctui_list l;
	ctui_list_init(&l, ITEMS, N, false);
	CHECK(l.selected == 1);
	CHECK(!ctui_list_selectable(&l, 0) && !ctui_list_selectable(&l, 2) &&
	      !ctui_list_selectable(&l, 4));
	CHECK(ctui_list_selectable(&l, 5) && !ctui_list_selectable(&l, 99));
}

static void test_moves(void)
{
	ctui_list l;
	ctui_list_init(&l, ITEMS, N, false);
	CHECK(ctui_list_key(&l, KEY_DOWN, 3) == CTUI_LIST_MOVED &&
	      l.selected == 3);
	CHECK(ctui_list_key(&l, 'j', 3) == CTUI_LIST_MOVED && l.selected == 5);
	CHECK(ctui_list_key(&l, KEY_DOWN, 3) == CTUI_LIST_NONE &&
	      l.selected == 5);
	CHECK(ctui_list_key(&l, KEY_UP, 3) == CTUI_LIST_MOVED &&
	      l.selected == 3);
	CHECK(ctui_list_key(&l, 'k', 3) == CTUI_LIST_MOVED && l.selected == 1);
	CHECK(ctui_list_key(&l, KEY_UP, 3) == CTUI_LIST_NONE &&
	      l.selected == 1);
}

static void test_wrap(void)
{
	ctui_list l;
	ctui_list_init(&l, ITEMS, N, true);
	CHECK(ctui_list_key(&l, KEY_UP, 3) == CTUI_LIST_MOVED &&
	      l.selected == 5);
	CHECK(ctui_list_key(&l, KEY_DOWN, 3) == CTUI_LIST_MOVED &&
	      l.selected == 1);
}

static void test_home_end_pages(void)
{
	ctui_list l;
	ctui_list_init(&l, ITEMS, N, false);
	ctui_list_key(&l, KEY_END, 3);
	CHECK(l.selected == 5);
	ctui_list_key(&l, KEY_HOME, 3);
	CHECK(l.selected == 1);
	ctui_list_key(&l, KEY_NPAGE, 2);
	CHECK(l.selected == 3);
	ctui_list_key(&l, KEY_NPAGE, 20);
	CHECK(l.selected == 5);
	ctui_list_key(&l, KEY_PPAGE, 20);
	CHECK(l.selected == 1);
}

static void test_activation(void)
{
	ctui_list l;
	ctui_list_init(&l, ITEMS, N, false);
	CHECK(ctui_list_key(&l, '\n', 3) == CTUI_LIST_ACTIVATED);
	CHECK(ctui_list_key(&l, ' ', 3) == CTUI_LIST_ACTIVATED);
	CHECK(ctui_list_key(&l, 'x', 3) == CTUI_LIST_NONE);
	CHECK(ctui_list_select(&l, 3) && l.selected == 3);
	CHECK(!ctui_list_select(&l, 4) && l.selected == 3);
	ctui_list empty;
	ctui_list_init(&empty, ITEMS, 0, false);
	CHECK(ctui_list_key(&empty, '\n', 3) == CTUI_LIST_NONE);
}

static void test_visible_scroll_hit(void)
{
	ctui_list l;
	ctui_list_init(&l, ITEMS, N, false);
	CHECK(ctui_list_visible(&l, 3) == 3); /* too short for details */
	CHECK(ctui_list_visible(&l, 8) == 4); /* two rows per item */
	ctui_list_select(&l, 5);
	ctui_list_scroll(&l, 3);
	CHECK(l.first == 3);
	ctui_list_select(&l, 1);
	ctui_list_scroll(&l, 3);
	CHECK(l.first == 1);
	CHECK(ctui_list_hit(&l, 3, 0) == 1 && ctui_list_hit(&l, 3, 2) == 3);
	CHECK(ctui_list_hit(&l, 3, 3) == -1 && ctui_list_hit(&l, 3, -1) == -1);
	l.first = 0;
	CHECK(ctui_list_hit(&l, 8, 3) == 1 && ctui_list_hit(&l, 8, 6) == 3);
}

static void test_scroll_to(void)
{
	ctui_list l;
	ctui_list_init(&l, ITEMS, N, false);
	/* the cursor (1) is above the window: it moves to its first item */
	ctui_list_scroll_to(&l, 3, 3);
	CHECK(l.first == 3 && l.selected == 3);
	/* past the end the window stays full */
	ctui_list_scroll_to(&l, 3, 99);
	CHECK(l.first == 3);
	/* the cursor (3) stays when it is still inside */
	ctui_list_scroll_to(&l, 3, 2);
	CHECK(l.first == 2 && l.selected == 3);
	/* cursor below the window: nearest selectable item, skipping the
	 * separator */
	l.selected = 5;
	ctui_list_scroll_to(&l, 3, 0);
	CHECK(l.first == 0 && l.selected == 1);
	/* with details the window holds fewer items */
	ctui_list_scroll_to(&l, 8, 99);
	CHECK(l.first == 2);
}

static void test_menu_width(void)
{
	ctui_list l;
	ctui_list_init(&l, ITEMS, N, true);
	CHECK(ctui_menu_width(&l, 4) == 9); /* "three" + 4 */
	CHECK(ctui_menu_width(&l, 30) == 30);
	CHECK(ctui_menu_height(&l) ==
	      (int)l.count + 2); /* padding above and below */
}

static const ctui_list_item SUB[] = {{"Alpha", NULL, false},
				     {"Beta", NULL, false}};
static const ctui_list_item NESTED[] = {
    {"Plain", NULL, false},
    {"Export", NULL, false},
    {"Empty", NULL, false},
    {"Off", NULL, true},
};
static const ctui_menu_link LINKS[] = {
    {&NESTED[1], SUB, 2},
    {&NESTED[2], SUB, 0},
    {&NESTED[3], SUB, 2},
};

static void init_nested(ctui_list *l)
{
	ctui_list_init(l, NESTED, 4, false);
	l->links = LINKS;
	l->link_count = 3;
}

static void test_submenu_item(void)
{
	ctui_list l;
	init_nested(&l);
	CHECK(ctui_list_submenu(&l, 0) == NULL);
	const ctui_menu_link *link = ctui_list_submenu(&l, 1);
	CHECK(link && link->items == SUB && link->count == 2);
	CHECK(ctui_list_submenu(&l, 2) == NULL); /* no entries */
	CHECK(ctui_list_submenu(&l, 3) == NULL); /* disabled */
	CHECK(ctui_list_submenu(&l, 9) == NULL);
	/* room for the marker: "Export" + 4 + 2 */
	CHECK(ctui_menu_width(&l, 4) == 12);
	/* a list without links behaves as before */
	ctui_list plain;
	ctui_list_init(&plain, NESTED, 4, false);
	CHECK(ctui_list_submenu(&plain, 1) == NULL &&
	      ctui_menu_width(&plain, 4) == 10);
}

static void test_submenu_marker(void)
{
	screen_open(24, 80);
	ctui_list l;
	init_nested(&l);
	screen_clear();
	ctui_menu_draw(&l, 0, 0, 12);
	CHECK(screen_has_text(ctui_theme_glyphs_for(true)->submenu));
	ctui_list plain;
	ctui_list_init(&plain, ITEMS, N, false);
	screen_clear();
	ctui_menu_draw(&plain, 0, 0, 12);
	CHECK(!screen_has_text(ctui_theme_glyphs_for(true)->submenu));
}

int main(void)
{
	RUN(test_init_skips_disabled);
	RUN(test_moves);
	RUN(test_wrap);
	RUN(test_home_end_pages);
	RUN(test_activation);
	RUN(test_visible_scroll_hit);
	RUN(test_scroll_to);
	RUN(test_menu_width);
	RUN(test_submenu_item);
	RUN(test_submenu_marker);
	return CHECK_DONE();
}
