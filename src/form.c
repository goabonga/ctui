/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * form.c - labelled form fields (form.h).
 */
#include <ctui/form.h>

#include <curses.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include <ctui/text.h>
#include <ctui/theme.h>

#define LABEL_GAP 2
#define MIN_CONTROL 8
#define VALUE_WIDTH 8 /* columns right of a slider track */
#define DROPDOWN_ROWS 8
#define PAGE_STEPS 10
#define NUMBER_WIDTH 20 /* columns of a number field, at most */

/* ---- fields ----------------------------------------------------- */

ctui_field ctui_field_text(const char *label, char *buffer, size_t capacity)
{
	if (capacity)
		buffer[capacity - 1] = '\0';
	ctui_field f = {.kind = CTUI_FIELD_TEXT,
			.label = label,
			.text = buffer,
			.capacity = capacity};
	ctui_input_init(&f.input, buffer, capacity);
	return f;
}

ctui_field ctui_field_number(const char *label, double min, double max,
			     double step, double value, int decimals)
{
	ctui_field f = {.kind = CTUI_FIELD_NUMBER, .label = label};
	ctui_numinput_init(&f.number, min, max, step, value, decimals);
	return f;
}

ctui_field ctui_field_check(const char *label, const char *caption,
			    bool checked)
{
	return (ctui_field){.kind = CTUI_FIELD_CHECK,
			    .label = label,
			    .caption = caption,
			    .checked = checked};
}

ctui_field ctui_field_select(const char *label, const char *const *options,
			     size_t count, size_t choice)
{
	return (ctui_field){.kind = CTUI_FIELD_SELECT,
			    .label = label,
			    .options = options,
			    .option_count = count,
			    .choice = choice < count ? choice : 0};
}

ctui_field ctui_field_slider(const char *label, double min, double max,
			     double step, double value, int decimals)
{
	if (max < min)
		max = min;
	if (step <= 0)
		step = (max - min) / 20;
	if (value < min)
		value = min;
	if (value > max)
		value = max;
	return (ctui_field){.kind = CTUI_FIELD_SLIDER,
			    .label = label,
			    .min = min,
			    .max = max,
			    .step = step,
			    .value = value,
			    .decimals = decimals < 0 ? 0 : decimals};
}

ctui_field ctui_field_list(const char *label, const char *const *options,
			   size_t count, size_t choice, int rows)
{
	ctui_field f = {.kind = CTUI_FIELD_LIST,
			.label = label,
			.options = options,
			.option_count = count,
			.choice = choice < count ? choice : 0,
			.rows = rows < 1 ? 1 : rows};
	if (f.choice >= (size_t)f.rows)
		f.first = f.choice - (size_t)f.rows + 1;
	return f;
}

ctui_field ctui_field_date(const char *label, int year, int month, int day)
{
	ctui_field f = {.kind = CTUI_FIELD_DATE, .label = label};
	ctui_dateinput_init(&f.date, year, month, day);
	return f;
}

ctui_field ctui_field_time(const char *label, ctui_time_precision precision,
			   long long micro)
{
	ctui_field f = {.kind = CTUI_FIELD_TIME, .label = label};
	ctui_timeinput_init(&f.time, precision, micro);
	return f;
}

ctui_field ctui_field_duration(const char *label, ctui_duration_unit largest,
			       ctui_duration_unit smallest, long long micro)
{
	ctui_field f = {.kind = CTUI_FIELD_DURATION, .label = label};
	ctui_durationinput_init(&f.duration, largest, smallest, micro);
	return f;
}

static bool is_input(const ctui_field *f)
{
	return f->kind == CTUI_FIELD_DATE || f->kind == CTUI_FIELD_TIME ||
	       f->kind == CTUI_FIELD_DURATION || f->kind == CTUI_FIELD_NUMBER;
}

static int field_rows(const ctui_field *f)
{
	return f->kind == CTUI_FIELD_LIST ? (f->rows < 1 ? 1 : f->rows) : 1;
}

/* Keep the choice of a list inside its window. */
static void list_scroll(ctui_field *f)
{
	size_t rows = (size_t)field_rows(f);
	if (f->choice < f->first)
		f->first = f->choice;
	else if (f->choice >= f->first + rows)
		f->first = f->choice - rows + 1;
}

/* ---- form ------------------------------------------------------- */

void ctui_form_init(ctui_form *f, ctui_field *fields, size_t count)
{
	*f = (ctui_form){.fields = fields, .count = count};
	for (size_t i = 0; i < count; i++)
		if (!fields[i].disabled) {
			f->focus = i;
			break;
		}
}

