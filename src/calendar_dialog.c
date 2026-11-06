/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * calendar_dialog.c - the recurrence dialog (calendar_dialog.h).
 */
#include <ctui/calendar_dialog.h>

#include <curses.h>
#include <stdio.h>
#include <string.h>

#include <ctui/dateinput.h>
#include <ctui/frame.h>
#include <ctui/text.h>

enum {
	F_REPEAT,
	F_EVERY,
	F_DAY0, /* seven weekdays */
	F_START = F_DAY0 + 7,
	F_END,
	F_UNTIL,
	F_COUNT
};

static const char *const REPEATS[5] = {"Does not repeat", "Daily", "Weekly",
				       "Monthly", "Yearly"};
static const char *const ENDS[3] = {"Never", "On a date",
				    "After a number of times"};
static const char *const DAYS[7] = {"Monday",	"Tuesday", "Wednesday",
				    "Thursday", "Friday",  "Saturday",
				    "Sunday"};

static int clamp(int v, int lo, int hi)
{
	return v < lo ? lo : v > hi ? hi : v;
}

/* ---- values ---- */

static void format_day(long day, char *out, size_t cap)
{
	if (day < ctui_dateinput_min_days())
		day = ctui_dateinput_min_days();
	if (day > ctui_dateinput_max_days())
		day = ctui_dateinput_max_days();
	ctui_dateinput_format(ctui_date_from_days(day), out, cap);
}

void ctui_cal_recur_values_from(ctui_cal_recur_values *v, long start_day,
				const ctui_cal_recur *r)
{
	*v = (ctui_cal_recur_values){0};
	format_day(start_day, v->start, sizeof v->start);
	ctui_date d = ctui_date_from_days(
	    start_day < ctui_dateinput_min_days()   ? ctui_dateinput_min_days()
	    : start_day > ctui_dateinput_max_days() ? ctui_dateinput_max_days()
						    : start_day);
	int interval = 1, count = 10;
	unsigned mask = 0;
	if (r) {
		v->repeat = clamp((int)r->repeat, 0, 4);
		if (r->interval >= 1 &&
		    r->interval <= CTUI_CAL_RECUR_INTERVAL_MAX)
			interval = r->interval;
		mask = r->weekdays & 0x7Fu;
		v->end = clamp((int)r->end, 0, 2);
		if (r->count >= 1 && r->count <= CTUI_CAL_RECUR_COUNT_MAX)
			count = r->count;
	}
	if (!mask) /* the weekday of the start, so that switching to weekly is
		      usable */
		mask =
		    1U << (unsigned)ctui_date_weekday(d.year, d.month, d.day);
	for (int i = 0; i < 7; i++)
		v->weekdays[i] = (mask >> (unsigned)i) & 1U;
	snprintf(v->interval, sizeof v->interval, "%d", interval);
	snprintf(v->count, sizeof v->count, "%d", count);
	long until = start_day;
	if (r && r->end == CTUI_CAL_END_UNTIL)
		until = r->until;
	format_day(until, v->until, sizeof v->until);
}

/* A whole number in 1..max, spaces around it allowed. */
static bool parse_number(const char *s, long max, long *out)
{
	while (*s == ' ')
		s++;
	long n = 0;
	int digits = 0;
	for (; *s >= '0' && *s <= '9'; s++, digits++) {
		n = n * 10 + (*s - '0');
		if (n > max)
			return false;
	}
	while (*s == ' ')
		s++;
	if (digits == 0 || *s != '\0' || n < 1)
		return false;
	*out = n;
	return true;
}

static bool parse_day(const char *s, long *day)
{
	ctui_date d;
	if (!ctui_dateinput_parse(s, &d))
		return false;
	*day = ctui_date_to_days(d.year, d.month, d.day);
	return true;
}

