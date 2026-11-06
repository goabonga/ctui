/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * list.h - selectable lists and popup menus.
 *
 * A list owns only its cursor; the items stay with the caller. Separators
 * and disabled items are skipped by every movement. ctui_list_key() maps
 * the usual keys (Up/Down/j/k, Home/End, PgUp/PgDn, Enter/Space) onto the
 * cursor and tells what happened. ctui_list_draw() paints a plain list, with
 * an optional detail line under each item when there is room;
 * ctui_menu_draw() paints the same items as a popup menu on the dialog
 * background.
 */
#ifndef CTUI_LIST_H
#define CTUI_LIST_H

#include <stdbool.h>
#include <stddef.h>

#include <ctui/theme.h>

typedef struct ctui_list_item {
	const char *text;   /* NULL for a separator */
	const char *detail; /* second line, may be NULL */
	bool disabled;
} ctui_list_item;

/* A submenu of a popup menu: the entries opened from `item`, which is an
 * element of some item array (a root menu or another submenu). Links live
 * in a caller-owned table, so items keep their plain initialisers. */
typedef struct ctui_menu_link {
	const ctui_list_item *item;
	const ctui_list_item *items;
	size_t count;
} ctui_menu_link;

typedef struct ctui_list {
	const ctui_list_item *items;
	size_t count;
	size_t selected; /* meaningful when count > 0 */
	size_t first;	 /* first visible item */
	bool wrap;	 /* Up on the first item goes to the last, and back */
	const ctui_style *style;     /* colours and glyphs of this list, may be
					NULL; set after init */
	const ctui_menu_link *links; /* submenu table of a popup menu, may be
					NULL; set after init */
	size_t link_count;
} ctui_list;

typedef enum ctui_list_event {
	CTUI_LIST_NONE = 0,
	CTUI_LIST_MOVED,
	CTUI_LIST_ACTIVATED
} ctui_list_event;

/* Start on the first selectable item. */
void ctui_list_init(ctui_list *list, const ctui_list_item *items, size_t count,
		    bool wrap);

/* Whether items[i] can hold the cursor. */
bool ctui_list_selectable(const ctui_list *list, size_t i);

/* Apply a key; page is the number of items PgUp/PgDn jump. */
ctui_list_event ctui_list_key(ctui_list *list, int key, size_t page);

/* Move the cursor to item i when it is selectable. */
bool ctui_list_select(ctui_list *list, size_t i);

/* Number of items that fit in height rows (two rows per item when some
 * item has a detail and there is room for it). */
size_t ctui_list_visible(const ctui_list *list, int height);

/* Scroll so that the cursor is inside a window of height rows. */
void ctui_list_scroll(ctui_list *list, int height);

/* Show the window starting at item first (clamped so the window stays
 * full) and pull the cursor into it, onto the nearest selectable item. */
void ctui_list_scroll_to(ctui_list *list, int height, size_t first);

/* Item under screen row row of a list drawn from row 0 in height rows,
 * or -1. */
int ctui_list_hit(const ctui_list *list, int height, int row);

/* Draw items [first, first + visible) in a width x height area. */
void ctui_list_draw(const ctui_list *list, int y, int x, int width, int height);

/* The link of the submenu opened by items[i], or NULL when i is not a
 * selectable item with a non-empty submenu. */
const ctui_menu_link *ctui_list_submenu(const ctui_list *list, size_t i);

/* Width a popup menu needs for its items (room for the submenu marker
 * included), at least min_width. */
int ctui_menu_width(const ctui_list *list, int min_width);

/* Rows a popup menu takes: one per item and a padding row above and below. */
int ctui_menu_height(const ctui_list *list);

/* Draw the menu on the dialog background: a padding row, one row per item,
 * a padding row (ctui_menu_height() rows in all). Items with a submenu get
 * the theme's submenu marker at the right edge. */
void ctui_menu_draw(const ctui_list *list, int y, int x, int width);

#endif /* CTUI_LIST_H */
