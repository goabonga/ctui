/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * calendar_dialog.h - a dialog to edit the recurrence of a calendar event.
 *
 * It is a framed dialog holding an ctui_form with
 *
 *   Repeat       does not repeat, daily, weekly, monthly, yearly
 *   Every        the interval: every N days / weeks / months / years
 *   Monday ...   seven checkboxes, for a weekly repeat
 *   Start date   YYYY-MM-DD, the day of the first occurrence
 *   Ends         never, on a date, after a number of times
 *   Until date   YYYY-MM-DD, for "on a date"
 *   Occurrences  the count, for "after a number of times"
 *
 * and OK / Cancel buttons. Fields that do not apply to the repeat or to
 * the end chosen are disabled (skipped by Tab and Up / Down). Under the
 * form the dialog tells what is wrong with the values, if anything, and
 * otherwise previews the next occurrences; OK (Enter on a field that takes
 * it, or a click on the button) is refused while something is wrong.
 *
 * Dates are typed as text and checked with real calendar rules (month ends,
 * leap years, years 1 to 9999, see dateinput.h); the interval must be 1 to
 * 10000, the count 1 to 1000000, a weekly repeat needs a weekday and the
 * last day may not come before the start.
 *
 * The values, the check and the application to an ctui_cal_recur are pure
 * functions and need no screen. The exceptions of the rule are kept when it
 * is applied (a rule that does not repeat has none).
 */
#ifndef CTUI_CALENDAR_DIALOG_H
#define CTUI_CALENDAR_DIALOG_H

#include <stdbool.h>
#include <stddef.h>

#include <ctui/calendar_view.h>
#include <ctui/dialog.h>
#include <ctui/form.h>

#define CTUI_CAL_RECUR_INTERVAL_MAX 10000
#define CTUI_CAL_RECUR_COUNT_MAX 1000000

/* What the dialog edits, as the user sees it. */
typedef struct ctui_cal_recur_values {
	int repeat;	  /* ctui_cal_repeat: 0 none ... 4 yearly */
	char interval[8]; /* text */
	bool weekdays[7]; /* Monday first */
	char start[11];	  /* "YYYY-MM-DD" */
	int end;	  /* ctui_cal_end: 0 never, 1 until, 2 count */
	char until[11];	  /* "YYYY-MM-DD" */
	char count[8];	  /* text */
} ctui_cal_recur_values;

typedef enum ctui_cal_recur_error {
	CTUI_CAL_RECUR_OK = 0,
	CTUI_CAL_RECUR_BAD_START, /* the start is not a real date YYYY-MM-DD */
	CTUI_CAL_RECUR_BAD_INTERVAL, /* not a whole number from 1 to 10000 */
	CTUI_CAL_RECUR_NO_WEEKDAY,   /* weekly with no weekday */
	CTUI_CAL_RECUR_BAD_UNTIL,    /* the end date is not a real date */
	CTUI_CAL_RECUR_UNTIL_BEFORE, /* the end date is before the start */
	CTUI_CAL_RECUR_BAD_COUNT     /* not a whole number from 1 to 1000000 */
} ctui_cal_recur_error;

/* The values of an event starting on day `start_day` (days since
 * 1970-01-01, as ctui_date_to_days(); clamped to years 1 to 9999) with rule r
 * (may be NULL: does not repeat). A weekly rule without weekdays gets the
 * weekday of the start; the other fields get usable defaults (every 1, the
 * start as end date, 10 times). */
void ctui_cal_recur_values_from(ctui_cal_recur_values *v, long start_day,
				const ctui_cal_recur *r);

/* Check the values. Only the fields that matter count: the interval when it
 * repeats, the weekdays when weekly, the end date or the count according to
 * the end. */
ctui_cal_recur_error
ctui_cal_recur_values_check(const ctui_cal_recur_values *v);

/* Apply the values: the start day goes to *start_day (may be NULL) and the
 * rule to *r. Nothing is written unless the result is OK. The weekday mask
 * is only set for a weekly repeat; the exceptions of *r are kept. */
ctui_cal_recur_error ctui_cal_recur_values_apply(const ctui_cal_recur_values *v,
						 long *start_day,
						 ctui_cal_recur *r);

/* A one-line message for an error ("" for OK). */
const char *ctui_cal_recur_error_text(ctui_cal_recur_error e);

/* "Next: 2026-11-02, 2026-11-04, 2026-11-06" (up to three) or "Does not
 * repeat"; false and "" when the values are not valid. */
bool ctui_cal_recur_values_preview(const ctui_cal_recur_values *v, char *out,
				   size_t cap);

/* ---- the dialog ---- */

#define CTUI_CAL_RECUR_FIELDS 13

typedef struct ctui_cal_recur_dialog {
	ctui_cal_recur_values values;
	ctui_field fields[CTUI_CAL_RECUR_FIELDS]; /* point into `values`: do not
						     copy the dialog */
	ctui_form form;
	ctui_cal_recur_error error; /* what is wrong with the values now */
	const char *title;	    /* default "Repeat" */
	const char *ok;		    /* default "OK" */
	const ctui_labels
	    *labels; /* override for the Cancel button, may be NULL */
} ctui_cal_recur_dialog;

/* Open the dialog on an event starting on `start_day` with rule r (may be
 * NULL). The dialog must stay where it is while it is used. */
void ctui_cal_recur_dialog_init(ctui_cal_recur_dialog *d, long start_day,
				const ctui_cal_recur *r);

/* Feed a key: Enter on a field that does not take it (a select opens its
 * dropdown) is OK, accepted only when the values are valid, Esc cancels. */
ctui_dialog_result ctui_cal_recur_dialog_key(ctui_cal_recur_dialog *d, int key);

/* True while typed characters belong to the dialog (always: it is modal). */
bool ctui_cal_recur_dialog_editing(const ctui_cal_recur_dialog *d);

/* Get the result of an accepted dialog (the values applied as above). */
ctui_cal_recur_error
ctui_cal_recur_dialog_result(const ctui_cal_recur_dialog *d, long *start_day,
			     ctui_cal_recur *r);

/* Where the parts go on a lines x cols screen. */
typedef struct ctui_cal_recur_dialog_layout {
	ctui_rect box;
	ctui_rect form; /* the area of the form */
	int error_y;	/* the row of the message, and under it the preview */
	int preview_y;
	ctui_rect ok, cancel; /* the buttons, one row each */
} ctui_cal_recur_dialog_layout;

/* False when the screen is too small (less than 40 x 10 cells for the box).
 * The buttons are as wide as their captions need, which come from `d` (may
 * be NULL for the defaults). */
bool ctui_cal_recur_dialog_layout_for(const ctui_cal_recur_dialog *d, int lines,
				      int cols,
				      ctui_cal_recur_dialog_layout *out);

/* A left click at screen cell (y, x): the form's fields, the buttons and
 * the close button. OPEN, or ACCEPTED / CANCELLED for the buttons (OK is
 * refused, with the message shown, while the values are not valid). */
ctui_dialog_result ctui_cal_recur_dialog_click(ctui_cal_recur_dialog *d,
					       int lines, int cols, int y,
					       int x);

void ctui_cal_recur_dialog_draw(const ctui_cal_recur_dialog *d);

#endif /* CTUI_CALENDAR_DIALOG_H */