bool ctui_form_editing(const ctui_form *f)
{
	if (f->count == 0)
		return false;
	const ctui_field *fld = &f->fields[f->focus];
	return f->dropdown || fld->kind == CTUI_FIELD_TEXT || is_input(fld);
}

int ctui_form_rows(const ctui_form *f)
{
	int rows = 0;
	for (size_t i = 0; i < f->count; i++)
		rows += field_rows(&f->fields[i]);
	return rows;
}

static int label_width(const ctui_form *f, int width)
{
	int w = 0;
	for (size_t i = 0; i < f->count; i++) {
		int cols = (int)ctui_utf8_columns(f->fields[i].label);
		if (cols > w)
			w = cols;
	}
	return w > width / 2 - 1 ? width / 2 - 1 : w;
}

bool ctui_form_slot(const ctui_form *f, size_t i, int width, ctui_rect *label,
		    ctui_rect *control)
{
	if (i >= f->count)
		return false;
	int lw = label_width(f, width);
	int cx = lw + LABEL_GAP;
	if (width - cx < MIN_CONTROL)
		return false;
	int y = 0;
	for (size_t k = 0; k < i; k++)
		y += field_rows(&f->fields[k]);
	int rows = field_rows(&f->fields[i]);
	*label = (ctui_rect){y, 0, 1, lw};
	*control = (ctui_rect){y, cx, rows, width - cx};
	return true;
}

/* Rows the form is scrolled by so that the focused field is visible. */
static int scroll_of(const ctui_form *f, int height)
{
	if (f->count == 0)
		return 0;
	int top = 0;
	for (size_t k = 0; k < f->focus; k++)
		top += field_rows(&f->fields[k]);
	int bottom = top + field_rows(&f->fields[f->focus]);
	int scroll = bottom > height ? bottom - height : 0;
	return scroll > top ? top : scroll;
}

/* ---- values ----------------------------------------------------- */

static double snap(const ctui_field *f, double v)
{
	if (v < f->min)
		v = f->min;
	if (v > f->max)
		v = f->max;
	if (f->step > 0) {
		v = f->min + round((v - f->min) / f->step) * f->step;
		if (v > f->max)
			v = f->max;
	}
	return v;
}

static ctui_form_event slider_to(ctui_field *f, double v)
{
	double before = f->value;
	f->value = snap(f, v);
	return f->value != before ? CTUI_FORM_CHANGED : CTUI_FORM_NONE;
}

static ctui_form_event choose(ctui_field *f, size_t choice)
{
	if (f->option_count == 0)
		return CTUI_FORM_NONE;
	if (choice >= f->option_count)
		choice = f->option_count - 1;
	size_t before = f->choice;
	f->choice = choice;
	if (f->kind == CTUI_FIELD_LIST)
		list_scroll(f);
	return f->choice != before ? CTUI_FORM_CHANGED : CTUI_FORM_NONE;
}

/* Bring the editor of a text field up to date with the settings of the
 * field, which the caller may change after the constructor. */
static ctui_input *text_input(const ctui_field *f)
{
	// NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
	ctui_input *in = (ctui_input *)&f->input;
	in->text = f->text;
	in->capacity = f->capacity;
	in->mask = f->mask;
	in->max_chars = f->max_chars;
	in->placeholder = f->placeholder;
	if (in->rule != f->rule)
		ctui_input_set_rule(in, f->rule);
	return in;
}

static ctui_form_event edit_text(ctui_field *f, int key)
{
	if (!f->text || f->capacity == 0)
		return CTUI_FORM_NONE;
	switch (ctui_input_key(text_input(f), key)) {
	case CTUI_INPUT_CHANGED:
		return CTUI_FORM_CHANGED;
	case CTUI_INPUT_MOVED:
		return CTUI_FORM_MOVED;
	case CTUI_INPUT_SUBMITTED:
		return CTUI_FORM_SUBMITTED;
	default:
		return CTUI_FORM_NONE;
	}
}

/* The focus leaves a field: a number being typed becomes its value. */
static void commit(ctui_field *f)
{
	if (f->kind == CTUI_FIELD_NUMBER)
		ctui_numinput_commit(&f->number);
}

static ctui_form_event move_focus(ctui_form *f, int direction)
{
	for (long i = (long)f->focus + direction; i >= 0 && i < (long)f->count;
	     i += direction)
		if (!f->fields[i].disabled) {
			commit(&f->fields[f->focus]);
			f->focus = (size_t)i;
			f->dropdown = false;
			return CTUI_FORM_MOVED;
		}
	return CTUI_FORM_NONE;
}

