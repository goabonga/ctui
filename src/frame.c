/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * frame.c - rounded boxes, separators and cards (frame.h).
 */
#include <ctui/frame.h>

#include <curses.h>
#include <stdio.h>

#include <ctui/gauge.h>
#include <ctui/text.h>
#include <ctui/theme.h>

void ctui_frame_draw(int y, int x, int height, int width, bool selected,
		     bool filled, const char *title)
{
	if (height < 3 || width < 4 || y < 0 || x < 0 || y + height > LINES ||
	    x + width > COLS)
		return;
	const ctui_glyphs *g = ctui_theme_glyphs();
	if (filled)
		for (int row = y; row < y + height; row++)
			ctui_fill(row, x, width,
				  ctui_attr(CTUI_PAIR_DIALOG, false));
	int attr = ctui_attr(filled	? CTUI_PAIR_DIALOG_BORDER
			     : selected ? CTUI_PAIR_BORDER_SELECTED
					: CTUI_PAIR_BORDER,
			     !ctui_theme_has_colors() && selected);
	for (int col = x + 1; col < x + width - 1; col++) {
		ctui_put(y, col, 1, g->box_h, attr);
		ctui_put(y + height - 1, col, 1, g->box_h, attr);
	}
	for (int row = y + 1; row < y + height - 1; row++) {
		ctui_put(row, x, 1, g->box_v, attr);
		ctui_put(row, x + width - 1, 1, g->box_v, attr);
	}
	ctui_put(y, x, 1, g->box_tl, attr);
	ctui_put(y, x + width - 1, 1, g->box_tr, attr);
	ctui_put(y + height - 1, x, 1, g->box_bl, attr);
	ctui_put(y + height - 1, x + width - 1, 1, g->box_br, attr);
	if (title && *title && width > 8) {
		char label[CTUI_TEXT_MAX];
		snprintf(label, sizeof label, " %s ", title);
		ctui_put(y, x + 2, width - 4, label,
			 ctui_attr(CTUI_PAIR_HEADING, true));
	}
}

void ctui_frame_close_draw(int y, int x, int width, bool filled)
{
	if (width < 8)
		return;
	int attr =
	    ctui_attr(filled ? CTUI_PAIR_NOTICE_ERROR : CTUI_PAIR_ERROR, true);
	int left = x + width - 1 - CTUI_CLOSE_WIDTH;
	char label[32];
	snprintf(label, sizeof label, " %s ", ctui_theme_glyphs()->close);
	ctui_put(y, left, CTUI_CLOSE_WIDTH, label, attr);
}

bool ctui_frame_close_hit(int y, int x, int width, int my, int mx)
{
	if (width < 8 || my != y)
		return false;
	int left = x + width - 1 - CTUI_CLOSE_WIDTH;
	return mx >= left && mx < left + CTUI_CLOSE_WIDTH;
}

void ctui_separator(int y, int x, int width, int attr)
{
	const ctui_glyphs *g = ctui_theme_glyphs();
	for (int i = 0; i < width; i++)
		ctui_put(y, x + i, 1, g->rule, attr);
}

ctui_card_plan ctui_card_plan_for(int inner)
{
	ctui_card_plan p = {0, -1, -1, -1, -1};
	if (inner >= 6)
		p = (ctui_card_plan){0, 1, 3, 4, 5};
	else if (inner == 5)
		p = (ctui_card_plan){0, 1, 2, 3, 4};
	else if (inner == 4)
		p = (ctui_card_plan){0, -1, 1, 2, 3};
	else if (inner == 3)
		p = (ctui_card_plan){0, -1, 1, 2, -1};
	else if (inner == 2)
		p = (ctui_card_plan){0, -1, 1, -1, -1};
	return p;
}

void ctui_card_draw(const ctui_card *card, int y, int x, int height, int width,
		    bool selected)
{
	if (height < 3 || width < 8)
		return;
	ctui_frame_draw(y, x, height, width, selected, false, NULL);
	int inner = height - 2, left = x + 2, usable = width - 4,
	    half = usable / 2;
	ctui_card_plan plan = ctui_card_plan_for(inner);
	const ctui_glyphs *g = ctui_theme_glyphs();
	int top = y + 1;
	int body = ctui_attr(CTUI_PAIR_BODY, false),
	    muted = ctui_attr(CTUI_PAIR_MUTED, false);

	char title[CTUI_TEXT_MAX];
	const char *icon = g->unicode ? card->icon : card->ascii_icon;
	snprintf(title, sizeof title, "%s%s%s", icon ? icon : "",
		 icon ? " " : "", card->title);
	ctui_put(
	    top + plan.title, left, usable, title,
	    (int)((unsigned)ctui_attr(CTUI_PAIR_HEADING, true) |
		  (selected && !ctui_theme_has_colors() ? (unsigned)A_REVERSE
							: 0U)));
	if (plan.subtitle >= 0 && card->subtitle)
		ctui_put(top + plan.subtitle, left, usable, card->subtitle,
			 muted);
	for (size_t i = 0; i < card->gauge_count && i < CTUI_CARD_GAUGES; i++) {
		int row = i < 2 ? plan.gauge_a : plan.gauge_b;
		if (row < 0)
			continue;
		const ctui_card_gauge *gg = &card->gauges[i];
		ctui_gauge_labeled(top + row, left + (int)(i % 2) * half,
				   half - 1, gg->label, gg->percent, gg->valid,
				   gg->text, body);
	}
	if (plan.note >= 0 && card->note)
		ctui_put(top + plan.note, left, usable, card->note, muted);
}
