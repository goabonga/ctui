/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * stepseq.c - a step sequencer grid (stepseq.h).
 */
#include <ctui/stepseq.h>

#include <curses.h>
#include <stdio.h>

#include <ctui/text.h>
#include <ctui/theme.h>

#define CELL 2 /* columns of a cell: the glyph and a space */

/* Off, then the four velocity levels. */
static const char *const GLYPHS_UNICODE[5] = {
    "\xc2\xb7", "\xe2\x96\x82", "\xe2\x96\x84", "\xe2\x96\x86", "\xe2\x96\x88"};
static const char *const GLYPHS_ASCII[5] = {".", "-", "=", "+", "#"};

uint32_t ctui_stepseq_bit_mask(unsigned steps)
{
	if (steps >= CTUI_STEPSEQ_MAX_STEPS)
		return 0xFFFFFFFFu;
	return ((uint32_t)1 << steps) - 1;
}

bool ctui_stepseq_bit_test(uint32_t row, unsigned step)
{
	return step < CTUI_STEPSEQ_MAX_STEPS && (row >> step) & 1u;
}

uint32_t ctui_stepseq_bit_set(uint32_t row, unsigned step, bool on)
{
	if (step >= CTUI_STEPSEQ_MAX_STEPS)
		return row;
	return on ? row | ((uint32_t)1 << step) : row & ~((uint32_t)1 << step);
}

uint32_t ctui_stepseq_bit_toggle(uint32_t row, unsigned step)
{
	return ctui_stepseq_bit_set(row, step,
				    !ctui_stepseq_bit_test(row, step));
}

unsigned ctui_stepseq_bit_count(uint32_t row, unsigned steps)
{
	unsigned n = 0;
	for (row &= ctui_stepseq_bit_mask(steps); row; row &= row - 1)
		n++;
	return n;
}

uint32_t ctui_stepseq_bit_rotate(uint32_t row, unsigned steps, int by)
{
	if (steps > CTUI_STEPSEQ_MAX_STEPS)
		steps = CTUI_STEPSEQ_MAX_STEPS;
	uint32_t mask = ctui_stepseq_bit_mask(steps);
	row &= mask;
	if (steps < 2)
		return row;
	unsigned n = ctui_stepseq_wrap(by, steps);
	if (n == 0)
		return row;
	uint64_t v = row;
	return (uint32_t)(((v << n) | (v >> (steps - n))) & mask);
}

unsigned ctui_stepseq_wrap(long pos, unsigned steps)
{
	if (steps == 0)
		return 0;
	long m = pos % (long)steps;
	return (unsigned)(m < 0 ? m + (long)steps : m);
}

unsigned ctui_stepseq_next(unsigned pos, unsigned steps)
{
	return ctui_stepseq_wrap((long)pos + 1, steps);
}

bool ctui_stepseq_group_start(unsigned step, unsigned group)
{
	return group == 0 ? step == 0 : step % group == 0;
}

unsigned ctui_stepseq_level(unsigned velocity)
{
	return velocity >= 96 ? 3 : velocity >= 64 ? 2 : velocity >= 32 ? 1 : 0;
}

void ctui_stepseq_init(ctui_stepseq *s, uint32_t *pattern, unsigned tracks,
		       unsigned steps)
{
	*s = (ctui_stepseq){.pattern = pattern,
			    .tracks = pattern ? tracks : 0,
			    .group = 4,
			    .fresh = 100};
	s->steps = steps < 1			    ? 1
		   : steps > CTUI_STEPSEQ_MAX_STEPS ? CTUI_STEPSEQ_MAX_STEPS
						    : steps;
}

static bool inside(const ctui_stepseq *s, unsigned track, unsigned step)
{
	return track < s->tracks && step < s->steps;
}

bool ctui_stepseq_on(const ctui_stepseq *s, unsigned track, unsigned step)
{
	return inside(s, track, step) &&
	       ctui_stepseq_bit_test(s->pattern[track], step);
}

unsigned ctui_stepseq_velocity(const ctui_stepseq *s, unsigned track,
			       unsigned step)
{
	if (!ctui_stepseq_on(s, track, step))
		return 0;
	if (!s->velocity)
		return CTUI_STEPSEQ_VELOCITY_MAX;
	unsigned v = s->velocity[(size_t)track * s->steps + step];
	return v < 1			       ? 1
	       : v > CTUI_STEPSEQ_VELOCITY_MAX ? CTUI_STEPSEQ_VELOCITY_MAX
					       : v;
}

bool ctui_stepseq_put(ctui_stepseq *s, unsigned track, unsigned step, bool on)
{
	if (!inside(s, track, step) ||
	    ctui_stepseq_bit_test(s->pattern[track], step) == on)
		return false;
	s->pattern[track] = ctui_stepseq_bit_set(s->pattern[track], step, on);
	if (on && s->velocity) {
		uint8_t *v = &s->velocity[(size_t)track * s->steps + step];
		if (*v == 0)
			*v = (uint8_t)(s->fresh < 1 ? 1
				       : s->fresh > CTUI_STEPSEQ_VELOCITY_MAX
					   ? CTUI_STEPSEQ_VELOCITY_MAX
					   : s->fresh);
	}
	return true;
}

