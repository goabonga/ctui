/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * form.h - labelled form fields: text, checkbox, select, slider, list, date,
 * time and duration.
 *
 * A form is a column of fields, each with its label on the left and its
 * control on the right; labels are aligned on the widest one. The caller
 * owns the fields (and the text buffers); the form owns only the focus and
 * the state of the open select. Like the dialogs it never blocks: feed keys
 * to ctui_form_key() or clicks to ctui_form_click(), redraw with
 * ctui_form_draw().
 *
 *   text    edits UTF-8 text in a caller buffer with a ctui_input: cursor
 *           moves, word jumps, Backspace, Delete, Ctrl-W/U/K, sideways
 *           scrolling and a click that places the cursor; mask hides it
 *           behind '*', max_chars bounds it and placeholder shows while it
 *           is empty; a rule (inputrule.h: password, e-mail, phone number,
 *           bank card, or one of your own) limits and lays out what is
 *           typed and marks the text right or wrong.
 *   check   Space toggles.
 *   select  Left/Right step through the options; Enter / Space open a
 *           dropdown (Up/Down/Home/End move, Enter / Space choose, Esc
 *           closes).
 *   slider  Left/Right move by one step, PgUp/PgDn by ten, Home/End to the
 *           ends.
 *   list    a box of rows showing several options at once; Up/Down/Home/
 *           End/PgUp/PgDn move the choice.
 *   number  a ctui_numinput (range, step, decimals, unit): digits are typed,
 *           PgUp/PgDn add or subtract a step (Home/End go to the ends), Enter
 *           submits and the value is committed when the focus leaves.
 *   date    a ctui_dateinput; Left/Right change the segment, digits and
 *           '-' type, PgUp/PgDn step it, Enter opens its month picker (the
 *           keys of ctui_calendar then apply, Enter picks, Esc closes).
 *   time    a ctui_timeinput (hours to microseconds, optional limits); the
 *           same keys, '+' and '-' step too, Enter submits.
 *   duration a ctui_durationinput (hours to microseconds, optional maximum);
 *           the same keys as the time field.
 *
 * Up/Down (and Tab / Shift-Tab) move between fields; Up/Down inside a list
 * only leave it past its first or last option. Enter submits the form
 * (except on a select, where it opens the dropdown) and Esc cancels it.
 * Set dialog when the form sits inside a framed dialog: fields then stand
 * out from the dialog background instead of from the page.
 *
 * While ctui_form_editing() is true the keyboard belongs to the form: typing
 * letters edits text, so callers must not treat them as shortcuts.
 */
#ifndef CTUI_FORM_H
#define CTUI_FORM_H

#include <stdbool.h>
#include <stddef.h>

#include <ctui/dateinput.h>
#include <ctui/dialog.h>
#include <ctui/durationinput.h>
#include <ctui/input.h>
#include <ctui/numinput.h>
#include <ctui/timeinput.h>

typedef enum ctui_field_kind {
	CTUI_FIELD_TEXT = 0,
	CTUI_FIELD_CHECK,
	CTUI_FIELD_SELECT,
	CTUI_FIELD_SLIDER,
	CTUI_FIELD_LIST,
	CTUI_FIELD_DATE,
	CTUI_FIELD_TIME,
	CTUI_FIELD_DURATION,
	CTUI_FIELD_NUMBER
} ctui_field_kind;

