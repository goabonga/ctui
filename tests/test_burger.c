/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * test_burger.c - burger state, geometry and mouse handling.
 */
#include <curses.h>

#include "check.h"
#include <ctui/burger.h>

static const ctui_list_item ITEMS[] = {
    {"New", NULL, false},   {"Open", NULL, false}, {NULL, NULL, false},
    {"Locked", NULL, true}, {"Quit", NULL, false},
};
#define COUNT (sizeof ITEMS / sizeof ITEMS[0])

static void test_keys(void)
{
	ctui_burger b;
	ctui_burger_init(&b, ITEMS, COUNT);
	CHECK(!b.open);
	CHECK(ctui_burger_key(&b, KEY_DOWN) == CTUI_BURGER_NONE);
	CHECK(ctui_burger_key(&b, '\n') == CTUI_BURGER_OPENED);
	CHECK(b.open);
	CHECK(ctui_burger_key(&b, KEY_DOWN) == CTUI_BURGER_NONE);
	CHECK(b.list.selected == 1);
	CHECK(ctui_burger_key(&b, 27) == CTUI_BURGER_CLOSED);
	CHECK(!b.open);
	CHECK(ctui_burger_key(&b, KEY_F(10)) == CTUI_BURGER_OPENED);
	CHECK(ctui_burger_key(&b, KEY_F(10)) == CTUI_BURGER_CLOSED);
}

static void test_activate(void)
{
	ctui_burger b;
	ctui_burger_init(&b, ITEMS, COUNT);
	ctui_burger_key(&b, ' ');
	ctui_burger_key(&b, KEY_END);
	CHECK(b.list.selected == 4);
	CHECK(ctui_burger_key(&b, '\n') == CTUI_BURGER_ACTIVATED);
	CHECK(!b.open);
	CHECK(b.list.selected == 4);
}

static void test_geometry(void)
{
	ctui_burger b;
	ctui_burger_init(&b, ITEMS, COUNT);
	ctui_rect box;
	CHECK(ctui_burger_geometry(&b, 0, 2, 24, 80, &box));
	CHECK(box.y == 1 && box.x == 2);
	CHECK(box.height == (int)COUNT + 4); /* frame and padding */
	CHECK(box.width == 16);

	/* it moves left to stay on screen */
	CHECK(ctui_burger_geometry(&b, 0, 70, 24, 80, &box));
	CHECK(box.x + box.width == 80);

	/* it shrinks to the rows left under the button */
	CHECK(ctui_burger_geometry(&b, 0, 0, 8, 80, &box));
	CHECK(box.height == 7);
	CHECK(box.y + box.height <= 8);

	/* and gives up when nothing fits */
	CHECK(!ctui_burger_geometry(&b, 0, 0, 5, 80, &box));
}

static void test_click(void)
{
	ctui_burger b;
	ctui_burger_init(&b, ITEMS, COUNT);
	CHECK(ctui_burger_click(&b, 0, 2, 24, 80, 5, 5) == CTUI_BURGER_NONE);
	CHECK(ctui_burger_click(&b, 0, 2, 24, 80, 0, 3) == CTUI_BURGER_OPENED);
	CHECK(b.open);
	/* row 1 is the frame, row 2 padding, rows 3.. hold New, Open,
	 * separator, Locked, Quit */
	CHECK(ctui_burger_click(&b, 0, 2, 24, 80, 1, 5) == CTUI_BURGER_NONE);
	CHECK(ctui_burger_click(&b, 0, 2, 24, 80, 2, 5) == CTUI_BURGER_NONE);
	CHECK(ctui_burger_click(&b, 0, 2, 24, 80, 5, 5) ==
	      CTUI_BURGER_NONE); /* separator */
	CHECK(b.open);
	CHECK(ctui_burger_click(&b, 0, 2, 24, 80, 6, 5) ==
	      CTUI_BURGER_NONE); /* disabled */
	CHECK(ctui_burger_click(&b, 0, 2, 24, 80, 8, 5) ==
	      CTUI_BURGER_NONE); /* bottom padding */
	CHECK(ctui_burger_click(&b, 0, 2, 24, 80, 4, 5) ==
	      CTUI_BURGER_ACTIVATED);
	CHECK(!b.open && b.list.selected == 1);

	ctui_burger_click(&b, 0, 2, 24, 80, 0, 2);
	CHECK(b.open);
	CHECK(ctui_burger_click(&b, 0, 2, 24, 80, 0, 2) == CTUI_BURGER_CLOSED);
	ctui_burger_click(&b, 0, 2, 24, 80, 0, 2);
	CHECK(ctui_burger_click(&b, 0, 2, 24, 80, 20, 60) ==
	      CTUI_BURGER_CLOSED);
	CHECK(!b.open);
}