bool ctui_stepseq_flip(ctui_stepseq *s, unsigned track, unsigned step)
{
	if (!inside(s, track, step))
		return false;
	ctui_stepseq_put(s, track, step, !ctui_stepseq_on(s, track, step));
	return ctui_stepseq_on(s, track, step);
}

bool ctui_stepseq_adjust(ctui_stepseq *s, unsigned track, unsigned step,
			 int delta)
{
	if (!s->velocity || !ctui_stepseq_on(s, track, step))
		return false;
	int v = (int)ctui_stepseq_velocity(s, track, step) + delta;
	v = v < 1			    ? 1
	    : v > CTUI_STEPSEQ_VELOCITY_MAX ? CTUI_STEPSEQ_VELOCITY_MAX
					    : v;
	uint8_t *cell = &s->velocity[(size_t)track * s->steps + step];
	bool changed = *cell != (uint8_t)v;
	*cell = (uint8_t)v;
	return changed;
}

void ctui_stepseq_clear_track(ctui_stepseq *s, unsigned track)
{
	if (track < s->tracks)
		s->pattern[track] = 0;
}

void ctui_stepseq_clear(ctui_stepseq *s)
{
	for (unsigned t = 0; t < s->tracks; t++)
		s->pattern[t] = 0;
}

void ctui_stepseq_rotate(ctui_stepseq *s, unsigned track, int by)
{
	if (track >= s->tracks)
		return;
	s->pattern[track] =
	    ctui_stepseq_bit_rotate(s->pattern[track], s->steps, by);
	if (!s->velocity || s->steps < 2)
		return;
	uint8_t old[CTUI_STEPSEQ_MAX_STEPS];
	uint8_t *v = &s->velocity[(size_t)track * s->steps];
	for (unsigned i = 0; i < s->steps; i++)
		old[i] = v[i];
	for (unsigned i = 0; i < s->steps; i++)
		v[ctui_stepseq_wrap((long)i + by, s->steps)] = old[i];
}

bool ctui_stepseq_key(ctui_stepseq *s, int key)
{
	if (s->tracks == 0)
		return false;
	if (s->track >= s->tracks)
		s->track = s->tracks - 1;
	if (s->step >= s->steps)
		s->step = s->steps - 1;
	switch (key) {
	case KEY_LEFT:
		if (s->step > 0)
			s->step--;
		break;
	case KEY_RIGHT:
		if (s->step + 1 < s->steps)
			s->step++;
		break;
	case KEY_UP:
		if (s->track > 0)
			s->track--;
		break;
	case KEY_DOWN:
		if (s->track + 1 < s->tracks)
			s->track++;
		break;
	case KEY_HOME:
		s->step = 0;
		break;
	case KEY_END:
		s->step = s->steps - 1;
		break;
	case ' ':
	case '\n':
	case KEY_ENTER:
		ctui_stepseq_flip(s, s->track, s->step);
		break;
	case KEY_DC:
		ctui_stepseq_put(s, s->track, s->step, false);
		break;
	case '+':
	case '=':
		ctui_stepseq_adjust(s, s->track, s->step,
				    CTUI_STEPSEQ_VELOCITY_STEP);
		break;
	case '-':
		ctui_stepseq_adjust(s, s->track, s->step,
				    -CTUI_STEPSEQ_VELOCITY_STEP);
		break;
	case '[':
		ctui_stepseq_rotate(s, s->track, -1);
		break;
	case ']':
		ctui_stepseq_rotate(s, s->track, 1);
		break;
	case 'c':
		ctui_stepseq_clear_track(s, s->track);
		break;
	case 'C':
		ctui_stepseq_clear(s);
		break;
	default:
		return false;
	}
	return true;
}

static void track_label(const ctui_stepseq *s, unsigned track, char *buf,
			size_t cap)
{
	if (s->labels && s->labels[track])
		snprintf(buf, cap, "%s", s->labels[track]);
	else
		snprintf(buf, cap, "T%u", track + 1);
}

int ctui_stepseq_label_width(const ctui_stepseq *s)
{
	size_t w = 0;
	for (unsigned t = 0; t < s->tracks; t++) {
		char label[64];
		track_label(s, t, label, sizeof label);
		size_t cols = ctui_utf8_columns(label);
		if (cols > w)
			w = cols;
	}
	return (int)(w > CTUI_STEPSEQ_LABEL_MAX ? CTUI_STEPSEQ_LABEL_MAX : w);
}

int ctui_stepseq_x(const ctui_stepseq *s, unsigned step)
{
	int gaps = s->group ? (int)(step / s->group) : 0;
	return ctui_stepseq_label_width(s) + 1 + (int)step * CELL + gaps;
}

int ctui_stepseq_width(const ctui_stepseq *s)
{
	if (s->steps == 0) /* only after a caller broke the 1..32 range */
		return ctui_stepseq_x(s, 0);
	return ctui_stepseq_x(s, s->steps - 1) + CELL;
}

