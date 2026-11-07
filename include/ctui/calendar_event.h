/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * calendar_event.h - a dialog to add a calendar event: its title and, unless
 * it lasts all day, the time it starts and ends.
 *
 * It is a framed dialog holding an ctui_form with
 *
 *   Title     the text of the event
 *   All day   a checkbox: the event takes the whole day
 *   Starts    a time field in hours and minutes (see timeinput.h), disabled
 *             for an all-day event
 *   Ends      likewise; it stops at 23:59, an event that runs until the end
 *             of the day is an all-day one
 *   Days      only when ways were set: how the event goes on the selected days
 *             (on each day, as a weekly series, as one event over them all)
 *
 * and OK / Cancel buttons. A line under the form can carry a note, for
 * instance how many days the event is put on. OK (Enter on a field that
 * takes it, or a click on the button) is refused while the end is not after
 * the start; the message says so.
 *
 * The values, the check and the application are pure functions and need no
 * screen. Times are minutes since midnight, as in ctui_cal_event; the
 * values hold them as "HH:MM" texts, which the check also reports when they
 * are not times (the dialog's time fields cannot make such texts, it keeps
 * `values` in step with them after every key and click).
 */
#ifndef CTUI_CALENDAR_EVENT_H
#define CTUI_CALENDAR_EVENT_H

#include <stdbool.h>
#include <stddef.h>

#include <ctui/dialog.h>
#include <ctui/form.h>

#define CTUI_CAL_EVENT_TITLE 96 /* bytes of the title buffer, with the NUL */

/* What the dialog edits, as the user sees it. */
typedef struct ctui_cal_event_values {
	char title[CTUI_CAL_EVENT_TITLE];
	bool all_day;
	char start[6]; /* "HH:MM" */
	char end[6];   /* "HH:MM", "24:00" at most */
} ctui_cal_event_values;

typedef enum ctui_cal_event_error {
	CTUI_CAL_EVENT_OK = 0,
	CTUI_CAL_EVENT_BAD_START, /* the start is not a time HH:MM */
	CTUI_CAL_EVENT_BAD_END,	  /* the end is not a time HH:MM */
	CTUI_CAL_EVENT_END_BEFORE /* the end is not after the start */
} ctui_cal_event_error;

/* The values for an event with this title (may be NULL), from start_minute
 * to end_minute (clamped to 0..1440). */
void ctui_cal_event_values_from(ctui_cal_event_values *v, const char *title,
				int start_minute, int end_minute, bool all_day);

/* Check the values: the times only count when the event is not all day. */
ctui_cal_event_error
ctui_cal_event_values_check(const ctui_cal_event_values *v);

/* Apply the values: the minutes go to *start_minute and *end_minute (either
 * may be NULL), an all-day event being 0 to 1440. Nothing is written unless
 * the result is OK. */
ctui_cal_event_error ctui_cal_event_values_apply(const ctui_cal_event_values *v,
						 int *start_minute,
						 int *end_minute);

/* A one-line message for an error ("" for OK). */
const char *ctui_cal_event_error_text(ctui_cal_event_error e);

/* ---- the dialog ---- */

/* The fifth field, Days, is there only when the caller gives ways. */
#define CTUI_CAL_EVENT_FIELDS 5

typedef struct ctui_cal_event_dialog {
	ctui_cal_event_values values;
	ctui_field fields[CTUI_CAL_EVENT_FIELDS]; /* point into `values`: do
						     not copy the dialog */
	const char *const *ways; /* see ctui_cal_event_dialog_set_ways() */
	size_t way_count;
	ctui_form form;
	ctui_cal_event_error error; /* what is wrong with the values now */
	const char *title;	    /* default "New event" */
	const char *ok;		    /* default "OK" */
	const char *note;	    /* shown under the form, may be NULL */
	const ctui_labels
	    *labels; /* override for the Cancel button, may be NULL */
} ctui_cal_event_dialog;

/* Open the dialog. The dialog must stay where it is while it is used. */
void ctui_cal_event_dialog_init(ctui_cal_event_dialog *d, const char *title,
				int start_minute, int end_minute, bool all_day);

/* Offer ways to put the event on the days it is for (a select field "Days",
 * the first way being chosen): count of at least 2 shows it, anything less
 * hides it. The strings must outlive the dialog. */
void ctui_cal_event_dialog_set_ways(ctui_cal_event_dialog *d,
				    const char *const *ways, size_t count);

/* The way chosen in the field Days (0 without ways). */
size_t ctui_cal_event_dialog_way(const ctui_cal_event_dialog *d);

/* Feed a key: Enter on a field that takes it is OK, accepted only when the
 * values are valid, Esc cancels. */
ctui_dialog_result ctui_cal_event_dialog_key(ctui_cal_event_dialog *d, int key);

/* Get the result of an accepted dialog (the values applied as above). */
ctui_cal_event_error
ctui_cal_event_dialog_result(const ctui_cal_event_dialog *d, int *start_minute,
			     int *end_minute);

/* Where the parts go on a lines x cols screen. */
typedef struct ctui_cal_event_dialog_layout {
	ctui_rect box;
	ctui_rect form; /* the area of the form */
	int error_y;	/* the row of the message, and under it the note */
	int note_y;
	ctui_rect ok, cancel; /* the buttons, one row each */
} ctui_cal_event_dialog_layout;

/* False when the screen is too small (less than 36 x 9 cells for the box). */
bool ctui_cal_event_dialog_layout_for(const ctui_cal_event_dialog *d, int lines,
				      int cols,
				      ctui_cal_event_dialog_layout *out);

/* A left click at screen cell (y, x): the form's fields, the buttons and
 * the close button. OPEN, or ACCEPTED / CANCELLED for the buttons (OK is
 * refused while the values are not valid). */
ctui_dialog_result ctui_cal_event_dialog_click(ctui_cal_event_dialog *d,
					       int lines, int cols, int y,
					       int x);

void ctui_cal_event_dialog_draw(const ctui_cal_event_dialog *d);

#endif /* CTUI_CALENDAR_EVENT_H */
