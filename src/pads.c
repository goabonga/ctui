/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * pads.c - a grid of trigger pads (pads.h).
 */
#include <ctui/pads.h>

#include <curses.h>
#include <stdio.h>
#include <string.h>

#include <ctui/text.h>
#include <ctui/theme.h>

static unsigned clamp_dim(unsigned n)
{
	if (n == 0)
		return 4;
	return n > CTUI_PADS_MAX ? CTUI_PADS_MAX : n;
}

void ctui_pads_init(ctui_pads *p, unsigned rows, unsigned cols,
		    const char *const *labels)
{
	*p = (ctui_pads){.rows = clamp_dim(rows),
			 .cols = clamp_dim(cols),
			 .labels = labels,
			 .held = -1,
			 .velocity = 0.8f,
			 .decay = 4.0f};
}

size_t ctui_pads_count(const ctui_pads *p)
{
	return (size_t)p->rows * p->cols;
}

bool ctui_pads_hit(ctui_pads *p, size_t index, float velocity)
{
	if (index >= ctui_pads_count(p))
		return false;
	if (!(velocity > 0)) /* also NaN */
		velocity = 0;
	p->level[index] = velocity > 1 ? 1 : velocity;
	return true;
}

bool ctui_pads_active(const ctui_pads *p)
{
	for (size_t i = 0; i < ctui_pads_count(p); i++)
		if (p->level[i] > 0)
			return true;
	return false;
}

bool ctui_pads_tick(ctui_pads *p, double seconds)
{
	if (p->ticked && seconds > p->last_time) {
		float lost = (float)(seconds - p->last_time) * p->decay;
		for (size_t i = 0; i < ctui_pads_count(p); i++)
			p->level[i] =
			    p->level[i] > lost ? p->level[i] - lost : 0;
	}
	if (!p->ticked || seconds > p->last_time)
		p->last_time = seconds;
	p->ticked = true;
	return ctui_pads_active(p);
}

void ctui_pads_move(ctui_pads *p, int drow, int dcol)
{
	long row = (long)(p->cursor / p->cols) + drow,
	     col = (long)(p->cursor % p->cols) + dcol;
	if (row < 0)
		row = 0;
	if (row >= (long)p->rows)
		row = (long)p->rows - 1;
	if (col < 0)
		col = 0;
	if (col >= (long)p->cols)
		col = (long)p->cols - 1;
	p->cursor = (size_t)row * p->cols + (size_t)col;
}

int ctui_pads_key_pad(const ctui_pads *p, int key)
{
	static const char MAP[4][4] = {{'1', '2', '3', '4'},
				       {'q', 'w', 'e', 'r'},
				       {'a', 's', 'd', 'f'},
				       {'z', 'x', 'c', 'v'}};
	for (unsigned r = 0; r < 4; r++)
		for (unsigned c = 0; c < 4; c++)
			if (key == MAP[r][c])
				return r < p->rows && c < p->cols
					   ? (int)(r * p->cols + c)
					   : -1;
	return -1;
}

bool ctui_pads_key(ctui_pads *p, int key, int *hit)
{
	int pad = -1;
	bool handled = true;
	switch (key) {
	case KEY_LEFT:
		ctui_pads_move(p, 0, -1);
		break;
	case KEY_RIGHT:
		ctui_pads_move(p, 0, 1);
		break;
	case KEY_UP:
		ctui_pads_move(p, -1, 0);
		break;
	case KEY_DOWN:
		ctui_pads_move(p, 1, 0);
		break;
	case KEY_HOME:
		p->cursor = 0;
		break;
	case KEY_END:
		p->cursor = ctui_pads_count(p) - 1;
		break;
	case '\n':
	case KEY_ENTER:
	case ' ':
		pad = (int)p->cursor;
		break;
	default:
		handled = false;
		if (p->key_map) {
			pad = ctui_pads_key_pad(p, key);
			if (pad >= 0) {
				p->cursor = (size_t)pad;
				handled = true;
			}
		}
	}
	if (pad >= 0)
		ctui_pads_hit(p, (size_t)pad, p->velocity);
	if (hit)
		*hit = pad;
	return handled;
}

bool ctui_pads_cell(const ctui_pads *p, int width, int height, size_t index,
		    int *y, int *x, int *w, int *h)
{
	int cols = (int)p->cols, rows = (int)p->rows;
	int cw = (width - (cols - 1)) / cols;
	int ch = (height - (rows - 1)) / rows;
	int gap_y = 1;
	if (ch < 1) { /* no room for the separating rows */
		gap_y = 0;
		ch = height / rows;
	}
	if (cw < 1 || ch < 1 || index >= ctui_pads_count(p))
		return false;
	int row = (int)(index / p->cols), col = (int)(index % p->cols);
	*y = row * (ch + gap_y);
	*x = col * (cw + 1);
	*w = cw;
	*h = ch;
	return true;
}

