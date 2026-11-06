/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * slider.c - value mapping, pointer handling and drawing of the fader.
 */
#include <ctui/slider.h>

#include <curses.h>
#include <math.h>
#include <stdio.h>

#include <ctui/text.h>

#define PAGE_STEPS 10
#define MAX_DECIMALS 4

static double effective_step(const ctui_slider *s)
{
	return s->step > 0 ? s->step : (s->max - s->min) / 100.0;
}

void ctui_slider_init(ctui_slider *s, const char *label, double min, double max,
		      double step, double value)
{
	*s = (ctui_slider){0};
	if (max < min) {
		double t = min;
		min = max;
		max = t;
	}
	s->label = label;
	s->min = min;
	s->max = max;
	s->step = step > 0 ? step : 0;
	double eff = effective_step(s);
	double scale = 1;
	while (s->decimals < MAX_DECIMALS && eff > 0 &&
	       fabs(eff * scale - round(eff * scale)) > 1e-9) {
		s->decimals++;
		scale *= 10;
	}
	s->value = ctui_slider_snap(s, value);
}

double ctui_slider_snap(const ctui_slider *s, double v)
{
	if (!(v == v)) /* NaN */
		return s->min;
	if (s->step > 0) {
		double on_grid =
		    s->min + round((v - s->min) / s->step) * s->step;
		/* the maximum is reachable even when the step does not divide
		 * the range */
		v = fabs(v - s->max) < fabs(v - on_grid) ? s->max : on_grid;
	}
	if (v < s->min)
		v = s->min;
	if (v > s->max)
		v = s->max;
	return v;
}

bool ctui_slider_set(ctui_slider *s, double v)
{
	v = ctui_slider_snap(s, v);
	bool changed = v != s->value;
	s->value = v;
	return changed;
}

double ctui_slider_ratio(const ctui_slider *s)
{
	double span = s->max - s->min;
	if (!(span > 0))
		return 0;
	double r = (s->value - s->min) / span;
	return r < 0 ? 0 : r > 1 ? 1 : r;
}

/* Cell of a ratio on a track, in screen order. */
static int cell_of(const ctui_slider *s, double ratio, int track)
{
	int cell = (int)lround(ratio * (track - 1));
	return s->vertical ? track - 1 - cell : cell;
}

int ctui_slider_pos(const ctui_slider *s, int track)
{
	return track < 1 ? -1 : cell_of(s, ctui_slider_ratio(s), track);
}

double ctui_slider_value_at(const ctui_slider *s, int track, int cell)
{
	if (track < 2)
		return s->min;
	if (cell < 0)
		cell = 0;
	if (cell > track - 1)
		cell = track - 1;
	double r = (double)cell / (track - 1);
	if (s->vertical)
		r = 1 - r;
	return ctui_slider_snap(s, s->min + r * (s->max - s->min));
}

char *ctui_slider_text(const ctui_slider *s, double v, char *buf, size_t cap)
{
	if (fabs(v) < 0.5 * pow(10, -s->decimals))
		v = 0;
	snprintf(buf, cap, "%.*f", s->decimals, v);
	return buf;
}

void ctui_slider_place(ctui_slider *s, int y, int x, int width, int height)
{
	s->y = y;
	s->x = x;
	s->width = width;
	s->height = height;
}

static int text_columns(const char *text)
{
	return text ? (int)ctui_utf8_columns(text) : 0;
}

/* Columns of the label (horizontal) and of the value text. */
static int label_columns(const ctui_slider *s)
{
	int cols = text_columns(s->label), cap = s->width / 3;
	return cols > cap ? cap : cols;
}

static int value_columns(const ctui_slider *s)
{
	char a[40], b[40];
	int wa = text_columns(ctui_slider_text(s, s->min, a, sizeof a));
	int wb = text_columns(ctui_slider_text(s, s->max, b, sizeof b));
	return wa > wb ? wa : wb;
}

bool ctui_slider_track(const ctui_slider *s, int *y, int *x, int *length)
{
	int ty, tx, len;
	if (s->vertical) {
		len = s->height - 1 - (s->label && *s->label ? 1 : 0);
		ty = s->y + 1;
		tx = s->x + s->width / 2;
	} else {
		int lead = s->label && *s->label ? label_columns(s) + 1 : 0;
		len = s->width - lead - 1 - value_columns(s);
		ty = s->y;
		tx = s->x + lead;
	}
	if (s->width < 1 || s->height < 1 || len < 2)
		return false;
	if (y)
		*y = ty;
	if (x)
		*x = tx;
	if (length)
		*length = len;
	return true;
}

bool ctui_slider_over(const ctui_slider *s, int y, int x)
{
	int ty, tx, len;
	if (!ctui_slider_track(s, &ty, &tx, &len))
		return false;
	if (s->vertical)
		return y >= ty && y < ty + len && x >= s->x &&
		       x < s->x + s->width;
	return y == ty && x >= tx && x < tx + len;
}