static ctui_form_event dropdown_key(ctui_form *f, ctui_field *fld, int key)
{
	switch (key) {
	case 27:
		f->dropdown = false;
		return CTUI_FORM_MOVED;
	case KEY_UP:
		if (f->pending > 0)
			f->pending--;
		return CTUI_FORM_MOVED;
	case KEY_DOWN:
		if (f->pending + 1 < fld->option_count)
			f->pending++;
		return CTUI_FORM_MOVED;
	case KEY_HOME:
		f->pending = 0;
		return CTUI_FORM_MOVED;
	case KEY_END:
		f->pending = fld->option_count ? fld->option_count - 1 : 0;
		return CTUI_FORM_MOVED;
	case '\n':
	case KEY_ENTER:
	case ' ': {
		f->dropdown = false;
		ctui_form_event e = choose(fld, f->pending);
		return e == CTUI_FORM_NONE ? CTUI_FORM_MOVED : e;
	}
	default:
		return CTUI_FORM_NONE;
	}
}

static ctui_form_event list_key(ctui_form *f, ctui_field *fld, int key)
{
	size_t last = fld->option_count ? fld->option_count - 1 : 0;
	size_t page = (size_t)field_rows(fld);
	switch (key) {
	case KEY_UP:
		return fld->choice == 0 ? move_focus(f, -1)
					: choose(fld, fld->choice - 1);
	case KEY_DOWN:
		return fld->choice >= last ? move_focus(f, 1)
					   : choose(fld, fld->choice + 1);
	case KEY_HOME:
		return choose(fld, 0);
	case KEY_END:
		return choose(fld, last);
	case KEY_PPAGE:
		return choose(fld, fld->choice > page ? fld->choice - page : 0);
	case KEY_NPAGE:
		return choose(fld, fld->choice + page);
	default:
		return CTUI_FORM_NONE;
	}
}

static ctui_form_event slider_key(ctui_field *fld, int key)
{
	switch (key) {
	case KEY_LEFT:
		return slider_to(fld, fld->value - fld->step);
	case KEY_RIGHT:
		return slider_to(fld, fld->value + fld->step);
	case KEY_PPAGE:
		return slider_to(fld, fld->value - PAGE_STEPS * fld->step);
	case KEY_NPAGE:
		return slider_to(fld, fld->value + PAGE_STEPS * fld->step);
	case KEY_HOME:
		return slider_to(fld, fld->min);
	case KEY_END:
		return slider_to(fld, fld->max);
	default:
		return CTUI_FORM_NONE;
	}
}

static ctui_form_event date_event(ctui_dateinput_event e)
{
	switch (e) {
	case CTUI_DATEINPUT_CHANGED:
		return CTUI_FORM_CHANGED;
	case CTUI_DATEINPUT_NONE:
		return CTUI_FORM_NONE;
	default:
		return CTUI_FORM_MOVED;
	}
}

static ctui_form_event time_event(ctui_timeinput_event e)
{
	switch (e) {
	case CTUI_TIMEINPUT_CHANGED:
		return CTUI_FORM_CHANGED;
	case CTUI_TIMEINPUT_MOVED:
		return CTUI_FORM_MOVED;
	case CTUI_TIMEINPUT_SUBMITTED:
		return CTUI_FORM_SUBMITTED;
	case CTUI_TIMEINPUT_CANCELLED:
		return CTUI_FORM_CANCELLED;
	default:
		return CTUI_FORM_NONE;
	}
}

static ctui_form_event duration_event(ctui_durationinput_event e)
{
	switch (e) {
	case CTUI_DURATIONINPUT_CHANGED:
		return CTUI_FORM_CHANGED;
	case CTUI_DURATIONINPUT_MOVED:
		return CTUI_FORM_MOVED;
	case CTUI_DURATIONINPUT_SUBMITTED:
		return CTUI_FORM_SUBMITTED;
	case CTUI_DURATIONINPUT_CANCELLED:
		return CTUI_FORM_CANCELLED;
	default:
		return CTUI_FORM_NONE;
	}
}

/* PgUp / PgDn step the value (Up / Down move between the fields), and + / -
 * too except in a date, where '-' separates the segments. */
static int step_of(int key, bool date)
{
	if (key == KEY_PPAGE || (!date && key == '+'))
		return 1;
	if (key == KEY_NPAGE || (!date && key == '-'))
		return -1;
	return 0;
}