static void test_scrolled_click(void)
{
	ctui_burger b;
	ctui_burger_init(&b, ITEMS, COUNT);
	ctui_burger_key(&b, '\n');
	ctui_burger_key(&b, KEY_END);
	/* 6 lines leave 1 row: the window ends on the cursor (Quit) */
	CHECK(ctui_burger_click(&b, 0, 0, 6, 80, 3, 2) ==
	      CTUI_BURGER_ACTIVATED);
	CHECK(b.list.selected == 4);
}

static const ctui_list_item SUB[] = {
    {"Recent", NULL, false},
    {"Last", NULL, false},
};
static const ctui_menu_link LINKS[] = {{&ITEMS[1], SUB, 2}};

static void nested(ctui_burger *b)
{
	ctui_burger_init(b, ITEMS, COUNT);
	ctui_burger_set_submenus(b, LINKS, 1);
}

static void test_sub_geometry(void)
{
	ctui_rect parent = {1, 20, 9, 16}, box;
	/* beside the parent, first item on the row of the parent item */
	CHECK(ctui_menu_sub_geometry(&parent, 4, 16, 2, 24, 80, &box));
	CHECK(box.y == 2 && box.x == 35 && box.height == 6 && box.width == 16);
	/* no room on the right: it opens on the left */
	CHECK(ctui_menu_sub_geometry(&parent, 4, 16, 2, 24, 40, &box));
	CHECK(box.x == 5 && box.x + box.width == parent.x + 1);
	/* room on neither side: kept on screen */
	ctui_rect wide = {1, 5, 9, 30};
	CHECK(ctui_menu_sub_geometry(&wide, 4, 20, 2, 24, 40, &box));
	CHECK(box.x >= 0 && box.x + box.width <= 40);
	/* it moves up to stay above the bottom */
	CHECK(ctui_menu_sub_geometry(&parent, 20, 16, 5, 24, 80, &box));
	CHECK(box.y + box.height == 24 && box.height == 9);
	/* it loses rows on a short screen, and gives up when nothing fits */
	CHECK(ctui_menu_sub_geometry(&parent, 4, 16, 5, 7, 80, &box));
	CHECK(box.y == 0 && box.height == 7);
	CHECK(!ctui_menu_sub_geometry(&parent, 4, 16, 5, 4, 80, &box));
	CHECK(!ctui_menu_sub_geometry(&parent, 4, 16, 0, 24, 80, &box));
}

static void test_menu_hit(void)
{
	ctui_rect box = {1, 2, 9, 16}; /* 5 item rows: 3..7 */
	CHECK(ctui_menu_hit(&box, 0, 5, 3, 5) == 0);
	CHECK(ctui_menu_hit(&box, 0, 5, 7, 5) == 4);
	CHECK(ctui_menu_hit(&box, 2, 5, 3, 5) == 2);  /* scrolled */
	CHECK(ctui_menu_hit(&box, 0, 5, 2, 5) == -1); /* padding */
	CHECK(ctui_menu_hit(&box, 0, 5, 8, 5) == -1);
	CHECK(ctui_menu_hit(&box, 0, 5, 1, 5) == -1); /* frame */
	CHECK(ctui_menu_hit(&box, 0, 5, 3, 2) == -1);
	CHECK(ctui_menu_hit(&box, 0, 5, 3, 17) == -1);
	CHECK(ctui_menu_hit(&box, 0, 3, 7, 5) == -1); /* past the last item */
}

