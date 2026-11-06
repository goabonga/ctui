/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * burger.h - a burger button that opens a dropdown menu.
 *
 * The button is three columns wide (" = " in ASCII). The menu drops under
 * it, framed, and keeps inside the screen: it moves left when it would
 * overflow on the right and scrolls when it has fewer rows than items.
 * Items are those of a list (list.h); their details are not shown.
 *
 * Closed, Enter / Space / F10 open the menu. Open, Up/Down/Home/End move
 * the cursor, Enter / Space choose the item (index in list.selected) and
 * Esc / F10 close it. ctui_burger_click() does the same for the mouse.
 *
 * Submenus: ctui_burger_set_submenus() gives a table of ctui_menu_link (list.h)
 * and the items it names show a marker. Right, Enter or Space on such an
 * item open its submenu beside the menu (on the left when there is no
 * room on the right), Left or Esc close it again, F10 closes everything.
 * A click opens or chooses, and ctui_burger_hover() follows the pointer.
 * Submenus nest up to CTUI_BURGER_DEPTH levels (the menu itself included).
 */
#ifndef CTUI_BURGER_H
#define CTUI_BURGER_H

#include <stdbool.h>
#include <stddef.h>

#include <ctui/dialog.h>
#include <ctui/list.h>

#define CTUI_BURGER_WIDTH 3
#define CTUI_BURGER_DEPTH 4 /* menu levels, the dropdown itself included */

typedef struct ctui_burger {
	ctui_list list;
	bool open;
	const ctui_style *style; /* colours and glyphs of this instance, may be
				    NULL; set after init */
	ctui_list
	    sub[CTUI_BURGER_DEPTH - 1]; /* open submenus, outermost first */
	size_t depth;			/* number of open submenus */
	const ctui_list_item
	    *chosen; /* item of the last CTUI_BURGER_ACTIVATED */
} ctui_burger;

typedef enum ctui_burger_event {
	CTUI_BURGER_NONE = 0,
	CTUI_BURGER_OPENED,
	CTUI_BURGER_CLOSED,
	CTUI_BURGER_ACTIVATED, /* an item was chosen; the menu is closed again
				*/
	CTUI_BURGER_ENTERED,   /* a submenu was opened */
	CTUI_BURGER_LEFT       /* a submenu was closed, its parent stays open */
} ctui_burger_event;

void ctui_burger_init(ctui_burger *b, const ctui_list_item *items,
		      size_t count);

/* Submenus of the items: links[count], owned by the caller and kept alive
 * while the burger is used. Closes any open submenu. */
void ctui_burger_set_submenus(ctui_burger *b, const ctui_menu_link *links,
			      size_t count);

/* The list of menu level (0 is the dropdown, 1.. are open submenus), or
 * NULL beyond the deepest open one. */
const ctui_list *ctui_burger_level(const ctui_burger *b, size_t level);

ctui_burger_event ctui_burger_key(ctui_burger *b, int key);

/* Where the menu goes for a button at (y, x) on a lines x cols screen;
 * false when there is no room for at least one row. */
bool ctui_burger_geometry(const ctui_burger *b, int y, int x, int lines,
			  int cols, ctui_rect *box);

/* Where a submenu goes beside its parent menu box: parent_row is the
 * screen row of the item that opens it, width and count the width of the
 * submenu box and the number of its items. It opens on the right of the
 * parent, on the left when it does not fit there, is moved up to stay
 * above the bottom and loses rows when the screen is too short. False when
 * not even one row fits. Pure. */
bool ctui_menu_sub_geometry(const ctui_rect *parent, int parent_row, int width,
			    size_t count, int lines, int cols, ctui_rect *box);

/* Item under screen cell (my, mx) of a menu box whose first shown item is
 * first, or -1 for the frame, the padding rows and cells outside. Pure. */
int ctui_menu_hit(const ctui_rect *box, size_t first, size_t count, int my,
		  int mx);

/* Box of menu level (0 is the dropdown) for a button at (y, x); false when
 * the level is not open or does not fit. */
bool ctui_burger_level_geometry(const ctui_burger *b, size_t level, int y,
				int x, int lines, int cols, ctui_rect *box);

/* Mouse click at (my, mx) for a button at (y, x). A click on the button
 * toggles the menu, on an item chooses it, anywhere else closes the menu. On an
 * item with a submenu it opens the submenu (CTUI_BURGER_ENTERED); the item is
 * chosen otherwise, whatever the level (see chosen). */
ctui_burger_event ctui_burger_click(ctui_burger *b, int y, int x, int lines,
				    int cols, int my, int mx);

/* Pointer moved to (my, mx) with the menu open: the cursor follows it, an
 * item with a submenu opens it (CTUI_BURGER_ENTERED) and moving to another
 * item of a parent menu closes the deeper submenus (CTUI_BURGER_LEFT).
 * Nothing else changes; clicking is what chooses. */
ctui_burger_event ctui_burger_hover(ctui_burger *b, int y, int x, int lines,
				    int cols, int my, int mx);

/* Draw the button at (y, x) and, when open, the menu under it. */
void ctui_burger_draw(const ctui_burger *b, int y, int x);

#endif /* CTUI_BURGER_H */