static ctui_form_event number_key(ctui_field *fld, int key)
{
	/* PgUp / PgDn add or subtract one step (Up / Down move between fields)
	 */
	if (key == KEY_PPAGE)
		key = KEY_UP;
	else if (key == KEY_NPAGE)
		key = KEY_DOWN;
	switch (ctui_numinput_key(&fld->number, key)) {
	case CTUI_NUMINPUT_CHANGED:
		return CTUI_FORM_CHANGED;
	case CTUI_NUMINPUT_SUBMITTED:
		return CTUI_FORM_SUBMITTED;
	case CTUI_NUMINPUT_CANCELLED:
		return CTUI_FORM_CANCELLED;
	default:
		return CTUI_FORM_NONE;
	}
}

static ctui_form_event input_key(ctui_field *fld, int key)
{
	if (fld->kind == CTUI_FIELD_NUMBER)
		return number_key(fld, key);
	int step = step_of(key, fld->kind == CTUI_FIELD_DATE);
	if (fld->kind == CTUI_FIELD_DATE) {
		if (step)
			return ctui_dateinput_step(&fld->date, step)
				   ? CTUI_FORM_CHANGED
				   : CTUI_FORM_NONE;
		if (key == KEY_UP || key == KEY_DOWN)
			return CTUI_FORM_NONE;
		return date_event(ctui_dateinput_key(&fld->date, key));
	}
	if (fld->kind == CTUI_FIELD_TIME) {
		if (step)
			return ctui_timeinput_step(&fld->time, step)
				   ? CTUI_FORM_CHANGED
				   : CTUI_FORM_NONE;
		return time_event(ctui_timeinput_key(&fld->time, key));
	}
	if (step)
		return ctui_durationinput_step(&fld->duration, step)
			   ? CTUI_FORM_CHANGED
			   : CTUI_FORM_NONE;
	return duration_event(ctui_durationinput_key(&fld->duration, key));
}

ctui_form_event ctui_form_key(ctui_form *f, int key)
{
	if (f->count == 0)
		return CTUI_FORM_NONE;
	ctui_field *fld = &f->fields[f->focus];
	if (f->dropdown)
		return dropdown_key(f, fld, key);
	if (fld->kind == CTUI_FIELD_DATE && fld->date.open)
		return date_event(ctui_dateinput_key(&fld->date, key));
	if (key == 27)
		return CTUI_FORM_CANCELLED;
	if (key == KEY_BTAB)
		return move_focus(f, -1);
	if (key == '\t')
		return move_focus(f, 1);

	if (fld->kind == CTUI_FIELD_LIST) {
		if (key == '\n' || key == KEY_ENTER)
			return CTUI_FORM_SUBMITTED;
		return list_key(f, fld, key);
	}
	if (key == KEY_UP)
		return move_focus(f, -1);
	if (key == KEY_DOWN)
		return move_focus(f, 1);

	switch (fld->kind) {
	case CTUI_FIELD_TEXT:
		if (key == '\n' || key == KEY_ENTER)
			return CTUI_FORM_SUBMITTED;
		return edit_text(fld, key);
	case CTUI_FIELD_CHECK:
		if (key == ' ') {
			fld->checked = !fld->checked;
			return CTUI_FORM_CHANGED;
		}
		return key == '\n' || key == KEY_ENTER ? CTUI_FORM_SUBMITTED
						       : CTUI_FORM_NONE;
	case CTUI_FIELD_SELECT:
		if (key == '\n' || key == KEY_ENTER || key == ' ') {
			if (fld->option_count == 0)
				return CTUI_FORM_NONE;
			f->dropdown = true;
			f->pending = fld->choice;
			return CTUI_FORM_MOVED;
		}
		if (key == KEY_LEFT)
			return fld->choice > 0 ? choose(fld, fld->choice - 1)
					       : CTUI_FORM_NONE;
		if (key == KEY_RIGHT)
			return choose(fld, fld->choice + 1);
		return CTUI_FORM_NONE;
	case CTUI_FIELD_SLIDER:
		if (key == '\n' || key == KEY_ENTER)
			return CTUI_FORM_SUBMITTED;
		return slider_key(fld, key);
	case CTUI_FIELD_DATE:
	case CTUI_FIELD_TIME:
	case CTUI_FIELD_DURATION:
	case CTUI_FIELD_NUMBER:
		return input_key(fld, key);
	default:
		return CTUI_FORM_NONE;
	}
}

