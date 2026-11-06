/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * burger.c - burger button and dropdown menu (burger.h).
 */
#include <ctui/burger.h>

#include <curses.h>

#include <ctui/frame.h>
#include <ctui/text.h>
#include <ctui/theme.h>

#define PAGE_ROWS 5
#define MENU_MIN_WIDTH 14

void ctui_burger_init(ctui_burger *b, const ctui_list_item *items, size_t count)
{
	*b = (ctui_burger){0};
	ctui_list_init(&b->list, items, count, false);
}

void ctui_burger_set_submenus(ctui_burger *b, const ctui_menu_link *links,
			      size_t count)
{
	b->list.links = links;
	b->list.link_count = count;
	b->depth = 0;
}

static ctui_list *current(ctui_burger *b)
{
	return b->depth ? &b->sub[b->depth - 1] : &b->list;
}

const ctui_list *ctui_burger_level(const ctui_burger *b, size_t level)
{
	if (level > b->depth)
		return NULL;
	return level ? &b->sub[level - 1] : &b->list;
}

static void close_all(ctui_burger *b)
{
	b->open = false;
	b->depth = 0;
}

/* Open the submenu of the cursor item of the deepest level. */
static bool open_submenu(ctui_burger *b)
{
	ctui_list *cur = current(b);
	const ctui_menu_link *link = ctui_list_submenu(cur, cur->selected);
	if (!link || b->depth >= CTUI_BURGER_DEPTH - 1)
		return false;
	ctui_list *sub = &b->sub[b->depth];
	ctui_list_init(sub, link->items, link->count, false);
	sub->links = b->list.links;
	sub->link_count = b->list.link_count;
	sub->style = b->list.style;
	b->depth++;
	return true;
}

static ctui_burger_event choose(ctui_burger *b)
{
	ctui_list *cur = current(b);
	b->chosen = &cur->items[cur->selected];
	close_all(b);
	return CTUI_BURGER_ACTIVATED;
}

ctui_burger_event ctui_burger_key(ctui_burger *b, int key)
{
	if (!b->open) {
		if (key == '\n' || key == KEY_ENTER || key == ' ' ||
		    key == KEY_F(10)) {
			b->open = true;
			return CTUI_BURGER_OPENED;
		}
		return CTUI_BURGER_NONE;
	}
	if (key == KEY_F(10)) {
		close_all(b);
		return CTUI_BURGER_CLOSED;
	}
	if (key == 27 || key == KEY_LEFT) {
		if (b->depth) {
			b->depth--;
			return CTUI_BURGER_LEFT;
		}
		if (key == 27) {
			close_all(b);
			return CTUI_BURGER_CLOSED;
		}
		return CTUI_BURGER_NONE;
	}
	ctui_list *cur = current(b);
	if (key == KEY_RIGHT)
		return open_submenu(b) ? CTUI_BURGER_ENTERED : CTUI_BURGER_NONE;
	if (ctui_list_key(cur, key, PAGE_ROWS) == CTUI_LIST_ACTIVATED) {
		if (ctui_list_submenu(cur, cur->selected))
			return open_submenu(b) ? CTUI_BURGER_ENTERED
					       : CTUI_BURGER_NONE;
		return choose(b);
	}
	return CTUI_BURGER_NONE;
}

bool ctui_burger_geometry(const ctui_burger *b, int y, int x, int lines,
			  int cols, ctui_rect *box)
{
	int rows = lines - (y + 1) -
		   4; /* room under the button, minus the frame and padding */
	if ((int)b->list.count < rows)
		rows = (int)b->list.count;
	if (rows < 1)
		return false;
	int width = ctui_menu_width(&b->list, MENU_MIN_WIDTH) + 2;
	if (width > cols)
		width = cols;
	int left = x;
	if (left + width > cols)
		left = cols - width;
	if (left < 0)
		left = 0;
	*box = (ctui_rect){y + 1, left, rows + 4, width};
	return true;
}

bool ctui_menu_sub_geometry(const ctui_rect *parent, int parent_row, int width,
			    size_t count, int lines, int cols, ctui_rect *box)
{
	if (count < 1 || width < 1)
		return false;
	if (width > cols)
		width = cols;
	int height = (int)count + 4; /* frame and padding */
	int y = parent_row - 2; /* the first item lines up with parent_row */
	if (y + height > lines)
		y = lines - height;
	if (y < 0)
		y = 0;
	if (y + height > lines)
		height = lines - y;
	if (height < 5)
		return false;
	int x = parent->x + parent->width -
		1; /* shares the parent's border column */
	if (x + width > cols) {
		x = parent->x - width + 1;
		if (x < 0)
			x = cols - width;
	}
	*box = (ctui_rect){y, x, height, width};
	return true;
}

int ctui_menu_hit(const ctui_rect *box, size_t first, size_t count, int my,
		  int mx)
{
	int rows = box->height - 4;
	if (my <= box->y + 1 || my > box->y + 1 + rows || mx <= box->x ||
	    mx >= box->x + box->width - 1)
		return -1;
	size_t item = first + (size_t)(my - box->y - 2);
	return item < count ? (int)item : -1;
}

/* First item shown in a menu of rows rows: the cursor is always inside. */
static size_t first_item(const ctui_list *list, int rows)
{
	size_t r = (size_t)rows;
	return list->selected >= r ? list->selected - r + 1 : 0;
}