int ctui_pads_at(const ctui_pads *p, int width, int height, int y, int x)
{
	for (size_t i = 0; i < ctui_pads_count(p); i++) {
		int cy, cx, cw, ch;
		if (!ctui_pads_cell(p, width, height, i, &cy, &cx, &cw, &ch))
			return -1;
		if (y >= cy && y < cy + ch && x >= cx && x < cx + cw)
			return (int)i;
	}
	return -1;
}

float ctui_pads_velocity(int row, int height)
{
	if (height <= 1)
		return 0.8f;
	if (row < 0)
		row = 0;
	if (row > height - 1)
		row = height - 1;
	return 1.0f - 0.6f * (float)row / (float)(height - 1);
}

ctui_pair ctui_pads_level_pair(float level)
{
	if (!(level > 0))
		return CTUI_PAIR_GAUGE_EMPTY;
	if (level < 0.34f)
		return CTUI_PAIR_GAUGE_GREEN;
	if (level < 0.67f)
		return CTUI_PAIR_GAUGE_YELLOW;
	return CTUI_PAIR_GAUGE_RED;
}

/* Hit the pad at (y, x), put the cursor on it and hold it; -1 when there is
 * none. */
static int strike(ctui_pads *p, int width, int height, int y, int x)
{
	int pad = ctui_pads_at(p, width, height, y, x);
	if (pad < 0)
		return -1;
	int cy = 0, cx = 0, cw = 0, ch = 0;
	ctui_pads_cell(p, width, height, (size_t)pad, &cy, &cx, &cw, &ch);
	ctui_pads_hit(p, (size_t)pad, ctui_pads_velocity(y - cy, ch));
	p->cursor = (size_t)pad;
	p->held = pad;
	return pad;
}

int ctui_pads_press(ctui_pads *p, int width, int height, int y, int x)
{
	return strike(p, width, height, y, x);
}

int ctui_pads_drag(ctui_pads *p, int width, int height, int y, int x)
{
	if (p->held < 0 || ctui_pads_at(p, width, height, y, x) == p->held)
		return -1;
	return strike(p, width, height, y, x);
}

bool ctui_pads_release(ctui_pads *p)
{
	bool was = p->held >= 0;
	p->held = -1;
	return was;
}

static void draw_pad(const ctui_pads *p, size_t index, int y, int x, int w,
		     int h)
{
	const ctui_glyphs *g = ctui_theme_glyphs();
	float level = p->level[index];
	bool lit = level > 0, cursor = index == p->cursor;
	ctui_pair pair = ctui_pads_level_pair(level);
	int ink = ctui_attr(pair, lit && level >= 0.67f);
	const char *fill = lit ? g->gauge_filled : g->gauge_empty;
	for (int r = 0; r < h; r++)
		for (int c = 0; c < w; c++)
			ctui_put(y + r, x + c, 1, fill, ink);

	char label[64];
	const char *name = p->labels ? p->labels[index] : NULL;
	if (name) {
		size_t n = strlen(name);
		if (n > sizeof label - 1) { /* cut on a character boundary */
			n = sizeof label - 1;
			while (n > 0 && ((unsigned)(unsigned char)name[n] &
					 0xC0U) == 0x80U)
				n--;
		}
		memcpy(label, name, n);
		label[n] = '\0';
	} else
		snprintf(label, sizeof label, "%zu", index + 1);
	char text[80];
	snprintf(text, sizeof text, cursor ? "[%s]" : " %s ", label);
	int attr =
	    lit ? (int)((unsigned)ctui_attr(pair, true) | (unsigned)A_REVERSE)
		: (cursor ? ctui_attr(CTUI_PAIR_SELECTION, true)
			  : ctui_attr(CTUI_PAIR_DIALOG, false));
	int len = (int)ctui_utf8_columns(text);
	if (len > w) { /* too narrow for the framing: the label alone, cut */
		snprintf(text, sizeof text, "%s", label);
		len = (int)ctui_utf8_columns(text);
	}
	int shown = len < w ? len : w;
	ctui_put(y + h / 2, x + (w - shown) / 2, w, text, attr);
}

void ctui_pads_draw(const ctui_pads *p, int y, int x, int width, int height)
{
	ctui_style_begin(p->style);
	for (size_t i = 0; i < ctui_pads_count(p); i++) {
		int cy, cx, cw, ch;
		if (!ctui_pads_cell(p, width, height, i, &cy, &cx, &cw, &ch))
			break;
		draw_pad(p, i, y + cy, x + cx, cw, ch);
	}
	ctui_style_end();
}
