/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * tabbar.c - the powerline tab bar (tabbar.h).
 */
#include <ctui/tabbar.h>

#include <curses.h>
#include <stdio.h>

#include <ctui/text.h>
#include <ctui/theme.h>

#define CTUI_TABS_MAX 64

static const char *tab_icon(const ctui_tab *tab, bool unicode)
{
	const char *icon = unicode ? tab->icon : tab->ascii_icon;
	return icon && *icon ? icon : NULL;
}

static int brand_width(const ctui_tabbar *bar)
{
	return bar->brand ? (int)ctui_utf8_columns(bar->brand) + 2 : 0;
}

static int body_width(const ctui_tab *tab, bool unicode)
{
	const char *icon = tab_icon(tab, unicode);
	int w =
	    2 + (int)ctui_utf8_columns(tab->label); /* a space on each side */
	if (icon)
		w += (int)ctui_utf8_columns(icon) + 1;
	if (tab->closable)
		w += 2; /* "x " */
	return w;
}

size_t ctui_tabbar_layout(const ctui_tabbar *bar, int columns, bool unicode,
			  ctui_tab_span *spans)
{
	int x = brand_width(bar);
	size_t fit = 0;
	for (size_t i = 0; i < bar->count; i++) {
		bool current = i == bar->current;
		bool prev_current = i > 0 && i - 1 == bar->current;
		int prefix = current ? 1 : (i > 0 && !prev_current ? 1 : 0);
		int body = body_width(&bar->tabs[i], unicode);
		int suffix = current ? 1 : 0;
		if (x + prefix + body + suffix > columns)
			break;
		spans[i].x = x;
		spans[i].width = prefix + body + suffix;
		spans[i].close_x =
		    bar->tabs[i].closable ? x + prefix + body - 2 : -1;
		x += prefix + body + suffix;
		fit++;
	}
	return fit;
}

int ctui_tabbar_hit(const ctui_tabbar *bar, int columns, bool unicode, int x,
		    bool *close)
{
	ctui_tab_span spans[CTUI_TABS_MAX];
	ctui_tabbar limited = *bar;
	if (limited.count > CTUI_TABS_MAX)
		limited.count = CTUI_TABS_MAX;
	size_t fit = ctui_tabbar_layout(&limited, columns, unicode, spans);
	if (close)
		*close = false;
	for (size_t i = 0; i < fit; i++) {
		if (x >= spans[i].x && x < spans[i].x + spans[i].width) {
			if (close)
				*close = spans[i].close_x >= 0 &&
					 x == spans[i].close_x;
			return (int)i;
		}
	}
	return -1;
}

void ctui_tabbar_draw(int y, const ctui_tabbar *bar)
{
	const ctui_glyphs *g = ctui_theme_glyphs();
	ctui_tab_span spans[CTUI_TABS_MAX];
	ctui_tabbar limited = *bar;
	if (limited.count > CTUI_TABS_MAX)
		limited.count = CTUI_TABS_MAX;
	size_t fit = ctui_tabbar_layout(&limited, COLS, g->unicode, spans);
	int bar_attr = ctui_attr(CTUI_PAIR_BAR, false);

	ctui_fill(y, 0, COLS, bar_attr);
	int x = 0;
	if (bar->brand) {
		char brand[CTUI_TEXT_MAX];
		snprintf(brand, sizeof brand, " %s ", bar->brand);
		x = ctui_put(y, 0, COLS, brand, ctui_attr(CTUI_PAIR_BAR, true));
	}
	for (size_t i = 0; i < fit; i++) {
		const ctui_tab *tab = &bar->tabs[i];
		bool current = i == bar->current;
		bool prev_current = i > 0 && i - 1 == bar->current;
		int attr = current ? ctui_attr_segment(CTUI_TONE_IDLE, true)
				   : ctui_attr(tab->enabled ? CTUI_PAIR_BAR
							    : CTUI_PAIR_SOFT,
					       false);
		if (current)
			x += ctui_put(
			    y, x, COLS - x, g->hard_right,
			    ctui_attr_divider(CTUI_TONE_BAR, CTUI_TONE_IDLE));
		else if (i > 0 && !prev_current)
			x += ctui_put(y, x, COLS - x, g->soft_right,
				      ctui_attr(CTUI_PAIR_SOFT, false));
		const char *icon = tab_icon(tab, g->unicode);
		char body[CTUI_TEXT_MAX];
		snprintf(body, sizeof body, " %s%s%s%s ", icon ? icon : "",
			 icon ? " " : "", tab->label,
			 tab->closable ? " x" : "");
		int w = body_width(tab, g->unicode);
		ctui_fill(y, x, w, attr);
		ctui_put(y, x, w, body, attr);
		x += w;
		if (current)
			x += ctui_put(
			    y, x, COLS - x, g->hard_right,
			    ctui_attr_divider(CTUI_TONE_IDLE, CTUI_TONE_BAR));
	}
	if (bar->hints) {
		int w = (int)ctui_utf8_columns(bar->hints);
		if (COLS - w - 1 > x + 1)
			ctui_put(y, COLS - w - 1, w, bar->hints, bar_attr);
	}
}