/* ---- dropdown geometry ------------------------------------------ */

/* Where the open dropdown of the focused select goes, relative to the
 * form area: under the control, or above it when there is no room. */
static bool dropdown_box(const ctui_form *f, int width, int height,
			 ctui_rect *box)
{
	ctui_rect label, control;
	if (!ctui_form_slot(f, f->focus, width, &label, &control))
		return false;
	int rows = (int)f->fields[f->focus].option_count;
	if (rows > DROPDOWN_ROWS)
		rows = DROPDOWN_ROWS;
	if (rows > height - 3) /* the frame takes two rows, the field one */
		rows = height - 3;
	if (rows < 1)
		return false;
	int tall = rows + 2;
	int y = control.y - scroll_of(f, height) + 1;
	if (y + tall > height)
		y = y - 1 - tall;
	if (y < 0)
		y = 0;
	*box = (ctui_rect){y, control.x, tall, control.width};
	return true;
}

static size_t dropdown_first(const ctui_form *f, int rows)
{
	size_t r = (size_t)rows;
	return f->pending >= r ? f->pending - r + 1 : 0;
}

/* ---- mouse ------------------------------------------------------ */

ctui_form_event ctui_form_click(ctui_form *f, int width, int height, int y,
				int x)
{
	if (f->count == 0)
		return CTUI_FORM_NONE;
	if (f->dropdown) {
		ctui_rect box;
		bool in_box = dropdown_box(f, width, height, &box) &&
			      y >= box.y && y < box.y + box.height &&
			      x >= box.x && x < box.x + box.width;
		if (in_box && y > box.y && y < box.y + box.height - 1) {
			f->pending = dropdown_first(f, box.height - 2) +
				     (size_t)(y - box.y - 1);
			return dropdown_key(f, &f->fields[f->focus], '\n');
		}
		if (in_box)
			return CTUI_FORM_NONE; /* the frame */
		f->dropdown = false;
	}
	int scroll = scroll_of(f, height);
	ctui_field *focused = &f->fields[f->focus];
	if (focused->kind == CTUI_FIELD_DATE && focused->date.open) {
		/* the picker hangs under the field: its clicks, or the one
		 * that closes it */
		ctui_rect label, control;
		if (!ctui_form_slot(f, f->focus, width, &label, &control))
			return CTUI_FORM_NONE;
		ctui_dateinput_event e = ctui_dateinput_click(
		    &focused->date, y - (label.y - scroll), x - control.x);
		if (e != CTUI_DATEINPUT_CLOSED)
			return date_event(e);
	}
	for (size_t i = 0; i < f->count; i++) {
		ctui_rect label, control;
		if (!ctui_form_slot(f, i, width, &label, &control))
			return CTUI_FORM_NONE;
		int top = label.y - scroll;
		if (y < top || y >= top + control.height)
			continue;
		ctui_field *fld = &f->fields[i];
		if (fld->disabled)
			return CTUI_FORM_NONE;
		ctui_form_event event = CTUI_FORM_NONE;
		if (f->focus != i) {
			commit(&f->fields[f->focus]);
			f->focus = i;
			event = CTUI_FORM_MOVED;
		}
		if (x < control.x)
			return event;
		switch (fld->kind) {
		case CTUI_FIELD_CHECK:
			fld->checked = !fld->checked;
			return CTUI_FORM_CHANGED;
		case CTUI_FIELD_SELECT:
			if (fld->option_count) {
				f->dropdown = true;
				f->pending = fld->choice;
				return CTUI_FORM_MOVED;
			}
			return event;
		case CTUI_FIELD_SLIDER: {
			int track = control.width - VALUE_WIDTH;
			if (track < 3 || fld->max <= fld->min)
				return event;
			int p = x - control.x;
			if (p > track - 1)
				p = track - 1;
			ctui_form_event e =
			    slider_to(fld, fld->min + (fld->max - fld->min) *
							  p / (track - 1));
			return e == CTUI_FORM_NONE ? event : e;
		}
		case CTUI_FIELD_LIST: {
			ctui_form_event e =
			    choose(fld, fld->first + (size_t)(y - top));
			return e == CTUI_FORM_NONE ? event : e;
		}
		case CTUI_FIELD_TEXT:
			if (fld->text && fld->capacity &&
			    ctui_input_click(text_input(fld), control.width,
					     x - control.x))
				return CTUI_FORM_MOVED;
			return event;
		case CTUI_FIELD_NUMBER:
			if (ctui_numinput_click(
				&fld->number,
				control.width < NUMBER_WIDTH ? control.width
							     : NUMBER_WIDTH,
				0, x - control.x) == CTUI_NUMINPUT_CHANGED)
				return CTUI_FORM_CHANGED;
			return event;
		case CTUI_FIELD_DATE: {
			ctui_form_event e = date_event(
			    ctui_dateinput_click(&fld->date, 0, x - control.x));
			return e == CTUI_FORM_NONE ? event : e;
		}
		case CTUI_FIELD_TIME: {
			ctui_form_event e = time_event(
			    ctui_timeinput_click(&fld->time, 0, x - control.x));
			return e == CTUI_FORM_NONE ? event : e;
		}
		case CTUI_FIELD_DURATION: {
			ctui_form_event e =
			    duration_event(ctui_durationinput_click(
				&fld->duration, 0, x - control.x));
			return e == CTUI_FORM_NONE ? event : e;
		}
		default:
			return event;
		}
	}
	return CTUI_FORM_NONE;
}

