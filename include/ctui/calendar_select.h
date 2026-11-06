/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * calendar_select.h - a set of selected days, shared by the month grid
 * (ctui_calendar) and the week and day views (ctui_cal_view).
 *
 * The set is a sorted array of day numbers (days since 1970-01-01, see
 * ctui_date_to_days()) of at most CTUI_CAL_SELECT_MAX days, within the years
 * 1 to 9999. It is plain state with no drawing: the components change it
 * through their keys and mouse, the caller reads it to place an event on
 * the selected days (see ctui_cal_plan_repeat() and ctui_cal_plan_span()).
 *
 * The anchor is the day a contiguous range grows from. A range is set by
 * extending from the anchor (Shift+arrows) or by a mouse drag; a toggle
 * adds or removes one day and makes it the new anchor; moving the cursor
 * without Shift forgets the anchor but keeps the days.
 */
#ifndef CTUI_CALENDAR_SELECT_H
#define CTUI_CALENDAR_SELECT_H

#include <stdbool.h>

#define CTUI_CAL_SELECT_MAX 64

typedef struct ctui_cal_select {
	long days[CTUI_CAL_SELECT_MAX]; /* ascending, without duplicates */
	int count;
	long anchor; /* valid when has_anchor */
	bool has_anchor;
	bool
	    dragging; /* a mouse drag that started at `anchor` is in progress */
} ctui_cal_select;

/* Empty the set and forget the anchor and the drag. */
void ctui_cal_select_clear(ctui_cal_select *s);

bool ctui_cal_select_contains(const ctui_cal_select *s, long day);

/* Add a day. False when it is out of the supported years or the set is
 * full; adding a day already in the set succeeds. */
bool ctui_cal_select_add(ctui_cal_select *s, long day);

/* Add the day when absent, remove it when present, and make it the anchor.
 * False when nothing changed (out of the years, or full when adding). */
bool ctui_cal_select_toggle(ctui_cal_select *s, long day);

/* Replace the set by all days from a to b (either order), with `a` as the
 * anchor. False, and nothing changed, when the range is out of the years or
 * longer than CTUI_CAL_SELECT_MAX days. */
bool ctui_cal_select_set_range(ctui_cal_select *s, long a, long b);

/* Shift+arrow: the cursor went from `from` to `to`. The set becomes the
 * range from the anchor (`from` when there is none) to `to`. False when it
 * does not fit, nothing changed. */
bool ctui_cal_select_extend(ctui_cal_select *s, long from, long to);

/* The cursor moved without Shift: the days stay, the anchor is forgotten. */
void ctui_cal_select_move(ctui_cal_select *s);

/* Mouse drag: begin() empties the set and arms the drag at a day (a plain
 * click selects nothing); drag_to() makes the set the range from there to
 * the day, or empty on the day itself; end() disarms. begin() and drag_to()
 * return true when the set changed; drag_to() does nothing unless armed. */
bool ctui_cal_select_begin(ctui_cal_select *s, long day);
bool ctui_cal_select_drag_to(ctui_cal_select *s, long day);
void ctui_cal_select_end(ctui_cal_select *s);

/* The sorted days: count, and the first and last (0 when empty). */
int ctui_cal_select_count(const ctui_cal_select *s);
long ctui_cal_select_first(const ctui_cal_select *s);
long ctui_cal_select_last(const ctui_cal_select *s);

/* The i-th day in ascending order, or 0 when i is out of 0..count-1. */
long ctui_cal_select_at(const ctui_cal_select *s, int i);

/* True when the days are consecutive (a set of one day counts). */
bool ctui_cal_select_contiguous(const ctui_cal_select *s);

#endif /* CTUI_CALENDAR_SELECT_H */