/* The rule the values make, from `base` (for the exceptions). */
static ctui_cal_recur_error build(const ctui_cal_recur_values *v, long *start,
				  ctui_cal_recur *out,
				  const ctui_cal_recur *base)
{
	long s, n, until = 0;
	if (!parse_day(v->start, &s))
		return CTUI_CAL_RECUR_BAD_START;
	ctui_cal_recur r = base ? *base : (ctui_cal_recur){0};
	int repeat = clamp(v->repeat, 0, 4);
	if (repeat == CTUI_CAL_REPEAT_NONE) {
		r = (ctui_cal_recur){0};
	} else {
		if (!parse_number(v->interval, CTUI_CAL_RECUR_INTERVAL_MAX, &n))
			return CTUI_CAL_RECUR_BAD_INTERVAL;
		r.repeat = (ctui_cal_repeat)repeat;
		r.interval = (int)n;
		r.weekdays = 0;
		if (repeat == CTUI_CAL_REPEAT_WEEKLY) {
			for (int i = 0; i < 7; i++)
				r.weekdays |=
				    v->weekdays[i] ? 1U << (unsigned)i : 0U;
			if (!r.weekdays)
				return CTUI_CAL_RECUR_NO_WEEKDAY;
		}
		r.end = (ctui_cal_end)clamp(v->end, 0, 2);
		r.until = 0;
		r.count = 0;
		if (r.end == CTUI_CAL_END_UNTIL) {
			if (!parse_day(v->until, &until))
				return CTUI_CAL_RECUR_BAD_UNTIL;
			if (until < s)
				return CTUI_CAL_RECUR_UNTIL_BEFORE;
			r.until = until;
		} else if (r.end == CTUI_CAL_END_COUNT) {
			if (!parse_number(v->count, CTUI_CAL_RECUR_COUNT_MAX,
					  &n))
				return CTUI_CAL_RECUR_BAD_COUNT;
			r.count = (int)n;
		}
	}
	if (start)
		*start = s;
	if (out)
		*out = r;
	return CTUI_CAL_RECUR_OK;
}

ctui_cal_recur_error ctui_cal_recur_values_check(const ctui_cal_recur_values *v)
{
	return build(v, NULL, NULL, NULL);
}

ctui_cal_recur_error ctui_cal_recur_values_apply(const ctui_cal_recur_values *v,
						 long *start_day,
						 ctui_cal_recur *r)
{
	long s = 0;
	ctui_cal_recur n;
	ctui_cal_recur_error e = build(v, &s, &n, r);
	if (e != CTUI_CAL_RECUR_OK)
		return e;
	if (n.exception_count < 0)
		n.exception_count = 0;
	if (n.exception_count > CTUI_CAL_EXCEPTIONS)
		n.exception_count = CTUI_CAL_EXCEPTIONS;
	if (start_day)
		*start_day = s;
	if (r)
		*r = n;
	return CTUI_CAL_RECUR_OK;
}

const char *ctui_cal_recur_error_text(ctui_cal_recur_error e)
{
	switch (e) {
	case CTUI_CAL_RECUR_BAD_START:
		return "Start date: type a real date as YYYY-MM-DD";
	case CTUI_CAL_RECUR_BAD_INTERVAL:
		return "Every: type a whole number from 1 to 10000";
	case CTUI_CAL_RECUR_NO_WEEKDAY:
		return "Weekly: choose at least one weekday";
	case CTUI_CAL_RECUR_BAD_UNTIL:
		return "Until date: type a real date as YYYY-MM-DD";
	case CTUI_CAL_RECUR_UNTIL_BEFORE:
		return "Until date is before the start date";
	case CTUI_CAL_RECUR_BAD_COUNT:
		return "Occurrences: type a whole number from 1 to 1000000";
	default:
		return "";
	}
}

bool ctui_cal_recur_values_preview(const ctui_cal_recur_values *v, char *out,
				   size_t cap)
{
	long start;
	ctui_cal_recur r;
	if (cap == 0)
		return false;
	out[0] = '\0';
	if (build(v, &start, &r, NULL) != CTUI_CAL_RECUR_OK)
		return false;
	if (r.repeat == CTUI_CAL_REPEAT_NONE) {
		snprintf(out, cap, "Does not repeat");
		return true;
	}
	ctui_cal_event e = {start, 0, 60, NULL, 0};
	long last = start + 36600, max = ctui_dateinput_max_days();
	long days[3];
	int n = ctui_cal_event_expand(&e, &r, start, last > max ? max : last,
				      days, 3);
	if (n > 3)
		n = 3;
	size_t used = (size_t)snprintf(out, cap, "Next:");
	for (int i = 0; i < n && used < cap; i++) {
		char text[CTUI_DATEINPUT_TEXT];
		format_day(days[i], text, sizeof text);
		used += (size_t)snprintf(out + used, cap - used, "%s %s",
					 i ? "," : "", text);
	}
	if (n == 0)
		snprintf(out, cap, "No occurrence");
	return true;
}

