/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * frame.h - rounded boxes, separators and cards.
 *
 * A frame is an outline with rounded corners ("+" corners and "-" "|"
 * edges in ASCII locales); a filled frame also paints its inside with the
 * dialog background. A selected frame uses the highlight colour (bold when
 * there are no colours). Everything is clipped to the screen.
 *
 * A card is a framed summary of one item: an icon and title, a subtitle,
 * up to four labelled gauges in two columns, and a note. How many of those
 * rows appear depends on the height, see ctui_card_plan().
 */
#ifndef CTUI_FRAME_H
#define CTUI_FRAME_H

#include <stdbool.h>
#include <stddef.h>

#define CTUI_CARD_GAUGES 4

/* Draw a frame of height x width cells at (y, x). title (may be NULL) is
 * shown as " title " in the top edge. Frames smaller than 3x4 are skipped. */
void ctui_frame_draw(int y, int x, int height, int width, bool selected,
		     bool filled, const char *title);

#define CTUI_CLOSE_WIDTH 3

/* Draw a close button (" x ", a multiplication sign in UTF-8 locales) in
 * the top right of a frame drawn at (y, x) and width cells wide, over its
 * edge. filled says whether the frame is filled (dialog background). */
void ctui_frame_close_draw(int y, int x, int width, bool filled);

/* Whether screen cell (my, mx) is on that close button. */
bool ctui_frame_close_hit(int y, int x, int width, int my, int mx);

/* A horizontal rule of width cells. */
void ctui_separator(int y, int x, int width, int attr);

typedef struct ctui_card_gauge {
	const char *label;
	double percent;
	bool valid;
	const char *text;
} ctui_card_gauge;

typedef struct ctui_card {
	const char *icon;	/* UTF-8 locales; may be NULL */
	const char *ascii_icon; /* other locales; may be NULL */
	const char *title;
	const char *subtitle; /* may be NULL */
	const char *note;     /* may be NULL */
	ctui_card_gauge gauges[CTUI_CARD_GAUGES];
	size_t gauge_count;
} ctui_card;

/* Which rows (inside the frame, 0-based) a card of the given inner height
 * uses; -1 when the row is not shown. */
typedef struct ctui_card_plan {
	int title, subtitle, gauge_a, gauge_b, note;
} ctui_card_plan;

ctui_card_plan ctui_card_plan_for(int inner_height);

void ctui_card_draw(const ctui_card *card, int y, int x, int height, int width,
		    bool selected);

#endif /* CTUI_FRAME_H */
