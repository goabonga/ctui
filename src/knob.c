/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * knob.c - a rotary knob (knob.h).
 */
#include <ctui/knob.h>

#include <curses.h>
#include <math.h>
#include <stdio.h>

#include <ctui/text.h>
#include <ctui/theme.h>

#define EPS 1e-9

/* The ring, clockwise from the bottom left: up the left side, across the
 * top, down the right side. */
static const unsigned char CELLS[CTUI_KNOB_POSITIONS][2] = {
    {2, 0}, {1, 0}, {0, 0}, {0, 1}, {0, 2}, {0, 3}, {0, 4}, {1, 4}, {2, 4},
};

const ctui_knob_glyphs *ctui_knob_glyph_set(bool unicode)
{
	static const ctui_knob_glyphs UNI = {"\xe2\x97\x8f", "\xc2\xb7",
					     "\xe2\x97\x89", "\xe2\x97\x87"};
	static const ctui_knob_glyphs ASCII = {"o", ".", "O", "+"};
	return unicode ? &UNI : &ASCII;
}

void ctui_knob_init(ctui_knob *k, const char *label, double min, double max,
		    double step, double value)
{
	if (!(max > min))
		max = min + 1;
	if (!(step > 0))
		step = (max - min) / 100;
	*k = (ctui_knob){.label = label, .min = min, .max = max, .step = step};
	double s = step;
	while (k->decimals < 4 && fabs(s - round(s)) > EPS) {
		s *= 10;
		k->decimals++;
	}
	ctui_knob_set(k, value);
	k->def = k->value;
}

double ctui_knob_centre(const ctui_knob *k)
{
	return (k->min + k->max) / 2;
}

static double clamp(const ctui_knob *k, double v)
{
	if (!(v > k->min)) /* also catches NaN */
		return k->min;
	return v > k->max ? k->max : v;
}

/* The grid starts at the minimum, or at the centre of a bipolar knob. */
static double origin(const ctui_knob *k)
{
	return k->bipolar ? ctui_knob_centre(k) : k->min;
}

static double snap(const ctui_knob *k, double v)
{
	double o = origin(k);
	return clamp(k, o + round((v - o) / k->step) * k->step);
}

double ctui_knob_value_at(const ctui_knob *k, double ratio)
{
	if (!(ratio > 0))
		ratio = 0;
	if (ratio > 1)
		ratio = 1;
	return snap(k, k->min + ratio * (k->max - k->min));
}

double ctui_knob_ratio(const ctui_knob *k)
{
	return (clamp(k, k->value) - k->min) / (k->max - k->min);
}

int ctui_knob_position(const ctui_knob *k)
{
	return (int)lround(ctui_knob_ratio(k) * (CTUI_KNOB_POSITIONS - 1));
}

double ctui_knob_position_value(const ctui_knob *k, int position)
{
	if (position < 0)
		position = 0;
	if (position > CTUI_KNOB_POSITIONS - 1)
		position = CTUI_KNOB_POSITIONS - 1;
	return ctui_knob_value_at(k,
				  (double)position / (CTUI_KNOB_POSITIONS - 1));
}

bool ctui_knob_cell(int position, int *row, int *col)
{
	if (position < 0 || position >= CTUI_KNOB_POSITIONS)
		return false;
	*row = CELLS[position][0];
	*col = CELLS[position][1];
	return true;
}

bool ctui_knob_set(ctui_knob *k, double value)
{
	double v = clamp(k, value);
	bool changed = fabs(v - k->value) > EPS * k->step;
	k->value = v;
	return changed;
}

bool ctui_knob_reset(ctui_knob *k)
{
	return ctui_knob_set(k, k->def);
}

char *ctui_knob_format(const ctui_knob *k, char *buf, size_t cap)
{
	double v = k->value;
	if (fabs(v) < 0.5 * pow(10, -k->decimals)) /* no "-0" */
		v = 0;
	snprintf(buf, cap, k->bipolar ? "%+.*f%s" : "%.*f%s", k->decimals, v,
		 k->unit ? k->unit : "");
	return buf;
}

/* Move by count grid points of size grid in a direction, from the nearest
 * grid point in that direction (so an off-grid value lands on the grid). */
static bool step_by(ctui_knob *k, int dir, double grid, int count)
{
	double o = origin(k), q = (k->value - o) / grid;
	double n = dir > 0 ? floor(q + EPS) + count : ceil(q - EPS) - count;
	return ctui_knob_set(k, o + n * grid);
}

