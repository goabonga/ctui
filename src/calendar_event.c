/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * calendar_event.c - the dialog to add a calendar event (calendar_event.h).
 */
#include <ctui/calendar_event.h>

#include <curses.h>
#include <stdio.h>
#include <string.h>

#include <ctui/frame.h>
#include <ctui/labels.h>
#include <ctui/text.h>
#include <ctui/theme.h>

enum { F_TITLE, F_ALL_DAY, F_START, F_END };

#define DAY_MINUTES 1440

/* ---- values ---- */

static int clamp(int v, int lo, int hi)
{
	return v < lo ? lo : v > hi ? hi : v;
}

static void format_time(int minute, char *out, size_t cap)
{
	minute = clamp(minute, 0, DAY_MINUTES);
	snprintf(out, cap, "%02d:%02d", minute / 60, minute % 60);
}

void ctui_cal_event_values_from(ctui_cal_event_values *v, const char *title,
				int start_minute, int end_minute, bool all_day)
{
	*v = (ctui_cal_event_values){0};
	if (title)
		snprintf(v->title, sizeof v->title, "%s", title);
	v->all_day = all_day;
	format_time(start_minute, v->start, sizeof v->start);
	format_time(end_minute, v->end, sizeof v->end);
}

/* "HH:MM" with a real time, or "24:00": the minutes since midnight. */
static bool parse_time(const char *s, int *minute)
{
	if (strlen(s) != 5 || s[2] != ':')
		return false;
	for (int i = 0; i < 5; i++)
		if (i != 2 && (s[i] < '0' || s[i] > '9'))
			return false;
	int h = (s[0] - '0') * 10 + (s[1] - '0');
	int m = (s[3] - '0') * 10 + (s[4] - '0');
	if (m > 59 || h > 24 || (h == 24 && m != 0))
		return false;
	*minute = h * 60 + m;
	return true;
}

static ctui_cal_event_error build(const ctui_cal_event_values *v, int *start,
				  int *end)
{
	int s = 0, e = DAY_MINUTES;
	if (!v->all_day) {
		if (!parse_time(v->start, &s) || s >= DAY_MINUTES)
			return CTUI_CAL_EVENT_BAD_START;
		if (!parse_time(v->end, &e))
			return CTUI_CAL_EVENT_BAD_END;
		if (e <= s)
			return CTUI_CAL_EVENT_END_BEFORE;
	}
	if (start)
		*start = s;
	if (end)
		*end = e;
	return CTUI_CAL_EVENT_OK;
}

ctui_cal_event_error ctui_cal_event_values_check(const ctui_cal_event_values *v)
{
	return build(v, NULL, NULL);
}

ctui_cal_event_error ctui_cal_event_values_apply(const ctui_cal_event_values *v,
						 int *start_minute,
						 int *end_minute)
{
	int s = 0, e = 0;
	ctui_cal_event_error err = build(v, &s, &e);
	if (err != CTUI_CAL_EVENT_OK)
		return err;
	if (start_minute)
		*start_minute = s;
	if (end_minute)
		*end_minute = e;
	return CTUI_CAL_EVENT_OK;
}

const char *ctui_cal_event_error_text(ctui_cal_event_error e)
{
	switch (e) {
	case CTUI_CAL_EVENT_BAD_START:
		return "Starts: type a time as HH:MM";
	case CTUI_CAL_EVENT_BAD_END:
		return "Ends: type a time as HH:MM (24:00 at most)";
	case CTUI_CAL_EVENT_END_BEFORE:
		return "The end must come after the start";
	default:
		return "";
	}
}

/* ---- the dialog ---- */

static void sync(ctui_cal_event_dialog *d)
{
	ctui_field *f = d->fields;
	// NOLINTNEXTLINE(clang-analyzer-core.NullDereference)
	d->values.all_day = f[F_ALL_DAY].checked;
	f[F_START].disabled = d->values.all_day;
	f[F_END].disabled = d->values.all_day;
	d->error = ctui_cal_event_values_check(&d->values);
}

void ctui_cal_event_dialog_init(ctui_cal_event_dialog *d, const char *title,
				int start_minute, int end_minute, bool all_day)
{
	*d = (ctui_cal_event_dialog){0};
	ctui_cal_event_values_from(&d->values, title, start_minute, end_minute,
				   all_day);
	ctui_cal_event_values *v = &d->values;
	ctui_field *f = d->fields;
	f[F_TITLE] = ctui_field_text("Title", v->title, sizeof v->title);
	f[F_ALL_DAY] = ctui_field_check("All day", NULL, v->all_day);
	f[F_START] = ctui_field_text("Starts", v->start, sizeof v->start);
	f[F_END] = ctui_field_text("Ends", v->end, sizeof v->end);
	ctui_form_init(&d->form, f, CTUI_CAL_EVENT_FIELDS);
	d->form.dialog = true;
	sync(d);
	ctui_form_init(&d->form, f, CTUI_CAL_EVENT_FIELDS);
}

ctui_dialog_result ctui_cal_event_dialog_key(ctui_cal_event_dialog *d, int key)
{
	ctui_form_event e = ctui_form_key(&d->form, key);
	sync(d);
	if (e == CTUI_FORM_CANCELLED)
		return CTUI_DIALOG_CANCELLED;
	if (e == CTUI_FORM_SUBMITTED)
		return d->error == CTUI_CAL_EVENT_OK ? CTUI_DIALOG_ACCEPTED
						     : CTUI_DIALOG_OPEN;
	return CTUI_DIALOG_OPEN;
}

