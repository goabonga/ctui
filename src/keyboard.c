/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * keyboard.c - a piano keyboard (keyboard.h).
 */
#include <ctui/keyboard.h>

#include <curses.h>
#include <stdio.h>

#include <ctui/text.h>
#include <ctui/theme.h>

#define OCTAVES_MAX 10
#define TYPING_KEYS "awsedftgyhujkolp"

static int clampi(int v, int lo, int hi)
{
	return v < lo ? lo : v > hi ? hi : v;
}

void ctui_keyboard_init(ctui_keyboard *kb, int first, int octaves)
{
	first = clampi(first, 0, CTUI_KEYBOARD_NOTES - 1);
	*kb = (ctui_keyboard){.first = first,
			      .octaves = clampi(octaves, 1, OCTAVES_MAX),
			      .key_width = 4,
			      .cursor = first,
			      .anchor = first,
			      .base = first,
			      .last = -1,
			      .mouse_note = -1};
}

bool ctui_keyboard_is_black(int note)
{
	if (note < 0)
		return false;
	switch (note % 12) {
	case 1:
	case 3:
	case 6:
	case 8:
	case 10:
		return true;
	default:
		return false;
	}
}

char *ctui_keyboard_name(char *buf, size_t cap, int note)
{
	static const char *const NAMES[12] = {
	    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
	if (cap == 0)
		return buf;
	if (note < 0 || note >= CTUI_KEYBOARD_NOTES)
		buf[0] = '\0';
	else
		snprintf(buf, cap, "%s%d", NAMES[note % 12], note / 12 - 1);
	return buf;
}

int ctui_keyboard_key_note(int key, int base)
{
	if (key <= 0 || key > 127)
		return -1;
	const char *at = NULL;
	for (const char *p = TYPING_KEYS; *p; p++)
		if (*p == key)
			at = p;
	if (!at)
		return -1;
	int note = base + (int)(at - TYPING_KEYS);
	return note >= 0 && note < CTUI_KEYBOARD_NOTES ? note : -1;
}

int ctui_keyboard_last(const ctui_keyboard *kb)
{
	int last = kb->first + 12 * kb->octaves - 1;
	return last > CTUI_KEYBOARD_NOTES - 1 ? CTUI_KEYBOARD_NOTES - 1 : last;
}

bool ctui_keyboard_contains(const ctui_keyboard *kb, int note)
{
	return note >= kb->first && note <= ctui_keyboard_last(kb);
}

/* White keys from the first note up to, not including, `note`. */
static int whites_before(const ctui_keyboard *kb, int note)
{
	int n = 0;
	for (int i = kb->first; i < note; i++)
		n += !ctui_keyboard_is_black(i);
	return n;
}

int ctui_keyboard_whites(const ctui_keyboard *kb)
{
	return whites_before(kb, ctui_keyboard_last(kb) + 1);
}

static int key_width(const ctui_keyboard *kb)
{
	return clampi(kb->key_width, 2, 8);
}

int ctui_keyboard_width(const ctui_keyboard *kb)
{
	return ctui_keyboard_whites(kb) * key_width(kb);
}

static int black_width(int kw)
{
	return (2 * kw + 2) / 3;
}

static int black_rows(int height)
{
	return height / 2 > 1 ? height / 2 : 1;
}

/* The white key number `index` (counted from the left), or -1. */
static int white_note(const ctui_keyboard *kb, int index)
{
	for (int n = kb->first; n <= ctui_keyboard_last(kb); n++)
		if (!ctui_keyboard_is_black(n) && index-- == 0)
			return n;
	return -1;
}

int ctui_keyboard_key_x(const ctui_keyboard *kb, int note, int *width)
{
	if (!ctui_keyboard_contains(kb, note))
		return -1;
	int kw = key_width(kb), x, w;
	if (ctui_keyboard_is_black(note)) {
		w = black_width(kw);
		x = whites_before(kb, note) * kw - w / 2;
	} else {
		w = kw;
		x = whites_before(kb, note) * kw;
	}
	if (width)
		*width = w;
	return x;
}

int ctui_keyboard_note_at(const ctui_keyboard *kb, int y, int x, int height)
{
	int kw = key_width(kb), bw = black_width(kw);
	if (y < 0 || y >= height || x < 0 || x >= ctui_keyboard_width(kb))
		return -1;
	int index = x / kw, note = white_note(kb, index);
	if (note < 0)
		return -1;
	if (y < black_rows(height)) {
		int right = note + 1, left = note - 1;
		if (ctui_keyboard_contains(kb, right) &&
		    ctui_keyboard_is_black(right) &&
		    x >= (index + 1) * kw - bw / 2)
			return right;
		if (ctui_keyboard_contains(kb, left) &&
		    ctui_keyboard_is_black(left) &&
		    x < index * kw + (bw - bw / 2))
			return left;
	}
	return note;
}

void ctui_keyboard_set_held(ctui_keyboard *kb, int note, bool held)
{
	if (note < 0 || note >= CTUI_KEYBOARD_NOTES)
		return;
	uint32_t bit = (uint32_t)1 << (unsigned)(note % 32);
	if (held)
		kb->held[note / 32] |= bit;
	else
		kb->held[note / 32] &= ~bit;
}

bool ctui_keyboard_is_held(const ctui_keyboard *kb, int note)
{
	if (note < 0 || note >= CTUI_KEYBOARD_NOTES)
		return false;
	return (kb->held[note / 32] >> (unsigned)(note % 32)) & 1u;
}

int ctui_keyboard_held_count(const ctui_keyboard *kb)
{
	int n = 0;
	for (int i = 0; i < CTUI_KEYBOARD_NOTES; i++)
		n += ctui_keyboard_is_held(kb, i);
	return n;
}

void ctui_keyboard_release_all(ctui_keyboard *kb)
{
	for (size_t i = 0; i < sizeof kb->held / sizeof kb->held[0]; i++)
		kb->held[i] = 0;
	kb->mouse_note = -1;
	kb->mouse_latched = false;
}

void ctui_keyboard_clear_selection(ctui_keyboard *kb)
{
	kb->selection = false;
}

bool ctui_keyboard_move(ctui_keyboard *kb, int delta, bool extend)
{
	int to = clampi(kb->cursor + delta, kb->first, ctui_keyboard_last(kb));
	if (extend && !kb->selection)
		kb->anchor = kb->cursor;
	bool moved = to != kb->cursor;
	kb->cursor = to;
	if (extend) {
		kb->selection = true;
		kb->sel_first = kb->anchor < to ? kb->anchor : to;
		kb->sel_last = kb->anchor < to ? to : kb->anchor;
	} else {
		kb->selection = false;
	}
	return moved;
}

static void trigger(ctui_keyboard *kb, int note)
{
	kb->cursor = note;
	kb->last = note;
	ctui_keyboard_set_held(kb, note, true);
}

bool ctui_keyboard_key(ctui_keyboard *kb, int key)
{
	switch (key) {
	case KEY_LEFT:
		ctui_keyboard_move(kb, -1, false);
		return true;
	case KEY_RIGHT:
		ctui_keyboard_move(kb, 1, false);
		return true;
	case KEY_UP:
		ctui_keyboard_move(kb, 12, false);
		return true;
	case KEY_DOWN:
		ctui_keyboard_move(kb, -12, false);
		return true;
	case KEY_HOME:
		ctui_keyboard_move(kb, -CTUI_KEYBOARD_NOTES, false);
		return true;
	case KEY_END:
		ctui_keyboard_move(kb, CTUI_KEYBOARD_NOTES, false);
		return true;
	case KEY_SLEFT:
		ctui_keyboard_move(kb, -1, true);
		return true;
	case KEY_SRIGHT:
		ctui_keyboard_move(kb, 1, true);
		return true;
	case '\n':
	case KEY_ENTER:
	case ' ':
		if (ctui_keyboard_is_held(kb, kb->cursor)) {
			ctui_keyboard_set_held(kb, kb->cursor, false);
		} else {
			trigger(kb, kb->cursor);
		}
		return true;
	case 'z':
		if (kb->base >= 12)
			kb->base -= 12;
		return true;
	case 'x':
		if (kb->base + 12 < CTUI_KEYBOARD_NOTES)
			kb->base += 12;
		return true;
	default:
		break;
	}
	int note = ctui_keyboard_key_note(key, kb->base);
	if (note < 0 || !ctui_keyboard_contains(kb, note))
		return false;
	kb->selection = false;
	trigger(kb, note);
	return true;
}

/* Let go of the note the mouse holds, unless it was already held by
 * something else when the mouse took it. */
static void let_go_mouse_note(ctui_keyboard *kb)
{
	if (kb->mouse_note >= 0 && !kb->mouse_latched)
		ctui_keyboard_set_held(kb, kb->mouse_note, false);
}

/* The mouse takes `note`, remembering whether it was held already. */
static void take_mouse_note(ctui_keyboard *kb, int note)
{
	kb->mouse_latched = ctui_keyboard_is_held(kb, note);
	trigger(kb, note);
	kb->mouse_note = note;
}

bool ctui_keyboard_press(ctui_keyboard *kb, int y, int x, int height)
{
	int note = ctui_keyboard_note_at(kb, y, x, height);
	if (note < 0)
		return false;
	let_go_mouse_note(kb);
	kb->selection = false;
	take_mouse_note(kb, note);
	kb->dragging = true;
	return true;
}

bool ctui_keyboard_drag(ctui_keyboard *kb, int y, int x, int height)
{
	if (!kb->dragging)
		return false;
	int note = ctui_keyboard_note_at(kb, y, x, height);
	if (note < 0 || note == kb->mouse_note)
		return true;
	let_go_mouse_note(kb);
	take_mouse_note(kb, note);
	return true;
}

bool ctui_keyboard_release(ctui_keyboard *kb)
{
	bool was = kb->dragging;
	let_go_mouse_note(kb);
	kb->mouse_note = -1;
	kb->dragging = false;
	return was;
}

/* The look of a key from its state. */
static int key_attr(const ctui_keyboard *kb, int note)
{
	bool black = ctui_keyboard_is_black(note);
	bool held = ctui_keyboard_is_held(kb, note);
	bool selected =
	    kb->selection && note >= kb->sel_first && note <= kb->sel_last;
	if (!ctui_theme_has_colors()) {
		int base = black ? 0 : (int)A_REVERSE;
		if (held)
			return black ? (int)A_REVERSE : 0;
		return selected ? (int)((unsigned)base | (unsigned)A_UNDERLINE)
		       : note == kb->cursor
			   ? (int)((unsigned)base | (unsigned)A_BOLD)
			   : base;
	}
	const ctui_palette *p = ctui_theme_palette();
	if (held)
		return ctui_attr(CTUI_PAIR_SELECTION, false);
	if (selected)
		return ctui_attr_color(p->bar, p->yellow);
	if (note == kb->cursor)
		return ctui_attr_color(p->bar, p->cyan);
	return black ? ctui_attr_color(p->text, p->black)
		     : ctui_attr_color(p->bar, p->text);
}

/* What fills a key cell: the colour tells the key apart, the glyph only
 * where colour cannot (ASCII, no colours). */
static const char *fill_glyph(bool black, bool held)
{
	bool unicode = ctui_theme_unicode(), colors = ctui_theme_has_colors();
	if (black)
		return unicode && colors ? " "
		       : unicode	 ? "\xe2\x96\x92"
		       : held		 ? "@"
					 : "#";
	return unicode || !held ? " " : "=";
}

static void keyboard_draw_impl(const ctui_keyboard *kb, int y, int x, int width,
			       int height)
{
	if (width < 1 || height < CTUI_KEYBOARD_MIN_HEIGHT)
		return;
	const ctui_glyphs *g = ctui_theme_glyphs();
	int kw = key_width(kb), cols = ctui_keyboard_width(kb);
	if (cols > width)
		cols = width;
	for (int row = 0; row < height; row++)
		for (int col = 0; col < cols; col++) {
			int note = ctui_keyboard_note_at(kb, row, col, height);
			if (note < 0)
				continue;
			bool black = ctui_keyboard_is_black(note);
			const char *glyph =
			    fill_glyph(black, ctui_keyboard_is_held(kb, note));
			if (!black && col % kw == kw - 1)
				glyph = g->box_v;
			ctui_put(y + row, x + col, 1, glyph,
				 key_attr(kb, note));
		}
	/* the bottom row names the C keys and the cursor key; a black key has
	 * no cell of its own there, so its name starts where the key does */
	char name[8];
	for (int note = kb->first; note <= ctui_keyboard_last(kb); note++) {
		bool black = ctui_keyboard_is_black(note);
		if (note % 12 != 0 && note != kb->cursor)
			continue;
		int left = ctui_keyboard_key_x(kb, note, NULL);
		int room = black ? 3 : kw - 1;
		if (left < 0) {
			room += left;
			left = 0;
		}
		if (left < width && room > 0)
			ctui_put(y + height - 1, x + left,
				 room < width - left ? room : width - left,
				 ctui_keyboard_name(name, sizeof name, note),
				 (int)((unsigned)key_attr(kb, note) |
				       (unsigned)A_BOLD));
	}
}

void ctui_keyboard_draw(const ctui_keyboard *kb, int y, int x, int width,
			int height)
{
	ctui_style_begin(kb->style);
	keyboard_draw_impl(kb, y, x, width, height);
	ctui_style_end();
}
