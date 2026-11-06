/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * toast.c - transient notifications (toast.h).
 */
#include <ctui/toast.h>

#include <curses.h>
#include <string.h>

#include <ctui/text.h>
#include <ctui/theme.h>

/* Columns around the message: space, glyph, space ... space, close, space. */
#define CHROME 7

void ctui_toasts_init(ctui_toasts *t)
{
	*t = (ctui_toasts){.count = 0};
}

/* Copy s into out, cutting on a character boundary when it is too long. */
static void copy_text(char *out, const char *s)
{
	size_t n = strlen(s);
	if (n >= CTUI_TOAST_TEXT) {
		n = CTUI_TOAST_TEXT - 1;
		while (n && ((unsigned)(unsigned char)s[n] & 0xC0U) == 0x80U)
			n--;
	}
	memcpy(out, s, n);
	out[n] = '\0';
}

void ctui_toasts_push(ctui_toasts *t, ctui_toast_level level, const char *text,
		      double now, double duration)
{
	size_t limit = t->limit ? t->limit : CTUI_TOAST_MAX;
	if (limit > CTUI_TOAST_CAPACITY)
		limit = CTUI_TOAST_CAPACITY;
	if (t->count >= limit) {
		size_t drop = t->count - limit + 1;
		memmove(t->items, t->items + drop,
			(t->count - drop) * sizeof t->items[0]);
		t->count -= drop;
	}
	ctui_toast *toast = &t->items[t->count++];
	toast->level = level;
	toast->expires = duration > 0 ? now + duration : 0;
	copy_text(toast->text, text ? text : "");
}

bool ctui_toasts_dismiss(ctui_toasts *t, size_t i)
{
	if (i >= t->count)
		return false;
	memmove(t->items + i, t->items + i + 1,
		(t->count - i - 1) * sizeof t->items[0]);
	t->count--;
	return true;
}

size_t ctui_toasts_expire(ctui_toasts *t, double now)
{
	size_t removed = 0;
	for (size_t i = 0; i < t->count;) {
		if (t->items[i].expires > 0 && now >= t->items[i].expires) {
			ctui_toasts_dismiss(t, i);
			removed++;
		} else {
			i++;
		}
	}
	return removed;
}

bool ctui_toasts_geometry(const ctui_toasts *t, int top, int lines, int cols,
			  size_t i, ctui_rect *box)
{
	if (i >= t->count || cols < CHROME + 4)
		return false;
	int y = top + (int)i;
	if (y < 0 || y >= lines)
		return false;
	int width = (int)ctui_utf8_columns(t->items[i].text) + CHROME;
	if (width > cols - 2)
		width = cols - 2;
	*box = (ctui_rect){y, cols - width - 1, 1, width};
	return true;
}

int ctui_toasts_hit(const ctui_toasts *t, int top, int lines, int cols, int y,
		    int x)
{
	for (size_t i = 0; i < t->count; i++) {
		ctui_rect box;
		if (ctui_toasts_geometry(t, top, lines, cols, i, &box) &&
		    y == box.y && x >= box.x && x < box.x + box.width)
			return (int)i;
	}
	return -1;
}

static ctui_tone tone_of(ctui_toast_level level)
{
	switch (level) {
	case CTUI_TOAST_OK:
		return CTUI_TONE_OK;
	case CTUI_TOAST_WARN:
		return CTUI_TONE_BUSY;
	case CTUI_TOAST_ERROR:
		return CTUI_TONE_BAD;
	default:
		return CTUI_TONE_INFO;
	}
}

static const char *glyph_of(ctui_toast_level level)
{
	const ctui_glyphs *g = ctui_theme_glyphs();
	switch (level) {
	case CTUI_TOAST_OK:
		return g->check;
	case CTUI_TOAST_WARN:
		return g->warn;
	case CTUI_TOAST_ERROR:
		return g->cross;
	default:
		return g->info;
	}
}

static void toasts_draw_impl(const ctui_toasts *t, int top)
{
	for (size_t i = 0; i < t->count; i++) {
		ctui_rect box;
		if (!ctui_toasts_geometry(t, top, LINES, COLS, i, &box))
			continue;
		const ctui_toast *toast = &t->items[i];
		int attr = ctui_attr_segment(tone_of(toast->level), true);
		ctui_fill(box.y, box.x, box.width, attr);
		ctui_put(box.y, box.x + 1, 1, glyph_of(toast->level), attr);
		ctui_put(box.y, box.x + 3, box.width - CHROME, toast->text,
			 attr);
		ctui_put(box.y, box.x + box.width - 2, 1,
			 ctui_theme_glyphs()->close, attr);
	}
}

void ctui_toasts_draw(const ctui_toasts *t, int top)
{
	ctui_style_begin(t->style);
	toasts_draw_impl(t, top);
	ctui_style_end();
}