// NOLINTNEXTLINE(clang-analyzer-optin.performance.Padding)
typedef struct ctui_field {
	ctui_field_kind kind;
	const char *label;
	bool disabled;
	/* text */
	char *text;	 /* caller buffer, NUL-terminated */
	size_t capacity; /* bytes, including the NUL */
	bool mask;
	size_t max_chars; /* 0 = limited by the buffer only */
	const char
	    *placeholder; /* shown while the text is empty, may be NULL */
	const ctui_input_rule *rule; /* what may be typed, see inputrule.h */
	ctui_input input; /* the editor of the text, set by the constructor */
	/* check */
	bool checked;
	const char *caption; /* shown after the box, may be NULL */
	/* select and list */
	const char *const *options;
	size_t option_count;
	size_t choice;
	size_t first; /* list: first visible option */
	int rows;     /* list: visible rows */
	/* slider */
	double value, min, max, step;
	int decimals; /* digits after the point when the value is shown */
	/* date, time and duration: the value lives in the input, read it there
	 * (ctui_dateinput_date(), .micro); set the limits of a time or the
	 * maximum of a duration on it after the constructor */
	ctui_dateinput date;
	ctui_timeinput time;
	ctui_durationinput duration;
	/* number: the value lives in the input, read it there (.number.value);
	 * its unit and other settings are set on it after the constructor */
	ctui_numinput number;
} ctui_field;

ctui_field ctui_field_text(const char *label, char *buffer, size_t capacity);
ctui_field ctui_field_check(const char *label, const char *caption,
			    bool checked);
ctui_field ctui_field_select(const char *label, const char *const *options,
			     size_t count, size_t choice);
ctui_field ctui_field_slider(const char *label, double min, double max,
			     double step, double value, int decimals);
ctui_field ctui_field_list(const char *label, const char *const *options,
			   size_t count, size_t choice, int rows);

/* A date (an invalid one gives 1970-01-01), a time of day cut to the
 * precision, and a duration from the largest to the smallest unit; see the
 * constructors of the inputs for the details. */
ctui_field ctui_field_number(const char *label, double min, double max,
			     double step, double value, int decimals);
ctui_field ctui_field_date(const char *label, int year, int month, int day);
ctui_field ctui_field_time(const char *label, ctui_time_precision precision,
			   long long micro);
ctui_field ctui_field_duration(const char *label, ctui_duration_unit largest,
			       ctui_duration_unit smallest, long long micro);

typedef struct ctui_form {
	ctui_field *fields;
	size_t count;
	size_t focus;
	bool blurred;	/* another part of the screen has the focus: no field is
			   drawn as focused (the focus index is kept) */
	bool dialog;	/* drawn inside a dialog (on the dialog background) */
	bool dropdown;	/* the select under the focus is open */
	size_t pending; /* option under the cursor of the open dropdown */
	const ctui_style *style; /* colours and glyphs of this instance, may be
				    NULL; set after init */
} ctui_form;

typedef enum ctui_form_event {
	CTUI_FORM_NONE = 0,
	CTUI_FORM_MOVED,   /* focus or dropdown changed */
	CTUI_FORM_CHANGED, /* a value changed */
	CTUI_FORM_SUBMITTED,
	CTUI_FORM_CANCELLED
} ctui_form_event;

/* Focus the first enabled field. */
void ctui_form_init(ctui_form *f, ctui_field *fields, size_t count);

/* True while the focused field takes typed characters (a text, number, date,
 * time or duration field), or a dropdown or a month picker is open. */
bool ctui_form_editing(const ctui_form *f);

ctui_form_event ctui_form_key(ctui_form *f, int key);

/* Rows the fields take (a list takes its row count, the others one). */
int ctui_form_rows(const ctui_form *f);

/* Where field i goes in a form width columns wide, relative to the top
 * left of the form: its label and its control. False when the form is too
 * narrow for a control of at least 8 columns. */
bool ctui_form_slot(const ctui_form *f, size_t i, int width, ctui_rect *label,
		    ctui_rect *control);

/* Click at (y, x) relative to a form drawn in width x height cells. It
 * focuses the field, and acts on its control: toggles a check, opens a
 * select or picks from its dropdown, sets a slider, picks a list row. */
ctui_form_event ctui_form_click(ctui_form *f, int width, int height, int y,
				int x);

/* Draw in a width x height area; it scrolls to keep the focused field
 * visible. */
void ctui_form_draw(const ctui_form *f, int y, int x, int width, int height);

#endif /* CTUI_FORM_H */