static void test_sub_keys(void)
{
	ctui_burger b;
	nested(&b);
	ctui_burger_key(&b, '\n');
	CHECK(ctui_burger_key(&b, KEY_RIGHT) ==
	      CTUI_BURGER_NONE); /* "New" has none */
	CHECK(ctui_burger_key(&b, KEY_LEFT) == CTUI_BURGER_NONE);
	ctui_burger_key(&b, KEY_DOWN);
	CHECK(ctui_burger_key(&b, KEY_RIGHT) == CTUI_BURGER_ENTERED);
	CHECK(b.depth == 1 && b.open && ctui_burger_level(&b, 1) &&
	      !ctui_burger_level(&b, 2));
	CHECK(ctui_burger_key(&b, KEY_DOWN) == CTUI_BURGER_NONE);
	CHECK(ctui_burger_level(&b, 1)->selected == 1);
	CHECK(ctui_burger_key(&b, KEY_LEFT) == CTUI_BURGER_LEFT);
	CHECK(b.depth == 0 && b.open);
	CHECK(ctui_burger_key(&b, '\n') ==
	      CTUI_BURGER_ENTERED); /* Enter opens it too */
	CHECK(ctui_burger_key(&b, 27) ==
	      CTUI_BURGER_LEFT); /* Esc closes one level */
	CHECK(ctui_burger_key(&b, ' ') == CTUI_BURGER_ENTERED);
	CHECK(ctui_burger_key(&b, KEY_F(10)) ==
	      CTUI_BURGER_CLOSED); /* F10 closes all */
	CHECK(!b.open && b.depth == 0);

	ctui_burger_key(&b, '\n'); /* the cursor is still on "Open" */
	ctui_burger_key(&b, KEY_RIGHT);
	ctui_burger_key(&b, KEY_DOWN);
	CHECK(ctui_burger_key(&b, '\n') == CTUI_BURGER_ACTIVATED);
	CHECK(!b.open && b.depth == 0);
	CHECK(b.chosen == &SUB[1] && b.list.selected == 1);
	ctui_burger_key(&b, '\n');
	CHECK(b.depth == 0); /* reopens at the top level */
	ctui_burger_key(&b, 27);
	ctui_burger_key(&b, '\n');
	ctui_burger_key(&b, KEY_END);
	CHECK(ctui_burger_key(&b, '\n') == CTUI_BURGER_ACTIVATED &&
	      b.chosen == &ITEMS[4]);
}