bool ctui_knob_key(ctui_knob *k, int key)
{
	switch (key) {
	case KEY_RIGHT:
	case KEY_UP:
		step_by(k, 1, k->step, 1);
		break;
	case KEY_LEFT:
	case KEY_DOWN:
		step_by(k, -1, k->step, 1);
		break;
	case KEY_PPAGE:
		step_by(k, 1, k->step, 10);
		break;
	case KEY_NPAGE:
		step_by(k, -1, k->step, 10);
		break;
	case KEY_SRIGHT:
	case KEY_SR:
		step_by(k, 1, k->step / 10, 1);
		break;
	case KEY_SLEFT:
	case KEY_SF:
		step_by(k, -1, k->step / 10, 1);
		break;
	case KEY_HOME:
		ctui_knob_set(k, k->min);
		break;
	case KEY_END:
		ctui_knob_set(k, k->max);
		break;
	case 'r':
	case KEY_DC:
		ctui_knob_reset(k);
		break;
	default:
		return false;
	}
	return true;
}

bool ctui_knob_wheel(ctui_knob *k, int notches)
{
	if (notches == 0)
		return false;
	return step_by(k, notches > 0 ? 1 : -1, k->step,
		       notches > 0 ? notches : -notches);
}

static int dial_x(int width)
{
	return width > CTUI_KNOB_WIDTH ? (width - CTUI_KNOB_WIDTH) / 2 : 0;
}

bool ctui_knob_hit(const ctui_knob *k, int width, int y, int x)
{
	(void)k;
	int x0 = dial_x(width);
	return y >= 0 && y < CTUI_KNOB_HEIGHT && x >= x0 &&
	       x < x0 + CTUI_KNOB_WIDTH;
}

bool ctui_knob_press(ctui_knob *k, int width, int y, int x)
{
	if (!ctui_knob_hit(k, width, y, x))
		return false;
	k->dragging = true;
	k->drag_y = y;
	k->drag_from = k->value;
	return true;
}

bool ctui_knob_drag(ctui_knob *k, int y)
{
	if (!k->dragging)
		return false;
	double travelled =
	    (double)(k->drag_y - y) * (k->max - k->min) / CTUI_KNOB_DRAG_ROWS;
	return ctui_knob_set(k, snap(k, k->drag_from + travelled));
}

bool ctui_knob_release(ctui_knob *k)
{
	bool was = k->dragging;
	k->dragging = false;
	return was;
}

/* Text centred in width columns at row y. */
static void centred(int y, int x, int width, const char *text, int attr)
{
	int cols = (int)ctui_utf8_columns(text);
	if (cols > width)
		cols = width;
	ctui_put(y, x + (width - cols) / 2, width, text, attr);
}

static void knob_draw_impl(const ctui_knob *k, int y, int x, int width,
			   bool focused)
{
	if (width < 1)
		return;
	const ctui_knob_glyphs *g = ctui_knob_glyph_set(ctui_theme_unicode());
	int x0 = x + dial_x(width);
	int pos = ctui_knob_position(k);
	int centre = (CTUI_KNOB_POSITIONS - 1) / 2;
	int lit = ctui_attr(CTUI_PAIR_BORDER_SELECTED, false),
	    unlit = ctui_attr(CTUI_PAIR_GAUGE_EMPTY, false);
	int lo = k->bipolar && pos > centre ? centre : k->bipolar ? pos : 0;
	int hi = k->bipolar && pos > centre ? pos : k->bipolar ? centre : pos;
	for (int i = 0; i < CTUI_KNOB_POSITIONS; i++) {
		int row, col;
		ctui_knob_cell(i, &row, &col);
		const char *glyph = g->unlit;
		int attr = unlit;
		if (i == pos) {
			glyph = g->pointer;
			attr = ctui_attr(CTUI_PAIR_CURSOR, true);
		} else if (i >= lo && i <= hi && !(k->bipolar && i == centre)) {
			glyph = g->lit;
			attr = lit;
		} else if (k->bipolar && i == centre) {
			glyph = g->detent;
			attr = ctui_attr(CTUI_PAIR_MUTED, false);
		}
		ctui_put(y + row, x0 + col, 1, glyph, attr);
	}
	if (k->label && *k->label)
		centred(y + 3, x, width, k->label,
			focused ? ctui_attr(CTUI_PAIR_HEADING, true)
				: ctui_attr(CTUI_PAIR_MUTED, false));
	char text[64];
	centred(y + 4, x, width, ctui_knob_format(k, text, sizeof text),
		ctui_attr(CTUI_PAIR_BODY, false));
}

void ctui_knob_draw(const ctui_knob *k, int y, int x, int width, bool focused)
{
	ctui_style_begin(k->style);
	knob_draw_impl(k, y, x, width, focused);
	ctui_style_end();
}
