/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * meter.c - audio level meter (meter.h).
 */
#include <ctui/meter.h>

#include <curses.h>
#include <math.h>

#include <ctui/text.h>

#define KEY_STEP_DB 1.0
#define KEY_PAGE_DB 6.0

double ctui_meter_db(double linear)
{
	double a = fabs(linear);
	if (!(a > 0.0)) /* zero or NaN */
		return -INFINITY;
	return 20.0 * log10(a);
}

double ctui_meter_linear(double db)
{
	return pow(10.0, db / 20.0);
}

double ctui_meter_fraction(double db)
{
	if (!(db > CTUI_METER_FLOOR_DB))
		return 0.0;
	if (db >= CTUI_METER_CLIP_DB)
		return 1.0;
	return (db - CTUI_METER_FLOOR_DB) /
	       (CTUI_METER_CLIP_DB - CTUI_METER_FLOOR_DB);
}

int ctui_meter_cells(double db, int length)
{
	if (length < 1)
		return 0;
	return (int)(ctui_meter_fraction(db) * length + 0.5);
}

int ctui_meter_cell(double db, int length)
{
	if (length < 1)
		return -1;
	int cell = (int)(ctui_meter_fraction(db) * length);
	return cell >= length ? length - 1 : cell;
}

double ctui_meter_cell_db(int cell, int length)
{
	if (length < 1)
		return CTUI_METER_FLOOR_DB;
	if (cell < 0)
		cell = 0;
	if (cell > length - 1)
		cell = length - 1;
	return CTUI_METER_FLOOR_DB +
	       ((double)cell + 0.5) / length *
		   (CTUI_METER_CLIP_DB - CTUI_METER_FLOOR_DB);
}

ctui_meter_zone ctui_meter_zone_of(double db)
{
	return db >= CTUI_METER_RED_DB	    ? CTUI_METER_RED
	       : db >= CTUI_METER_YELLOW_DB ? CTUI_METER_YELLOW
					    : CTUI_METER_GREEN;
}

ctui_pair ctui_meter_zone_pair(ctui_meter_zone zone, bool bright)
{
	switch (zone) {
	case CTUI_METER_RED:
		return bright ? CTUI_PAIR_GAUGE_RED_BRIGHT
			      : CTUI_PAIR_GAUGE_RED;
	case CTUI_METER_YELLOW:
		return bright ? CTUI_PAIR_GAUGE_YELLOW_BRIGHT
			      : CTUI_PAIR_GAUGE_YELLOW;
	default:
		return bright ? CTUI_PAIR_GAUGE_GREEN_BRIGHT
			      : CTUI_PAIR_GAUGE_GREEN;
	}
}

int ctui_meter_scale_length(int length)
{
	return length < 2 ? 0 : length - 1;
}

void ctui_meter_peak_update(ctui_meter_peak *peak, double level, double dt,
			    double hold, double decay)
{
	if (!(dt > 0.0))
		dt = 0.0;
	if (level >= peak->db) {
		peak->db = level;
		peak->held = 0.0;
		return;
	}
	peak->held += dt;
	double falling = peak->held - hold; /* seconds spent falling so far */
	if (falling > 0.0) {
		peak->db -= decay * (falling < dt ? falling : dt);
		if (peak->db < level)
			peak->db = level;
	}
}

void ctui_meter_init(ctui_meter *m, bool vertical)
{
	*m = (ctui_meter){0};
	m->vertical = vertical;
	m->level = CTUI_METER_FLOOR_DB;
	m->peak.db = CTUI_METER_FLOOR_DB;
	m->hold = CTUI_METER_HOLD;
	m->decay = CTUI_METER_DECAY;
	m->show_peak = true;
}

void ctui_meter_set_db(ctui_meter *m, double db, double dt)
{
	if (!(db > CTUI_METER_FLOOR_DB))
		db = CTUI_METER_FLOOR_DB;
	if (db > CTUI_METER_MAX_DB)
		db = CTUI_METER_MAX_DB;
	m->level = db;
	ctui_meter_peak_update(&m->peak, db, dt, m->hold, m->decay);
	if (db >= CTUI_METER_CLIP_DB)
		m->clip = true;
}

void ctui_meter_set_linear(ctui_meter *m, double linear, double dt)
{
	ctui_meter_set_db(m, ctui_meter_db(linear), dt);
}

void ctui_meter_clear(ctui_meter *m)
{
	m->peak.db = m->level;
	m->peak.held = 0.0;
	m->clip = false;
}

void ctui_meter_place(ctui_meter *m, int y, int x, int width, int height)
{
	m->y = y;
	m->x = x;
	m->width = width > 0 ? width : 0;
	m->height = height > 0 ? height : 0;
}

void ctui_meter_place_pair(ctui_meter *left, ctui_meter *right, int y, int x,
			   int width, int height)
{
	bool vertical = left->vertical;
	right->vertical = vertical;
	int thick = vertical ? width : height;
	int gap = thick >= 3 ? 1 : 0;
	int first = (thick - gap + 1) / 2, second = thick - gap - first;
	if (second < 0)
		second = 0;
	if (vertical) {
		ctui_meter_place(left, y, x, first, height);
		ctui_meter_place(right, y, x + first + gap, second, height);
	} else {
		ctui_meter_place(left, y, x, width, first);
		ctui_meter_place(right, y + first + gap, x, width, second);
	}
}