/* ---- drawing ---------------------------------------------------- */

typedef struct paint {
	int y, x, width, height; /* the form area on screen */
	int scroll;
	bool dialog; /* drawn on the dialog background */
} paint;

/* On the page background fields are inset on the bar colour; on a dialog
 * (which is the bar colour) it is the other way round. */
static int field_attr(const paint *p, bool focused)
{
	if (focused)
		return ctui_attr(CTUI_PAIR_SELECTION, true);
	return ctui_attr(p->dialog ? CTUI_PAIR_BODY : CTUI_PAIR_DIALOG, false);
}

static int plain_attr(const paint *p, bool focused)
{
	if (focused)
		return ctui_attr(CTUI_PAIR_SELECTION, true);
	return ctui_attr(p->dialog ? CTUI_PAIR_DIALOG : CTUI_PAIR_BODY, false);
}

static int muted_attr(const paint *p)
{
	return p->dialog ? (int)((unsigned)ctui_attr(CTUI_PAIR_DIALOG, false) |
				 (unsigned)A_DIM)
			 : ctui_attr(CTUI_PAIR_MUTED, false);
}

static bool row_visible(const paint *p, int row)
{
	return row >= 0 && row < p->height;
}

/* On a dialog the fields are inset in the body colour, which the input
 * components get by taking it as their dialog colour. */
static void inset_begin(const paint *p, ctui_style *inset)
{
	*inset = (ctui_style){0};
	if (p->dialog)
		inset->attr[CTUI_PAIR_DIALOG] =
		    ctui_attr(CTUI_PAIR_BODY, false);
	ctui_style_begin(p->dialog ? inset : NULL);
}

static void draw_text(const paint *p, const ctui_field *fld, ctui_rect c,
		      bool focused)
{
	ctui_style inset;
	inset_begin(p, &inset);
	ctui_input_draw(text_input(fld), p->y + c.y - p->scroll, p->x + c.x,
			c.width, focused);
	ctui_style_end();
}

static void draw_number(const paint *p, const ctui_field *fld, ctui_rect c,
			bool focused)
{
	ctui_style inset;
	inset_begin(p, &inset);
	ctui_numinput_draw(&fld->number, p->y + c.y - p->scroll, p->x + c.x,
			   c.width < NUMBER_WIDTH ? c.width : NUMBER_WIDTH,
			   focused);
	ctui_style_end();
}

static void draw_check(const paint *p, const ctui_field *fld, ctui_rect c,
		       bool focused)
{
	int row = c.y - p->scroll;
	int attr = plain_attr(p, focused);
	if (focused)
		ctui_fill(p->y + row, p->x + c.x, c.width, attr);
	ctui_put(p->y + row, p->x + c.x + 1, 3, fld->checked ? "[x]" : "[ ]",
		 attr);
	if (fld->caption)
		ctui_put(p->y + row, p->x + c.x + 5, c.width - 6, fld->caption,
			 attr);
}

static void draw_select(const paint *p, const ctui_field *fld, ctui_rect c,
			bool focused, bool open)
{
	int row = c.y - p->scroll;
	int attr = field_attr(p, focused);
	ctui_fill(p->y + row, p->x + c.x, c.width, attr);
	if (fld->choice < fld->option_count)
		ctui_put(p->y + row, p->x + c.x + 1, c.width - 4,
			 fld->options[fld->choice], attr);
	const ctui_glyphs *g = ctui_theme_glyphs();
	const char *arrow = open ? g->select_open : g->select_shut;
	ctui_put(p->y + row, p->x + c.x + c.width - 2, 1, arrow, attr);
}