/* ---- the dialog ---- */

static void sync(ctui_cal_recur_dialog *d)
{
	ctui_cal_recur_values *v = &d->values;
	ctui_field *f = d->fields;
	// NOLINTNEXTLINE(clang-analyzer-core.NullDereference)
	v->repeat = (int)f[F_REPEAT].choice;
	for (int i = 0; i < 7; i++)
		v->weekdays[i] = f[F_DAY0 + i].checked;
	v->end = (int)f[F_END].choice;
	bool repeats = v->repeat != CTUI_CAL_REPEAT_NONE;
	f[F_EVERY].disabled = !repeats;
	for (int i = 0; i < 7; i++)
		f[F_DAY0 + i].disabled = v->repeat != CTUI_CAL_REPEAT_WEEKLY;
	f[F_END].disabled = !repeats;
	f[F_UNTIL].disabled = !repeats || v->end != CTUI_CAL_END_UNTIL;
	f[F_COUNT].disabled = !repeats || v->end != CTUI_CAL_END_COUNT;
	d->error = ctui_cal_recur_values_check(v);
}

void ctui_cal_recur_dialog_init(ctui_cal_recur_dialog *d, long start_day,
				const ctui_cal_recur *r)
{
	*d = (ctui_cal_recur_dialog){0};
	ctui_cal_recur_values_from(&d->values, start_day, r);
	ctui_cal_recur_values *v = &d->values;
	ctui_field *f = d->fields;
	f[F_REPEAT] =
	    ctui_field_select("Repeat", REPEATS, 5, (size_t)v->repeat);
	f[F_EVERY] = ctui_field_text("Every", v->interval, sizeof v->interval);
	for (int i = 0; i < 7; i++)
		f[F_DAY0 + i] = ctui_field_check(DAYS[i], NULL, v->weekdays[i]);
	f[F_START] = ctui_field_text("Start date", v->start, sizeof v->start);
	f[F_END] = ctui_field_select("Ends", ENDS, 3, (size_t)v->end);
	f[F_UNTIL] = ctui_field_text("Until date", v->until, sizeof v->until);
	f[F_COUNT] = ctui_field_text("Occurrences", v->count, sizeof v->count);
	ctui_form_init(&d->form, f, CTUI_CAL_RECUR_FIELDS);
	d->form.dialog = true;
	sync(d);
	ctui_form_init(&d->form, f,
		       CTUI_CAL_RECUR_FIELDS); /* the first enabled field */
}

bool ctui_cal_recur_dialog_editing(const ctui_cal_recur_dialog *d)
{
	(void)d;
	return true;
}

ctui_dialog_result ctui_cal_recur_dialog_key(ctui_cal_recur_dialog *d, int key)
{
	ctui_form_event e = ctui_form_key(&d->form, key);
	sync(d);
	if (e == CTUI_FORM_CANCELLED)
		return CTUI_DIALOG_CANCELLED;
	if (e == CTUI_FORM_SUBMITTED)
		return d->error == CTUI_CAL_RECUR_OK ? CTUI_DIALOG_ACCEPTED
						     : CTUI_DIALOG_OPEN;
	return CTUI_DIALOG_OPEN;
}

ctui_cal_recur_error
ctui_cal_recur_dialog_result(const ctui_cal_recur_dialog *d, long *start_day,
			     ctui_cal_recur *r)
{
	return ctui_cal_recur_values_apply(&d->values, start_day, r);
}

/* ---- layout, click and drawing ---- */

#define BOX_WIDTH 56
#define BOX_HEIGHT                                                             \
	(CTUI_CAL_RECUR_FIELDS +                                               \
	 5) /* the fields, a message, a preview, the buttons, the frame */

/* The captions of the buttons, "[ OK ]" and "[ Cancel ]" by default. */
static void captions(const ctui_cal_recur_dialog *d, char *ok, size_t okcap,
		     char *cancel, size_t cancelcap)
{
	ctui_labels labels = ctui_labels_resolve(d ? d->labels : NULL);
	snprintf(ok, okcap, "[ %s ]", d && d->ok ? d->ok : "OK");
	snprintf(cancel, cancelcap, "[ %s ]",
		 labels.cancel ? labels.cancel : "Cancel");
}