/* Boxes of levels 0..depth; returns how many fit. */
static size_t level_boxes(const ctui_burger *b, int y, int x, int lines,
			  int cols, ctui_rect boxes[])
{
	if (!ctui_burger_geometry(b, y, x, lines, cols, &boxes[0]))
		return 0;
	size_t n = 1;
	while (n <= b->depth) {
		const ctui_list *parent = n > 1 ? &b->sub[n - 2] : &b->list;
		const ctui_list *sub = &b->sub[n - 1];
		const ctui_rect *pb = &boxes[n - 1];
		int prow = pb->y + 2 +
			   (int)(parent->selected -
				 first_item(parent, pb->height - 4));
		int width = ctui_menu_width(sub, MENU_MIN_WIDTH) + 2;
		if (!ctui_menu_sub_geometry(pb, prow, width, sub->count, lines,
					    cols, &boxes[n]))
			break;
		n++;
	}
	return n;
}

bool ctui_burger_level_geometry(const ctui_burger *b, size_t level, int y,
				int x, int lines, int cols, ctui_rect *box)
{
	ctui_rect boxes[CTUI_BURGER_DEPTH];
	if (level >= level_boxes(b, y, x, lines, cols, boxes))
		return false;
	*box = boxes[level];
	return true;
}

static bool inside(const ctui_rect *box, int my, int mx)
{
	return my >= box->y && my < box->y + box->height && mx >= box->x &&
	       mx < box->x + box->width;
}

ctui_burger_event ctui_burger_click(ctui_burger *b, int y, int x, int lines,
				    int cols, int my, int mx)
{
	if (my == y && mx >= x && mx < x + CTUI_BURGER_WIDTH) {
		b->open = !b->open;
		b->depth = 0;
		return b->open ? CTUI_BURGER_OPENED : CTUI_BURGER_CLOSED;
	}
	if (!b->open)
		return CTUI_BURGER_NONE;
	ctui_rect boxes[CTUI_BURGER_DEPTH];
	size_t n = level_boxes(b, y, x, lines, cols, boxes);
	for (size_t level = n; level-- > 0;) {
		const ctui_list *list = ctui_burger_level(b, level);
		const ctui_rect *box = &boxes[level];
		int item = ctui_menu_hit(box, first_item(list, box->height - 4),
					 list->count, my, mx);
		if (item >= 0) {
			if (!ctui_list_selectable(list, (size_t)item))
				return CTUI_BURGER_NONE; /* separator or
							    disabled item */
			b->depth = level;
			ctui_list *cur = current(b);
			ctui_list_select(cur, (size_t)item);
			if (ctui_list_submenu(cur, cur->selected))
				return open_submenu(b) ? CTUI_BURGER_ENTERED
						       : CTUI_BURGER_NONE;
			return choose(b);
		}
		if (inside(box, my, mx))
			return CTUI_BURGER_NONE; /* the frame or a padding row
						  */
	}
	close_all(b);
	return CTUI_BURGER_CLOSED;
}

ctui_burger_event ctui_burger_hover(ctui_burger *b, int y, int x, int lines,
				    int cols, int my, int mx)
{
	if (!b->open)
		return CTUI_BURGER_NONE;
	ctui_rect boxes[CTUI_BURGER_DEPTH];
	size_t n = level_boxes(b, y, x, lines, cols, boxes);
	for (size_t level = n; level-- > 0;) {
		const ctui_list *list = ctui_burger_level(b, level);
		const ctui_rect *box = &boxes[level];
		int item = ctui_menu_hit(box, first_item(list, box->height - 4),
					 list->count, my, mx);
		if (item < 0)
			continue;
		if (!ctui_list_selectable(list, (size_t)item))
			return CTUI_BURGER_NONE;
		if (level < b->depth && list->selected == (size_t)item)
			return CTUI_BURGER_NONE; /* the item whose submenu is
						    open */
		bool shut = level < b->depth;
		b->depth = level;
		ctui_list *cur = current(b);
		ctui_list_select(cur, (size_t)item);
		if (ctui_list_submenu(cur, cur->selected) && open_submenu(b))
			return CTUI_BURGER_ENTERED;
		return shut ? CTUI_BURGER_LEFT : CTUI_BURGER_NONE;
	}
	return CTUI_BURGER_NONE;
}

static void draw_level(const ctui_list *list, const ctui_rect *box)
{
	ctui_frame_draw(box->y, box->x, box->height, box->width, true, true,
			NULL);
	int rows = box->height - 4;
	size_t first = first_item(list, rows);
	ctui_list view = *list;
	view.items += first;
	view.count -= first;
	view.selected -= first;
	if (view.count > (size_t)rows)
		view.count = (size_t)rows;
	ctui_menu_draw(&view, box->y + 1, box->x + 1, box->width - 2);
}

static void burger_draw_impl(const ctui_burger *b, int y, int x)
{
	const char *glyph = ctui_theme_glyphs()->burger;
	char label[8];
	int n = 0;
	label[n++] = ' ';
	for (const char *p = glyph; *p; p++)
		label[n++] = *p;
	label[n++] = ' ';
	label[n] = '\0';
	int attr =
	    ctui_attr_segment(b->open ? CTUI_TONE_INFO : CTUI_TONE_DARK, true);
	ctui_fill(y, x, CTUI_BURGER_WIDTH, attr);
	ctui_put(y, x, CTUI_BURGER_WIDTH, label, attr);

	ctui_rect boxes[CTUI_BURGER_DEPTH];
	if (!b->open)
		return;
	size_t levels = level_boxes(b, y, x, LINES, COLS, boxes);
	for (size_t level = 0; level < levels; level++)
		draw_level(ctui_burger_level(b, level), &boxes[level]);
}

void ctui_burger_draw(const ctui_burger *b, int y, int x)
{
	ctui_style_begin(b->style);
	burger_draw_impl(b, y, x);
	ctui_style_end();
}