static void draw_slider(const paint *p, const ctui_field *fld, ctui_rect c,
			bool focused)
{
	int row = p->y + c.y - p->scroll;
	const ctui_glyphs *g = ctui_theme_glyphs();
	int track = c.width - VALUE_WIDTH;
	char value[32];
	snprintf(value, sizeof value, "%.*f", fld->decimals, fld->value);
	int vattr = plain_attr(p, focused);
	if (track >= 3) {
		double span = fld->max - fld->min;
		int knob = span > 0 ? (int)lround((fld->value - fld->min) /
						  span * (track - 1))
				    : 0;
		for (int i = 0; i < track; i++) {
			const char *glyph;
			int attr;
			if (i < knob) {
				glyph = g->heavy;
				attr = ctui_attr(CTUI_PAIR_CURSOR, false);
			} else if (i == knob) {
				glyph = g->knob;
				attr = ctui_attr(focused
						     ? CTUI_PAIR_CURSOR_SELECTED
						     : CTUI_PAIR_CURSOR,
						 true);
			} else {
				glyph = g->rule;
				attr = ctui_attr(CTUI_PAIR_MUTED, false);
			}
			ctui_put(row, p->x + c.x + i, 1, glyph, attr);
		}
	}
	int vx = p->x + c.x + (track >= 3 ? track : 0);
	int vw = c.width - (track >= 3 ? track : 0);
	ctui_fill(row, vx, vw, vattr);
	int cols = (int)ctui_utf8_columns(value);
	ctui_put(row, vx + (vw > cols + 1 ? vw - cols - 1 : 0), vw, value,
		 vattr);
}

static void draw_list(const paint *p, const ctui_field *fld, ctui_rect c,
		      bool focused)
{
	for (int r = 0; r < c.height; r++) {
		int row = c.y - p->scroll + r;
		if (!row_visible(p, row))
			continue;
		size_t i = fld->first + (size_t)r;
		bool chosen = i == fld->choice;
		int attr = chosen && focused ? field_attr(p, true)
					     : field_attr(p, false);
		if (chosen)
			attr = (int)((unsigned)attr | (unsigned)A_BOLD);
		ctui_fill(p->y + row, p->x + c.x, c.width, attr);
		if (i < fld->option_count)
			ctui_put(p->y + row, p->x + c.x + 1, c.width - 2,
				 fld->options[i], attr);
	}
}

/* The input drawn by its own component when the control is wide enough, else
 * its text cut to the control. */
static void draw_input(const paint *p, const ctui_field *fld, ctui_rect c,
		       bool focused)
{
	int y = p->y + c.y - p->scroll, x = p->x + c.x;
	char text[CTUI_DURATION_TEXT];
	const char *shown = NULL;
	int needed;
	switch (fld->kind) {
	case CTUI_FIELD_DATE:
		needed = CTUI_DATEINPUT_WIDTH;
		shown = ctui_dateinput_format(ctui_dateinput_date(&fld->date),
					      text, sizeof text);
		break;
	case CTUI_FIELD_TIME:
		needed = ctui_timeinput_width(
		    (ctui_time_precision)fld->time.precision);
		shown = ctui_timeinput_format(
		    fld->time.micro, (ctui_time_precision)fld->time.precision,
		    text, sizeof text);
		break;
	default:
		needed = ctui_durationinput_width(&fld->duration);
		shown = ctui_durationinput_format(
		    fld->duration.micro,
		    (ctui_duration_unit)fld->duration.largest,
		    (ctui_duration_unit)fld->duration.smallest,
		    fld->duration.digits, text, sizeof text);
		break;
	}
	if (c.width < needed) {
		int attr = field_attr(p, focused);
		ctui_fill(y, x, c.width, attr);
		if (shown)
			ctui_put(y, x + 1, c.width - 2, shown, attr);
		return;
	}
	ctui_style inset;
	inset_begin(p, &inset);
	if (fld->kind == CTUI_FIELD_DATE)
		ctui_dateinput_draw(&fld->date, y, x, focused);
	else if (fld->kind == CTUI_FIELD_TIME)
		ctui_timeinput_draw(&fld->time, y, x, focused);
	else
		ctui_durationinput_draw(&fld->duration, y, x, focused);
	ctui_style_end();
}