static void test_sub_click(void)
{
	ctui_burger b;
	nested(&b);
	ctui_burger_click(&b, 0, 2, 24, 80, 0, 3);
	/* the root menu is at x 2..17, "Open" on row 4; its submenu at x 17,
	 * rows 4 and 5 */
	CHECK(ctui_burger_click(&b, 0, 2, 24, 80, 4, 5) == CTUI_BURGER_ENTERED);
	CHECK(b.open && b.depth == 1);
	ctui_rect box;
	CHECK(ctui_burger_level_geometry(&b, 1, 0, 2, 24, 80, &box));
	CHECK(box.x == 17 && box.y == 2 && box.height == 6);
	CHECK(!ctui_burger_level_geometry(&b, 2, 0, 2, 24, 80, &box));
	CHECK(ctui_burger_click(&b, 0, 2, 24, 80, 3, 20) ==
	      CTUI_BURGER_NONE); /* sub padding */
	CHECK(ctui_burger_click(&b, 0, 2, 24, 80, 2, 20) ==
	      CTUI_BURGER_NONE); /* sub frame */
	CHECK(b.open && b.depth == 1);
	CHECK(ctui_burger_click(&b, 0, 2, 24, 80, 5, 20) ==
	      CTUI_BURGER_ACTIVATED);
	CHECK(!b.open && b.depth == 0 && b.chosen == &SUB[1]);

	ctui_burger_click(&b, 0, 2, 24, 80, 0, 3);
	ctui_burger_click(&b, 0, 2, 24, 80, 4, 5);
	/* a click on another item of the parent menu chooses it and closes all
	 */
	CHECK(ctui_burger_click(&b, 0, 2, 24, 80, 3, 5) ==
	      CTUI_BURGER_ACTIVATED);
	CHECK(!b.open && b.chosen == &ITEMS[0]);

	ctui_burger_click(&b, 0, 2, 24, 80, 0, 3);
	ctui_burger_click(&b, 0, 2, 24, 80, 4, 5);
	CHECK(ctui_burger_click(&b, 0, 2, 24, 80, 20, 60) ==
	      CTUI_BURGER_CLOSED);
	CHECK(!b.open && b.depth == 0);
}

static void test_sub_hover(void)
{
	ctui_burger b;
	nested(&b);
	CHECK(ctui_burger_hover(&b, 0, 2, 24, 80, 4, 5) ==
	      CTUI_BURGER_NONE); /* closed */
	ctui_burger_click(&b, 0, 2, 24, 80, 0, 3);
	CHECK(ctui_burger_hover(&b, 0, 2, 24, 80, 4, 5) == CTUI_BURGER_ENTERED);
	CHECK(b.depth == 1 && b.list.selected == 1);
	CHECK(ctui_burger_hover(&b, 0, 2, 24, 80, 4, 5) ==
	      CTUI_BURGER_NONE); /* already open */
	CHECK(b.depth == 1);
	CHECK(ctui_burger_hover(&b, 0, 2, 24, 80, 5, 20) == CTUI_BURGER_NONE);
	CHECK(ctui_burger_level(&b, 1)->selected == 1); /* the cursor follows */
	CHECK(ctui_burger_hover(&b, 0, 2, 24, 80, 5, 5) ==
	      CTUI_BURGER_NONE); /* separator */
	CHECK(b.depth == 1);
	CHECK(ctui_burger_hover(&b, 0, 2, 24, 80, 3, 5) == CTUI_BURGER_LEFT);
	CHECK(b.depth == 0 && b.list.selected == 0 && b.open);
	CHECK(ctui_burger_hover(&b, 0, 2, 24, 80, 7, 5) == CTUI_BURGER_NONE);
	CHECK(b.list.selected == 4);
	CHECK(ctui_burger_hover(&b, 0, 2, 24, 80, 20, 60) ==
	      CTUI_BURGER_NONE); /* outside */
	CHECK(b.open);
}

static void test_sub_left_side(void)
{
	ctui_burger b;
	nested(&b);
	ctui_burger_click(&b, 0, 60, 24, 80, 0, 61);
	ctui_rect root, sub;
	CHECK(ctui_burger_geometry(&b, 0, 60, 24, 80, &root));
	CHECK(root.x == 60);
	CHECK(ctui_burger_click(&b, 0, 60, 24, 80, 4, 70) ==
	      CTUI_BURGER_ENTERED);
	CHECK(ctui_burger_level_geometry(&b, 1, 0, 60, 24, 80, &sub));
	CHECK(sub.x + sub.width == root.x + 1); /* on the left of the parent */
}

int main(void)
{
	RUN(test_keys);
	RUN(test_activate);
	RUN(test_geometry);
	RUN(test_click);
	RUN(test_scrolled_click);
	RUN(test_sub_geometry);
	RUN(test_menu_hit);
	RUN(test_sub_keys);
	RUN(test_sub_click);
	RUN(test_sub_hover);
	RUN(test_sub_left_side);
	return CHECK_DONE();
}