bool ctui_cal_recur_dialog_layout_for(const ctui_cal_recur_dialog *d, int lines,
				      int cols,
				      ctui_cal_recur_dialog_layout *out)
{
	char ok[40], cancel[40];
	captions(d, ok, sizeof ok, cancel, sizeof cancel);
	int ok_width = (int)ctui_utf8_columns(ok),
	    cancel_width = (int)ctui_utf8_columns(cancel);
	ctui_rect box;
	if (!ctui_dialog_box(lines, cols, BOX_WIDTH, BOX_HEIGHT, &box) ||
	    box.width < 40 || box.height < 10)
		return false;
	int formh = box.height - 5;
	int by = box.y + box.height - 2;
	int bx = box.x + (box.width - (ok_width + 3 + cancel_width)) / 2;
	if (bx < box.x + 1)
		bx = box.x + 1;
	out->box = box;
	out->form = (ctui_rect){box.y + 1, box.x + 2, formh, box.width - 4};
	out->error_y = box.y + box.height - 4;
	out->preview_y = box.y + box.height - 3;
	out->ok = (ctui_rect){by, bx, 1, ok_width};
	out->cancel = (ctui_rect){by, bx + ok_width + 3, 1, cancel_width};
	return true;
}

static bool inside(const ctui_rect *r, int y, int x)
{
	return y >= r->y && y < r->y + r->height && x >= r->x &&
	       x < r->x + r->width;
}

ctui_dialog_result ctui_cal_recur_dialog_click(ctui_cal_recur_dialog *d,
					       int lines, int cols, int y,
					       int x)
{
	ctui_cal_recur_dialog_layout l;
	if (!ctui_cal_recur_dialog_layout_for(d, lines, cols, &l))
		return CTUI_DIALOG_OPEN;
	if (ctui_frame_close_hit(l.box.y, l.box.x, l.box.width, y, x) ||
	    inside(&l.cancel, y, x))
		return CTUI_DIALOG_CANCELLED;
	if (inside(&l.ok, y, x))
		return d->error == CTUI_CAL_RECUR_OK ? CTUI_DIALOG_ACCEPTED
						     : CTUI_DIALOG_OPEN;
	if (inside(&l.form, y, x)) {
		ctui_form_click(&d->form, l.form.width, l.form.height,
				y - l.form.y, x - l.form.x);
		sync(d);
	}
	return CTUI_DIALOG_OPEN;
}

void ctui_cal_recur_dialog_draw(const ctui_cal_recur_dialog *d)
{
	ctui_cal_recur_dialog_layout l;
	int text = ctui_attr(CTUI_PAIR_DIALOG, false);
	if (!ctui_cal_recur_dialog_layout_for(d, LINES, COLS, &l)) {
		ctui_rect box;
		if (ctui_dialog_box(LINES, COLS, 30, 3, &box)) {
			ctui_frame_draw(box.y, box.x, box.height, box.width,
					true, true,
					d->title ? d->title : "Repeat");
			ctui_put(box.y + 1, box.x + 2, box.width - 4,
				 "Resize the terminal", text);
		}
		return;
	}
	ctui_frame_draw(l.box.y, l.box.x, l.box.height, l.box.width, true, true,
			d->title ? d->title : "Repeat");
	ctui_frame_close_draw(l.box.y, l.box.x, l.box.width, true);
	ctui_form_draw(&d->form, l.form.y, l.form.x, l.form.width,
		       l.form.height);
	int inner = l.box.width - 4;
	if (d->error != CTUI_CAL_RECUR_OK) {
		ctui_put(l.error_y, l.box.x + 2, inner,
			 ctui_cal_recur_error_text(d->error),
			 ctui_attr(CTUI_PAIR_NOTICE_ERROR, true));
	} else {
		char line[96];
		if (ctui_cal_recur_values_preview(&d->values, line,
						  sizeof line))
			ctui_put(
			    l.preview_y, l.box.x + 2, inner, line,
			    (int)((unsigned)ctui_attr(CTUI_PAIR_DIALOG, false) |
				  (unsigned)A_DIM));
	}
	char ok[40], cancel[40];
	captions(d, ok, sizeof ok, cancel, sizeof cancel);
	int on = ctui_attr(CTUI_PAIR_SELECTION, true);
	ctui_put(l.ok.y, l.ok.x, l.ok.width, ok,
		 d->error == CTUI_CAL_RECUR_OK
		     ? on
		     : (int)((unsigned)text | (unsigned)A_DIM));
	ctui_put(l.cancel.y, l.cancel.x, l.cancel.width, cancel, text);
}
