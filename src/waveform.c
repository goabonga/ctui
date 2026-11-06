/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * waveform.c - a zoomable waveform view (waveform.h).
 */
#include <ctui/waveform.h>

#include <curses.h>

#include <ctui/text.h>
#include <ctui/theme.h>

void ctui_waveform_init(ctui_waveform *w, const float *samples, size_t frames,
			unsigned channels)
{
	*w = (ctui_waveform){.samples = samples,
			     .frames = frames,
			     .channels = channels,
			     .span = frames,
			     .first = 0,
			     .end = frames};
}

void ctui_waveform_column(const ctui_waveform *w, int x, int cols, size_t *a,
			  size_t *b)
{
	if (cols < 1)
		cols = 1;
	*a = w->offset + (size_t)((double)x * (double)w->span / cols);
	*b = w->offset + (size_t)((double)(x + 1) * (double)w->span / cols);
	if (*b <= *a)
		*b = *a + 1;
	if (*b > w->frames)
		*b = w->frames;
}

void ctui_waveform_peak(const ctui_waveform *w, size_t a, size_t b, float *low,
			float *high)
{
	float lo = 0, hi = 0;
	for (size_t i = a; i < b && i < w->frames; i++)
		for (unsigned c = 0; c < w->channels; c++) {
			float v = w->samples[i * w->channels + c];
			if (v < lo)
				lo = v;
			if (v > hi)
				hi = v;
		}
	*low = lo;
	*high = hi;
}

int ctui_waveform_x(const ctui_waveform *w, size_t frame, int cols)
{
	if (frame < w->offset || frame >= w->offset + w->span || cols < 1)
		return -1;
	return (int)((double)(frame - w->offset) * cols / (double)w->span);
}

static void fit_window(ctui_waveform *w)
{
	if (w->span > w->frames)
		w->span = w->frames;
	if (w->span < 1)
		w->span = 1;
	if (w->offset > w->frames - w->span)
		w->offset = w->frames - w->span;
}

static size_t frame_at(const ctui_waveform *w, int x, int cols)
{
	if (cols < 1)
		cols = 1;
	if (x < 0)
		x = 0;
	if (x > cols - 1)
		x = cols - 1;
	size_t a, b;
	ctui_waveform_column(w, x, cols, &a, &b);
	return a < w->frames ? a : w->frames - 1;
}

void ctui_waveform_press(ctui_waveform *w, int x, int cols)
{
	w->cursor = w->anchor = frame_at(w, x, cols);
	w->dragging = true;
}

bool ctui_waveform_drag(ctui_waveform *w, int x, int cols)
{
	if (!w->dragging)
		return false;
	size_t at = frame_at(w, x, cols);
	w->cursor = at;
	if (at == w->anchor && !w->selection)
		return true;
	w->first = at < w->anchor ? at : w->anchor;
	w->end = (at < w->anchor ? w->anchor : at) + 1;
	w->selection = true;
	return true;
}

bool ctui_waveform_release(ctui_waveform *w)
{
	bool was = w->dragging;
	w->dragging = false;
	return was;
}

void ctui_waveform_reset(ctui_waveform *w)
{
	w->selection = false;
	w->dragging = false;
	w->first = 0;
	w->end = w->frames;
	w->span = w->frames;
	w->offset = 0;
	w->cursor = 0;
}

bool ctui_waveform_key(ctui_waveform *w, int key, int cols)
{
	size_t step = w->span / (size_t)(cols > 0 ? cols : 1);
	if (!step)
		step = 1;
	switch (key) {
	case KEY_LEFT:
		w->cursor = w->cursor > step ? w->cursor - step : 0;
		break;
	case KEY_RIGHT:
		w->cursor = step > w->frames - 1 - w->cursor ? w->frames - 1
							     : w->cursor + step;
		break;
	case KEY_HOME:
		w->cursor = 0;
		break;
	case KEY_END:
		w->cursor = w->frames - 1;
		break;
	case '+':
	case '=':
		if (w->span > 2)
			w->span /= 2;
		break;
	case '-':
		w->span = w->span > w->frames / 2 ? w->frames : w->span * 2;
		break;
	case 'r':
		ctui_waveform_reset(w);
		break;
	case '[':
		if (w->cursor < w->end) {
			w->first = w->cursor;
			w->selection = true;
		}
		return true;
	case ']':
		if (w->cursor >= w->first) {
			w->end = w->cursor + 1;
			w->selection = true;
		}
		return true;
	default:
		return false;
	}
	if ((w->cursor < w->offset || w->cursor >= w->offset + w->span))
		w->offset =
		    w->cursor > w->span / 2 ? w->cursor - w->span / 2 : 0;
	fit_window(w);
	return true;
}

