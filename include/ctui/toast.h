/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * toast.h - transient notifications stacked in a corner of the screen.
 *
 * A toast is one row: a level glyph, a message and a close mark, on the
 * tone of its level. Toasts stack downwards from a top row, right-aligned,
 * the oldest first. The set holds CTUI_TOAST_MAX of them; pushing one more
 * drops the oldest. Time is passed in by the caller (any monotonic clock,
 * in seconds), so nothing here reads a clock and expiry is testable.
 */
#ifndef CTUI_TOAST_H
#define CTUI_TOAST_H

#include <stdbool.h>
#include <stddef.h>

#include <ctui/dialog.h>

#define CTUI_TOAST_MAX 4       /* shown at once unless `limit` says otherwise */
#define CTUI_TOAST_CAPACITY 16 /* the most `limit` can ask for */
#define CTUI_TOAST_TEXT 120    /* bytes kept of a message, incl. NUL */

typedef enum ctui_toast_level {
	CTUI_TOAST_INFO = 0,
	CTUI_TOAST_OK,
	CTUI_TOAST_WARN,
	CTUI_TOAST_ERROR
} ctui_toast_level;

typedef struct ctui_toast {
	ctui_toast_level level;
	char text[CTUI_TOAST_TEXT];
	double expires; /* seconds; 0 for a toast that stays until dismissed */
} ctui_toast;

typedef struct ctui_toasts {
	ctui_toast items[CTUI_TOAST_CAPACITY]; /* oldest first */
	size_t count;
	size_t limit; /* toasts kept at once: 0 = CTUI_TOAST_MAX, at most
			 CTUI_TOAST_CAPACITY */
	const ctui_style *style; /* colours and glyphs of this instance, may be
				    NULL; set after init */
} ctui_toasts;

void ctui_toasts_init(ctui_toasts *t);

/* Add a toast shown for duration seconds from now; a duration of 0 or
 * less keeps it until it is dismissed. The text is truncated to fit. */
void ctui_toasts_push(ctui_toasts *t, ctui_toast_level level, const char *text,
		      double now, double duration);

/* Remove the toasts whose time is up; returns how many went. */
size_t ctui_toasts_expire(ctui_toasts *t, double now);

/* Remove toast i; false when there is none. */
bool ctui_toasts_dismiss(ctui_toasts *t, size_t i);

/* Where toast i goes when stacked from row top on a lines x cols screen;
 * false when it does not fit. */
bool ctui_toasts_geometry(const ctui_toasts *t, int top, int lines, int cols,
			  size_t i, ctui_rect *box);

/* Toast under (y, x), or -1. */
int ctui_toasts_hit(const ctui_toasts *t, int top, int lines, int cols, int y,
		    int x);

/* Draw every toast that fits. */
void ctui_toasts_draw(const ctui_toasts *t, int top);

#endif /* CTUI_TOAST_H */
