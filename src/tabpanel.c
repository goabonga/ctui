/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * tabpanel.c - tab strip inside a page or dialog (tabpanel.h).
 */
#include <ctui/tabpanel.h>

#include <curses.h>

#include <ctui/text.h>
#include <ctui/theme.h>

#define PADDING 2

void ctui_tabpanel_init(ctui_tabpanel *p, const char *const *labels,
			size_t count)
{
	*p = (ctui_tabpanel){.labels = labels, .count = count};
}

bool ctui_tabpanel_select(ctui_tabpanel *p, size_t i)
{
	if (i >= p->count)
		return false;
	p->selected = i;
	return true;
}

bool ctui_tabpanel_key(ctui_tabpanel *p, int key)
{
	if (p->count == 0)
		return false;
	size_t before = p->selected;
	switch (key) {
	case KEY_LEFT:
	case 'h':
		if (p->selected > 0)
			p->selected--;
		break;
	case KEY_RIGHT:
	case 'l':
		if (p->selected + 1 < p->count)
			p->selected++;
		break;
	case KEY_HOME:
		p->selected = 0;
		break;
	case KEY_END:
		p->selected = p->count - 1;
		break;
	default:
		break;
	}
	return p->selected != before;
}

int ctui_tabpanel_tab_width(const ctui_tabpanel *p, size_t i)
{
	return (int)ctui_utf8_columns(p->labels[i]) + 2 * PADDING;
}

int ctui_tabpanel_hit(const ctui_tabpanel *p, int width, int x)
{
	int left = 0;
	for (size_t i = 0; i < p->count; i++) {
		int w = ctui_tabpanel_tab_width(p, i);
		if (x >= left && x < left + w && x < width)
			return (int)i;
		left += w;
	}
	return -1;
}

static int tabpanel_draw_impl(const ctui_tabpanel *p, int y, int x, int width,
			      bool focused)
{
	const ctui_glyphs *g = ctui_theme_glyphs();
	int surface =
	    ctui_attr(p->dialog ? CTUI_PAIR_DIALOG : CTUI_PAIR_BODY, false);
	int muted = p->dialog ? (int)((unsigned)surface | (unsigned)A_DIM)
			      : ctui_attr(CTUI_PAIR_MUTED, false);
	int rule = ctui_attr(
	    p->dialog ? CTUI_PAIR_DIALOG_BORDER : CTUI_PAIR_BORDER, false);
	ctui_fill(y, x, width, surface);
	ctui_fill(y + 1, x, width,
		  p->dialog ? surface : ctui_attr(CTUI_PAIR_BODY, false));
	for (int i = 0; i < width; i++)
		ctui_put(y + 1, x + i, 1, g->rule, rule);

	int left = 0;
	for (size_t i = 0; i < p->count && left < width; i++) {
		int w = ctui_tabpanel_tab_width(p, i);
		bool current = i == p->selected;
		int attr = !current ? muted
			   : focused
			       ? ctui_attr(CTUI_PAIR_SELECTION, true)
			       : (int)((unsigned)surface | (unsigned)A_BOLD);
		int room = width - left < w ? width - left : w;
		ctui_fill(y, x + left, room, attr);
		ctui_put(y, x + left + PADDING, room - PADDING, p->labels[i],
			 attr);
		if (current)
			for (int k = 0; k < room; k++)
				ctui_put(y + 1, x + left + k, 1, g->heavy,
					 ctui_attr(CTUI_PAIR_CURSOR, true));
		left += w;
	}
	return CTUI_TABPANEL_ROWS;
}

int ctui_tabpanel_draw(const ctui_tabpanel *p, int y, int x, int width,
		       bool focused)
{
	ctui_style_begin(p->style);
	int result = tabpanel_draw_impl(p, y, x, width, focused);
	ctui_style_end();
	return result;
}
