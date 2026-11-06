/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * statusbar.c - the powerline status bar (statusbar.h).
 */
#include <ctui/statusbar.h>

#include <curses.h>
#include <stdio.h>

#include <ctui/text.h>

int ctui_statusbar_width(const ctui_segment *segments, size_t count)
{
	int w = 0;
	for (size_t i = 0; i < count; i++)
		w += (int)ctui_utf8_columns(segments[i].text) + 2 + 1;
	return w;
}

static int draw_text(int y, int x, const ctui_segment *s)
{
	char buf[CTUI_TEXT_MAX];
	snprintf(buf, sizeof buf, " %s ", s->text ? s->text : "");
	return ctui_put(y, x, COLS - x, buf,
			ctui_attr_segment(s->tone, s->bold));
}

void ctui_statusbar_draw(int y, const ctui_segment *left, size_t left_count,
			 const ctui_segment *right, size_t right_count)
{
	const ctui_glyphs *g = ctui_theme_glyphs();
	ctui_fill(y, 0, COLS, ctui_attr(CTUI_PAIR_BAR, false));

	int x = 0;
	for (size_t i = 0; i < left_count; i++) {
		x += draw_text(y, x, &left[i]);
		ctui_tone next =
		    i + 1 < left_count ? left[i + 1].tone : CTUI_TONE_BAR;
		if (next == left[i].tone)
			x += ctui_put(y, x, COLS - x, g->soft_right,
				      ctui_attr(CTUI_PAIR_SOFT, false));
		else
			x += ctui_put(y, x, COLS - x, g->hard_right,
				      ctui_attr_divider(left[i].tone, next));
	}

	int width = ctui_statusbar_width(right, right_count);
	if (right_count == 0 || width + x >= COLS)
		return;
	x = COLS - width;
	ctui_tone previous = CTUI_TONE_BAR;
	for (size_t i = 0; i < right_count; i++) {
		if (previous == right[i].tone)
			x += ctui_put(y, x, COLS - x, g->soft_left,
				      ctui_attr(CTUI_PAIR_SOFT, false));
		else
			x += ctui_put(
			    y, x, COLS - x, g->hard_left,
			    ctui_attr_divider(right[i].tone, previous));
		x += draw_text(y, x, &right[i]);
		previous = right[i].tone;
	}
}