bool ctui_meter_key(ctui_meter *m, int key)
{
	double step;
	switch (key) {
	case KEY_UP:
	case KEY_RIGHT:
		step = KEY_STEP_DB;
		break;
	case KEY_DOWN:
	case KEY_LEFT:
		step = -KEY_STEP_DB;
		break;
	case KEY_PPAGE:
		step = KEY_PAGE_DB;
		break;
	case KEY_NPAGE:
		step = -KEY_PAGE_DB;
		break;
	case KEY_HOME:
		ctui_meter_set_db(m, CTUI_METER_FLOOR_DB, 0.0);
		return true;
	case KEY_END:
		ctui_meter_set_db(m, CTUI_METER_CLIP_DB, 0.0);
		return true;
	case 'c':
		ctui_meter_clear(m);
		return true;
	default:
		return false;
	}
	ctui_meter_set_db(m, m->level + step, 0.0);
	return true;
}

bool ctui_meter_over(const ctui_meter *m, int y, int x)
{
	return y >= m->y && y < m->y + m->height && x >= m->x &&
	       x < m->x + m->width;
}

/* Cell under a screen position along the length, 0 = the floor end; may be
 * outside 0 .. scale (the scale cell count is the clip indicator). */
static int cell_at(const ctui_meter *m, int y, int x)
{
	return m->vertical ? m->y + m->height - 1 - y : x - m->x;
}

static void set_from_cell(ctui_meter *m, int cell)
{
	int scale = ctui_meter_scale_length(m->vertical ? m->height : m->width);
	if (scale < 1)
		return;
	if (cell < 0) {
		ctui_meter_set_db(m, CTUI_METER_FLOOR_DB, 0.0);
		return;
	}
	ctui_meter_set_db(m, ctui_meter_cell_db(cell, scale), 0.0);
}

bool ctui_meter_press(ctui_meter *m, int y, int x)
{
	if (!ctui_meter_over(m, y, x))
		return false;
	int scale = ctui_meter_scale_length(m->vertical ? m->height : m->width);
	int cell = cell_at(m, y, x);
	if (scale < 1)
		return true;
	if (cell >= scale) { /* the clip indicator */
		m->clip = false;
		return true;
	}
	m->dragging = true;
	set_from_cell(m, cell);
	return true;
}

bool ctui_meter_drag(ctui_meter *m, int y, int x)
{
	if (!m->dragging)
		return false;
	set_from_cell(m, cell_at(m, y, x));
	return true;
}

bool ctui_meter_release(ctui_meter *m)
{
	bool was = m->dragging;
	m->dragging = false;
	return was;
}

bool ctui_meter_wheel(ctui_meter *m, int y, int x, int rows)
{
	if (!ctui_meter_over(m, y, x))
		return false;
	ctui_meter_set_db(m, m->level + rows, 0.0);
	return true;
}

static void draw_impl(const ctui_meter *m)
{
	const ctui_glyphs *g = ctui_theme_glyphs();
	int length = m->vertical ? m->height : m->width;
	int thick = m->vertical ? m->width : m->height;
	int scale = ctui_meter_scale_length(length);
	if (scale < 1 || thick < 1)
		return;
	int lit = ctui_meter_cells(m->level, scale);
	int peak = m->show_peak && m->peak.db > CTUI_METER_FLOOR_DB
		       ? ctui_meter_cell(m->peak.db, scale)
		       : -1;
	const char *marker = m->vertical ? g->box_h : g->box_v;
	for (int i = 0; i <= scale; i++) {
		const char *glyph;
		int attr;
		if (i == scale) { /* the clip indicator */
			glyph = m->clip ? g->gauge_filled : g->gauge_empty;
			attr = (int)((unsigned)ctui_attr(
					 ctui_meter_zone_pair(CTUI_METER_RED,
							      m->clip),
					 m->clip) |
				     (m->clip ? 0U : (unsigned)A_DIM));
		} else {
			ctui_meter_zone zone =
			    ctui_meter_zone_of(ctui_meter_cell_db(i, scale));
			if (i == peak) {
				glyph = marker;
				attr = ctui_attr(
				    ctui_meter_zone_pair(zone, true), true);
			} else if (i < lit) {
				glyph = g->gauge_filled;
				attr = ctui_attr(
				    ctui_meter_zone_pair(zone, true), false);
			} else {
				glyph = g->gauge_empty;
				attr =
				    (int)((unsigned)ctui_attr(
					      ctui_meter_zone_pair(zone, false),
					      false) |
					  (unsigned)A_DIM);
			}
		}
		for (int t = 0; t < thick; t++) {
			if (m->vertical)
				ctui_put(m->y + length - 1 - i, m->x + t, 1,
					 glyph, attr);
			else
				ctui_put(m->y + t, m->x + i, 1, glyph, attr);
		}
	}
}

void ctui_meter_draw(const ctui_meter *m)
{
	ctui_style_begin(m->style);
	draw_impl(m);
	ctui_style_end();
}