/* One column as a solid bar from the middle line: block glyphs make rows of
 * two halves, so the bar grows in steps of half a row. */
static void draw_filled_column(int y, int x, int height, float low, float high,
			       int background, int ink)
{
	int half = height; /* the middle line, in half rows from the top */
	int up = high > 0 ? (int)(high * (float)half) : 0;
	int down = low < 0 ? (int)(-low * (float)half) : 0;
	if (up + down == 0)
		down = 1; /* silence keeps a mark on the middle line */
	int first = half - up, last = half + down - 1;
	if (first < 0)
		first = 0;
	if (last > 2 * height - 1)
		last = 2 * height - 1;
	for (int row = 0; row < height; row++) {
		bool top = 2 * row >= first && 2 * row <= last;
		bool bottom = 2 * row + 1 >= first && 2 * row + 1 <= last;
		const char *glyph = top && bottom ? "\xe2\x96\x88"
				    : top	  ? "\xe2\x96\x80"
				    : bottom	  ? "\xe2\x96\x84"
						  : " ";
		ctui_put(y + row, x, 1, glyph,
			 glyph[0] == ' ' ? background : ink);
	}
}

static void waveform_draw_impl(const ctui_waveform *w, int y, int x, int width,
			       int height)
{
	if (width < 1 || height < 3)
		return;
	const ctui_glyphs *g = ctui_theme_glyphs();
	int mid = y + height / 2;
	int body = ctui_attr(CTUI_PAIR_BODY, false),
	    sel = ctui_attr(CTUI_PAIR_SELECTION, false);
	for (int col = 0; col < width; col++) {
		size_t a, b;
		ctui_waveform_column(w, col, width, &a, &b);
		float low, high;
		ctui_waveform_peak(w, a, b, &low, &high);
		int y1 = mid - (int)(high * (float)(height - 1) / 2);
		int y2 = mid - (int)(low * (float)(height - 1) / 2);
		bool selected = w->selection && a < w->end && b > w->first;
		int background = selected ? sel : body;
		if (w->filled && ctui_theme_unicode()) {
			/* the tones of the header: info up to the playhead (all
			 * of it without one), idle after */
			bool played = !w->show_playhead || a <= w->playhead;
			int ink =
			    selected
				? ctui_attr(CTUI_PAIR_CURSOR_SELECTED, false)
				: ctui_attr_color(ctui_tone_background(
						      played ? CTUI_TONE_INFO
							     : CTUI_TONE_IDLE),
						  -1);
			draw_filled_column(y, x + col, height, low, high,
					   background, ink);
			continue;
		}
		for (int row = y; row < y + height; row++)
			ctui_put(row, x + col, 1, " ", background);
		for (int row = y1; row <= y2; row++)
			if (row >= y && row < y + height)
				ctui_put(row, x + col, 1, g->box_v, background);
	}
	int cx = ctui_waveform_x(w, w->cursor, width);
	if (cx >= 0) {
		bool selected =
		    w->selection && w->cursor >= w->first && w->cursor < w->end;
		int attr = ctui_attr(selected ? CTUI_PAIR_CURSOR_SELECTED
					      : CTUI_PAIR_CURSOR,
				     true);
		for (int row = y; row < y + height; row++)
			ctui_put(row, x + cx, 1, g->box_v, attr);
	}
	if (w->show_playhead) {
		size_t last =
		    w->playhead >= w->frames ? w->frames - 1 : w->playhead;
		int px = ctui_waveform_x(w, last, width);
		if (px >= 0)
			for (int row = y; row < y + height; row++)
				ctui_put(row, x + px, 1, g->box_v,
					 ctui_attr(CTUI_PAIR_PLAYHEAD, false));
	}
}

void ctui_waveform_draw(const ctui_waveform *w, int y, int x, int width,
			int height)
{
	ctui_style_begin(w->style);
	waveform_draw_impl(w, y, x, width, height);
	ctui_style_end();
}