/* A rounded outline on the field background, without changing colours. */
static void draw_border(const paint *p, int y, int x, int height, int width)
{
	const ctui_glyphs *g = ctui_theme_glyphs();
	int attr = ctui_attr(
	    p->dialog ? CTUI_PAIR_BORDER : CTUI_PAIR_DIALOG_BORDER, false);
	for (int col = x + 1; col < x + width - 1; col++) {
		ctui_put(y, col, 1, g->box_h, attr);
		ctui_put(y + height - 1, col, 1, g->box_h, attr);
	}
	for (int row = y + 1; row < y + height - 1; row++) {
		ctui_put(row, x, 1, g->box_v, attr);
		ctui_put(row, x + width - 1, 1, g->box_v, attr);
	}
	ctui_put(y, x, 1, g->box_tl, attr);
	ctui_put(y, x + width - 1, 1, g->box_tr, attr);
	ctui_put(y + height - 1, x, 1, g->box_bl, attr);
	ctui_put(y + height - 1, x + width - 1, 1, g->box_br, attr);
}

static void form_draw_impl(const ctui_form *f, int y, int x, int width,
			   int height)
{
	paint p = {y, x, width, height, scroll_of(f, height), f->dialog};
	for (size_t i = 0; i < f->count; i++) {
		const ctui_field *fld = &f->fields[i];
		ctui_rect label, control;
		if (!ctui_form_slot(f, i, width, &label, &control))
			return;
		bool focused = i == f->focus && !f->blurred;
		int row = label.y - p.scroll;
		if (row_visible(&p, row)) {
			int attr = fld->disabled ? muted_attr(&p)
				   : p.dialog
				       ? ctui_attr(CTUI_PAIR_DIALOG, focused)
				       : ctui_attr(CTUI_PAIR_HEADING, focused);
			ctui_put(y + row, x + label.x, label.width, fld->label,
				 attr);
		}
		if (fld->kind == CTUI_FIELD_LIST) {
			draw_list(&p, fld, control, focused);
			continue;
		}
		if (!row_visible(&p, row))
			continue;
		if (fld->disabled) {
			ctui_put(y + row, x + control.x + 1, control.width - 1,
				 "-", muted_attr(&p));
			continue;
		}
		switch (fld->kind) {
		case CTUI_FIELD_TEXT:
			draw_text(&p, fld, control, focused);
			break;
		case CTUI_FIELD_CHECK:
			draw_check(&p, fld, control, focused);
			break;
		case CTUI_FIELD_SELECT:
			draw_select(&p, fld, control, focused,
				    i == f->focus && f->dropdown);
			break;
		case CTUI_FIELD_SLIDER:
			draw_slider(&p, fld, control, focused);
			break;
		case CTUI_FIELD_NUMBER:
			draw_number(&p, fld, control, focused);
			break;
		case CTUI_FIELD_DATE:
		case CTUI_FIELD_TIME:
		case CTUI_FIELD_DURATION:
			draw_input(&p, fld, control, focused);
			break;
		default:
			break;
		}
	}
	/* the month picker of the focused date, over the fields under it */
	const ctui_field *date = &f->fields[f->focus];
	ctui_rect label, control;
	if (f->count && date->kind == CTUI_FIELD_DATE && date->date.open &&
	    !date->disabled &&
	    ctui_form_slot(f, f->focus, width, &label, &control) &&
	    control.width >= CTUI_DATEINPUT_WIDTH &&
	    row_visible(&p, label.y - p.scroll))
		ctui_dateinput_draw(&date->date, y + label.y - p.scroll,
				    x + control.x, true);
	ctui_rect box;
	if (f->dropdown && dropdown_box(f, width, height, &box)) {
		const ctui_field *fld = &f->fields[f->focus];
		int rows = box.height - 2;
		size_t first = dropdown_first(f, rows);
		/* A border sets the list apart from the fields around and under
		 * it; inside it keeps the colours of the select it belongs to.
		 */
		draw_border(&p, y + box.y, x + box.x, box.height, box.width);
		for (int r = 0; r < rows; r++) {
			size_t i = first + (size_t)r;
			bool current = i == f->pending;
			int attr = field_attr(&p, current);
			ctui_fill(y + box.y + 1 + r, x + box.x + 1,
				  box.width - 2, attr);
			if (i < fld->option_count)
				ctui_put(y + box.y + 1 + r, x + box.x + 2,
					 box.width - 4, fld->options[i], attr);
		}
	}
}

void ctui_form_draw(const ctui_form *f, int y, int x, int width, int height)
{
	ctui_style_begin(f->style);
	form_draw_impl(f, y, x, width, height);
	ctui_style_end();
}