int ctui_stepseq_height(const ctui_stepseq *s)
{
	return (int)s->tracks + 1;
}

bool ctui_stepseq_hit(const ctui_stepseq *s, int y, int x, unsigned *track,
		      unsigned *step)
{
	if (y < 1 || y > (int)s->tracks || x < 0)
		return false;
	for (unsigned i = 0; i < s->steps; i++) {
		int cx = ctui_stepseq_x(s, i);
		if (x >= cx && x < cx + CELL) {
			if (track)
				*track = (unsigned)y - 1;
			if (step)
				*step = i;
			return true;
		}
	}
	return false;
}

bool ctui_stepseq_press(ctui_stepseq *s, int y, int x)
{
	unsigned t, st;
	if (!ctui_stepseq_hit(s, y, x, &t, &st))
		return false;
	s->track = t;
	s->step = st;
	s->paint = ctui_stepseq_flip(s, t, st);
	s->dragging = true;
	return true;
}

bool ctui_stepseq_drag(ctui_stepseq *s, int y, int x)
{
	if (!s->dragging)
		return false;
	unsigned t, st;
	if (ctui_stepseq_hit(s, y, x, &t, &st)) {
		s->track = t;
		s->step = st;
		ctui_stepseq_put(s, t, st, s->paint);
	}
	return true;
}

bool ctui_stepseq_release(ctui_stepseq *s)
{
	bool was = s->dragging;
	s->dragging = false;
	return was;
}

bool ctui_stepseq_wheel(ctui_stepseq *s, int y, int x, int notches)
{
	unsigned t, st;
	if (!ctui_stepseq_hit(s, y, x, &t, &st))
		return false;
	return ctui_stepseq_adjust(s, t, st,
				   notches * CTUI_STEPSEQ_VELOCITY_STEP);
}

static int cell_attr(const ctui_stepseq *s, unsigned st, unsigned level,
		     bool on)
{
	if (s->show_playhead && st == s->playhead)
		return ctui_attr(CTUI_PAIR_PLAYHEAD, false);
	if (!on)
		return ctui_attr((s->group ? st / s->group : 0) % 2
				     ? CTUI_PAIR_MUTED
				     : CTUI_PAIR_GAUGE_EMPTY,
				 false);
	if (!s->velocity)
		return ctui_attr(CTUI_PAIR_GAUGE_GREEN_BRIGHT, true);
	static const ctui_pair LEVEL_PAIR[4] = {
	    CTUI_PAIR_GAUGE_GREEN, CTUI_PAIR_GAUGE_GREEN_BRIGHT,
	    CTUI_PAIR_GAUGE_YELLOW, CTUI_PAIR_GAUGE_YELLOW_BRIGHT};
	return ctui_attr(LEVEL_PAIR[level], level >= 1);
}

static void draw_impl(const ctui_stepseq *s, int y, int x, int width,
		      int height)
{
	if (width < 1 || height < 1 || s->tracks == 0)
		return;
	const char *const *glyph =
	    ctui_theme_unicode() ? GLYPHS_UNICODE : GLYPHS_ASCII;
	int lw = ctui_stepseq_label_width(s);
	int muted = ctui_attr(CTUI_PAIR_MUTED, false);
	for (unsigned st = 0; st < s->steps; st++) {
		int cx = ctui_stepseq_x(s, st);
		if (cx + CELL > width)
			break;
		char head[16] = "  ";
		if (ctui_stepseq_group_start(st, s->group))
			snprintf(head, sizeof head, "%-2u", st + 1);
		bool here = s->show_playhead && st == s->playhead;
		ctui_put(y, x + cx, CELL, head,
			 here ? ctui_attr(CTUI_PAIR_PLAYHEAD, false) : muted);
	}
	for (unsigned t = 0; t < s->tracks && (int)t + 1 < height; t++) {
		int row = y + 1 + (int)t;
		char label[64];
		track_label(s, t, label, sizeof label);
		ctui_put(row, x, lw < width ? lw : width, label,
			 t == s->track ? ctui_attr(CTUI_PAIR_HEADING, true)
				       : muted);
		for (unsigned st = 0; st < s->steps; st++) {
			int cx = ctui_stepseq_x(s, st);
			if (cx + CELL > width)
				break;
			bool on = ctui_stepseq_on(s, t, st);
			unsigned level =
			    on ? ctui_stepseq_level(
				     ctui_stepseq_velocity(s, t, st))
			       : 0;
			int attr = cell_attr(s, st, level, on);
			if (!s->hide_cursor && t == s->track && st == s->step)
				attr = ctui_attr(CTUI_PAIR_SELECTION, on);
			char cell[16];
			snprintf(cell, sizeof cell, "%s ",
				 glyph[on ? (s->velocity ? level + 1 : 4) : 0]);
			ctui_put(row, x + cx, CELL, cell, attr);
		}
	}
}

void ctui_stepseq_draw(const ctui_stepseq *s, int y, int x, int width,
		       int height)
{
	ctui_style_begin(s->style);
	draw_impl(s, y, x, width, height);
	ctui_style_end();
}