/* Value under a screen position, clamped to the ends of the track. */
static bool value_under(const ctui_slider *s, int y, int x, double *value)
{
	int ty, tx, len;
	if (!ctui_slider_track(s, &ty, &tx, &len))
		return false;
	*value = ctui_slider_value_at(s, len, s->vertical ? y - ty : x - tx);
	return true;
}

bool ctui_slider_key(ctui_slider *s, int key)
{
	double step = effective_step(s);
	switch (key) {
	case KEY_LEFT:
	case KEY_DOWN:
		ctui_slider_set(s, s->value - step);
		return true;
	case KEY_RIGHT:
	case KEY_UP:
		ctui_slider_set(s, s->value + step);
		return true;
	case KEY_PPAGE:
		ctui_slider_set(s, s->value + PAGE_STEPS * step);
		return true;
	case KEY_NPAGE:
		ctui_slider_set(s, s->value - PAGE_STEPS * step);
		return true;
	case KEY_HOME:
		ctui_slider_set(s, s->min);
		return true;
	case KEY_END:
		ctui_slider_set(s, s->max);
		return true;
	default:
		return false;
	}
}

bool ctui_slider_press(ctui_slider *s, int y, int x)
{
	double v;
	if (!ctui_slider_over(s, y, x) || !value_under(s, y, x, &v))
		return false;
	s->dragging = true;
	ctui_slider_set(s, v);
	return true;
}

bool ctui_slider_drag(ctui_slider *s, int y, int x)
{
	double v;
	if (!s->dragging || !value_under(s, y, x, &v))
		return false;
	return ctui_slider_set(s, v);
}

bool ctui_slider_release(ctui_slider *s)
{
	bool was = s->dragging;
	s->dragging = false;
	return was;
}

bool ctui_slider_wheel(ctui_slider *s, int notches)
{
	return ctui_slider_set(s, s->value + notches * effective_step(s));
}

/* Draw text centred in width columns at (y, x). */
static void put_centred(int y, int x, int width, const char *text, int attr)
{
	int cols = text_columns(text);
	int pad = cols < width ? (width - cols) / 2 : 0;
	ctui_put(y, x + pad, width - pad, text, attr);
}

static void draw_track(const ctui_slider *s, bool focused)
{
	int ty, tx, len;
	if (!ctui_slider_track(s, &ty, &tx, &len))
		return;
	const ctui_glyphs *g = ctui_theme_glyphs();
	const char *fill = s->vertical ? g->scroll_thumb : g->heavy;
	const char *empty = s->vertical ? g->box_v : g->rule;
	const char *mark = s->vertical ? g->box_h : g->box_v;
	int knob = ctui_slider_pos(s, len);
	int centre = cell_of(s, 0.5, len);
	int anchor = s->centre ? centre : cell_of(s, 0, len);
	int lo = anchor < knob ? anchor : knob,
	    hi = anchor < knob ? knob : anchor;
	for (int i = 0; i < len; i++) {
		const char *glyph = empty;
		int attr = ctui_attr(CTUI_PAIR_MUTED, false);
		if (i == knob) {
			glyph = g->knob;
			attr = ctui_attr(focused ? CTUI_PAIR_CURSOR_SELECTED
						 : CTUI_PAIR_CURSOR,
					 true);
		} else if (s->centre && i == centre) {
			glyph = mark; /* stays visible under the fill */
			attr = ctui_attr(CTUI_PAIR_BODY, false);
		} else if (i >= lo && i <= hi) {
			glyph = fill;
			attr = ctui_attr(CTUI_PAIR_CURSOR, false);
		}
		ctui_put(s->vertical ? ty + i : ty, s->vertical ? tx : tx + i,
			 1, glyph, attr);
	}
}

static void draw_impl(const ctui_slider *s, bool focused)
{
	int body = ctui_attr(CTUI_PAIR_BODY, false);
	int label_attr =
	    ctui_attr(focused ? CTUI_PAIR_HEADING : CTUI_PAIR_MUTED, focused);
	char value[40];
	ctui_slider_text(s, s->value, value, sizeof value);
	bool label = s->label && *s->label;
	for (int r = 0; r < s->height; r++)
		ctui_fill(s->y + r, s->x, s->width, body);
	if (s->vertical) {
		put_centred(s->y, s->x, s->width, value, body);
		if (label && s->height >= 2)
			put_centred(s->y + s->height - 1, s->x, s->width,
				    s->label, label_attr);
	} else {
		if (label)
			ctui_put(s->y, s->x, label_columns(s), s->label,
				 label_attr);
		int vw = value_columns(s), cols = text_columns(value);
		if (s->width > vw)
			ctui_put(s->y, s->x + s->width - vw + (vw - cols), vw,
				 value, body);
	}
	draw_track(s, focused);
}

void ctui_slider_draw(const ctui_slider *s, bool focused)
{
	ctui_style_begin(s->style);
	draw_impl(s, focused);
	ctui_style_end();
}
