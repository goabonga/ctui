/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * spinner.c - activity indicator (spinner.h).
 */
#include <ctui/spinner.h>

#include <math.h>

#include <ctui/text.h>

typedef struct frames {
	const char *const *items;
	size_t count;
} frames;

static const char *const BRAILLE[] = {
    "\xe2\xa0\x8b", "\xe2\xa0\x99", "\xe2\xa0\xb9", "\xe2\xa0\xb8",
    "\xe2\xa0\xbc", "\xe2\xa0\xb4", "\xe2\xa0\xa6", "\xe2\xa0\xa7",
    "\xe2\xa0\x87", "\xe2\xa0\x8f"};
static const char *const BRAILLE_ASCII[] = {"|", "/", "-", "\\"};
static const char *const LINE_UTF8[] = {"\xe2\x94\x80", "\xe2\x95\xb2",
					"\xe2\x94\x82", "\xe2\x95\xb1"};
static const char *const LINE_ASCII[] = {"-", "\\", "|", "/"};
static const char *const DOTS_UTF8[] = {
    "\xe2\xa3\xbe", "\xe2\xa3\xbd", "\xe2\xa3\xbb", "\xe2\xa2\xbf",
    "\xe2\xa1\xbf", "\xe2\xa3\x9f", "\xe2\xa3\xaf", "\xe2\xa3\xb7"};
static const char *const DOTS_ASCII[] = {".  ", ".. ", "...", "   "};

#define SET(a) {(a), sizeof(a) / sizeof((a)[0])}

static frames set_of(ctui_spinner_kind kind, bool unicode)
{
	switch (kind) {
	case CTUI_SPINNER_BRAILLE:
		return unicode ? (frames)SET(BRAILLE)
			       : (frames)SET(BRAILLE_ASCII);
	case CTUI_SPINNER_LINE:
		return unicode ? (frames)SET(LINE_UTF8)
			       : (frames)SET(LINE_ASCII);
	case CTUI_SPINNER_DOTS:
		return unicode ? (frames)SET(DOTS_UTF8)
			       : (frames)SET(DOTS_ASCII);
	}
	return (frames){NULL, 0};
}

size_t ctui_spinner_frame_count(ctui_spinner_kind kind, bool unicode)
{
	return set_of(kind, unicode).count;
}

size_t ctui_spinner_index(ctui_spinner_kind kind, bool unicode, double seconds,
			  double fps)
{
	size_t n = set_of(kind, unicode).count;
	if (n == 0 || !(seconds > 0))
		return 0;
	if (!(fps > 0))
		fps = CTUI_SPINNER_FPS;
	double ticks = floor(seconds * fps);
	if (!(ticks < 9e15)) /* beyond exact integers: any frame will do */
		return 0;
	return (size_t)((unsigned long long)ticks % n);
}

const char *ctui_spinner_frame(ctui_spinner_kind kind, bool unicode, size_t i)
{
	frames f = set_of(kind, unicode);
	return f.count ? f.items[i % f.count] : "";
}

static int frame_width(ctui_spinner_kind kind, bool unicode)
{
	frames f = set_of(kind, unicode);
	size_t widest = 0;
	for (size_t i = 0; i < f.count; i++) {
		size_t w = ctui_utf8_columns(f.items[i]);
		if (w > widest)
			widest = w;
	}
	return (int)widest;
}

int ctui_spinner_width(const ctui_spinner *s)
{
	int w = frame_width(s->kind, ctui_theme_glyphs()->unicode);
	if (s->label && *s->label)
		w += 1 + (int)ctui_utf8_columns(s->label);
	return w;
}

int ctui_spinner_draw(const ctui_spinner *s, int y, int x, int maxcols,
		      double seconds)
{
	ctui_style_begin(s->style);
	bool unicode = ctui_theme_glyphs()->unicode;
	size_t i = ctui_spinner_index(s->kind, unicode, seconds, s->fps);
	int used =
	    ctui_put(y, x, maxcols, ctui_spinner_frame(s->kind, unicode, i),
		     ctui_attr(CTUI_PAIR_CURSOR, true));
	int fw = frame_width(s->kind, unicode);
	if (s->label && *s->label && maxcols > fw + 1)
		used = fw + 1 +
		       ctui_put(y, x + fw + 1, maxcols - fw - 1, s->label,
				ctui_attr(CTUI_PAIR_BODY, false));
	ctui_style_end();
	return used;
}