ctui_cal_event_error
ctui_cal_event_dialog_result(const ctui_cal_event_dialog *d, int *start_minute,
			     int *end_minute)
{
	return ctui_cal_event_values_apply(&d->values, start_minute,
					   end_minute);
}

/* ---- layout, click and drawing ---- */

#define BOX_WIDTH 46
#define BOX_HEIGHT                                                             \
	(CTUI_CAL_EVENT_FIELDS +                                               \
	 5) /* the fields, a message, a note, the buttons, the frame */

/* The captions of the buttons, "[ OK ]" and "[ Cancel ]" by default. */
static void captions(const ctui_cal_event_dialog *d, char *ok, size_t okcap,
		     char *cancel, size_t cancelcap)
{
	ctui_labels labels = ctui_labels_resolve(d ? d->labels : NULL);
	snprintf(ok, okcap, "[ %s ]", d && d->ok ? d->ok : "OK");
	snprintf(cancel, cancelcap, "[ %s ]",
		 labels.cancel ? labels.cancel : "Cancel");
}

bool ctui_cal_event_dialog_layout_for(const ctui_cal_event_dialog *d, int lines,
				      int cols,
				      ctui_cal_event_dialog_layout *out)
{
	char ok[40], cancel[40];
	captions(d, ok, sizeof ok, cancel, sizeof cancel);
	int ok_width = (int)ctui_utf8_columns(ok),
	    cancel_width = (int)ctui_utf8_columns(cancel);
	ctui_rect box;
	if (!ctui_dialog_box(lines, cols, BOX_WIDTH, BOX_HEIGHT, &box) ||
	    box.width < 36 || box.height < 9)
		return false;
	int formh = box.height - 5;
	int by = box.y + box.height - 2;
	int bx = box.x + (box.width - (ok_width + 3 + cancel_width)) / 2;
	if (bx < box.x + 1)
		bx = box.x + 1;
	out->box = box;
	out->form = (ctui_rect){box.y + 1, box.x + 2, formh, box.width - 4};
	out->error_y = box.y + box.height - 4;
	out->note_y = box.y + box.height - 3;
	out->ok = (ctui_rect){by, bx, 1, ok_width};
	out->cancel = (ctui_rect){by, bx + ok_width + 3, 1, cancel_width};
	return true;
}

static bool inside(const ctui_rect *r, int y, int x)
{
	return y >= r->y && y < r->y + r->height && x >= r->x &&
	       x < r->x + r->width;
}

ctui_dialog_result ctui_cal_event_dialog_click(ctui_cal_event_dialog *d,
					       int lines, int cols, int y,
					       int x)
{
	ctui_cal_event_dialog_layout l;
	if (!ctui_cal_event_dialog_layout_for(d, lines, cols, &l))
		return CTUI_DIALOG_OPEN;
	if (ctui_frame_close_hit(l.box.y, l.box.x, l.box.width, y, x) ||
	    inside(&l.cancel, y, x))
		return CTUI_DIALOG_CANCELLED;
	if (inside(&l.ok, y, x))
		return d->error == CTUI_CAL_EVENT_OK ? CTUI_DIALOG_ACCEPTED
						     : CTUI_DIALOG_OPEN;
	if (inside(&l.form, y, x)) {
		ctui_form_click(&d->form, l.form.width, l.form.height,
				y - l.form.y, x - l.form.x);
		sync(d);
	}
	return CTUI_DIALOG_OPEN;
}

void ctui_cal_event_dialog_draw(const ctui_cal_event_dialog *d)
{
	ctui_cal_event_dialog_layout l;
	int text = ctui_attr(CTUI_PAIR_DIALOG, false);
	const char *title = d->title ? d->title : "New event";
	if (!ctui_cal_event_dialog_layout_for(d, LINES, COLS, &l)) {
		ctui_rect box;
		if (ctui_dialog_box(LINES, COLS, 30, 3, &box)) {
			ctui_frame_draw(box.y, box.x, box.height, box.width,
					true, true, title);
			ctui_put(box.y + 1, box.x + 2, box.width - 4,
				 "Resize the terminal", text);
		}
		return;
	}
	ctui_frame_draw(l.box.y, l.box.x, l.box.height, l.box.width, true, true,
			title);
	ctui_frame_close_draw(l.box.y, l.box.x, l.box.width, true);
	ctui_form_draw(&d->form, l.form.y, l.form.x, l.form.width,
		       l.form.height);
	int inner = l.box.width - 4;
	if (d->error != CTUI_CAL_EVENT_OK)
		ctui_put(l.error_y, l.box.x + 2, inner,
			 ctui_cal_event_error_text(d->error),
			 ctui_attr(CTUI_PAIR_NOTICE_ERROR, true));
	else if (d->note)
		ctui_put(l.note_y, l.box.x + 2, inner, d->note,
			 (int)((unsigned)text | (unsigned)A_DIM));
	char ok[40], cancel[40];
	captions(d, ok, sizeof ok, cancel, sizeof cancel);
	int on = ctui_attr(CTUI_PAIR_SELECTION, true);
	ctui_put(l.ok.y, l.ok.x, l.ok.width, ok,
		 d->error == CTUI_CAL_EVENT_OK
		     ? on
		     : (int)((unsigned)text | (unsigned)A_DIM));
	ctui_put(l.cancel.y, l.cancel.x, l.cancel.width, cancel, text);
}
