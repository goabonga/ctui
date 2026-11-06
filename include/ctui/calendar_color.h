/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * calendar_color.h - a background colour for each calendar event.
 *
 * The colour of an event is kept apart from the event, in an array parallel
 * to the events (like the rules and the spans), so ctui_cal_event stays as it
 * is. An index of 0 (or no array) means no colour of its own: the event uses
 * its `pair`. The other indexes name a tone of the theme, so the colours
 * follow the palette (ctui_theme_set_palette()):
 *
 *   1 green   CTUI_TONE_OK        4 blue   CTUI_TONE_INFO
 *   2 red     CTUI_TONE_BAD       5 grey   CTUI_TONE_IDLE
 *   3 yellow  CTUI_TONE_BUSY      6 dark   CTUI_TONE_DARK
 *
 * Event blocks of the week and day views are filled with the tone, and the
 * rows of the events in the month (calendar_month.h) too. A detached
 * occurrence keeps the colour of its series (ctui_cal_view_resolve_scope()).
 */
#ifndef CTUI_CALENDAR_COLOR_H
#define CTUI_CALENDAR_COLOR_H

#include <stdbool.h>

#include <ctui/theme.h>

#define CTUI_CAL_COLORS 6 /* colours 1..CTUI_CAL_COLORS; 0 is none */

typedef struct ctui_cal_color {
	int index; /* 0 none, 1..CTUI_CAL_COLORS */
} ctui_cal_color;

/* Is it 0..CTUI_CAL_COLORS? */
bool ctui_cal_color_valid(int index);

/* The tone of a colour; false (and *tone untouched) for none or an invalid
 * index. */
bool ctui_cal_color_tone(int index, ctui_tone *tone);

/* The name of a colour: "none", "green", "red", "yellow", "blue", "grey",
 * "dark"; "none" for an invalid index. */
const char *ctui_cal_color_name(int index);

/* Step through none, 1 .. CTUI_CAL_COLORS and back to none: delta 1 is the
 * next colour, -1 the previous one. An invalid index counts as none. */
int ctui_cal_color_next(int index, int delta);

/* The curses attribute of text on that colour; 0 for none. */
int ctui_cal_color_attr(int index, bool bold);

#endif /* CTUI_CALENDAR_COLOR_H */
