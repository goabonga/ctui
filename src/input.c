/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * input.c - single-line text input (input.h).
 */
#include <ctui/input.h>

#include <curses.h>
#include <stdio.h>
#include <string.h>

#include <ctui/text.h>
#include <ctui/theme.h>

#define MASK_GLYPH "*"

/* ---- characters ------------------------------------------------- */

static size_t len_of(const ctui_input *in)
{
	return in->capacity ? strlen(in->text) : 0;
}

/* True for a code point that may be typed. */
static bool printable(unsigned long cp)
{
	return cp >= 32 && cp != 127 && !(cp >= 0x80 && cp < 0xA0) &&
	       cp <= 0x10FFFF && !(cp >= 0xD800 && cp < 0xE000);
}

static bool blank(char c)
{
	return c == ' ' || c == '\t';
}

size_t ctui_input_prev_char(const ctui_input *in, size_t offset)
{
	size_t len = len_of(in);
	if (offset > len)
		offset = len;
	if (offset == 0)
		return 0;
	size_t i = offset - 1;
	int back = 0;
	while (i > 0 && back < 3 &&
	       ((unsigned)(unsigned char)in->text[i] & 0xC0U) == 0x80U) {
		i--;
		back++;
	}
	/* Only accept the start found when the character really ends there. */
	unsigned long cp;
	if (((unsigned)(unsigned char)in->text[i] & 0xC0U) != 0x80U &&
	    i + ctui_utf8_decode(in->text + i, &cp) == offset)
		return i;
	return offset - 1;
}

size_t ctui_input_next_char(const ctui_input *in, size_t offset)
{
	size_t len = len_of(in);
	if (offset >= len)
		return len;
	unsigned long cp;
	size_t n = ctui_utf8_decode(in->text + offset, &cp);
	return offset + n > len ? len : offset + n;
}

size_t ctui_input_prev_word(const ctui_input *in, size_t offset)
{
	size_t i = offset > len_of(in) ? len_of(in) : offset;
	while (i > 0 && blank(in->text[i - 1]))
		i--;
	while (i > 0 && !blank(in->text[i - 1]))
		i = ctui_input_prev_char(in, i);
	return i;
}

size_t ctui_input_next_word(const ctui_input *in, size_t offset)
{
	size_t len = len_of(in);
	size_t i = offset > len ? len : offset;
	while (i < len && !blank(in->text[i]))
		i = ctui_input_next_char(in, i);
	while (i < len && blank(in->text[i]))
		i++;
	return i;
}

/* ---- setup ------------------------------------------------------ */

void ctui_input_init(ctui_input *in, char *buffer, size_t capacity)
{
	*in = (ctui_input){.text = buffer, .capacity = capacity};
	if (capacity) {
		buffer[capacity - 1] = '\0';
		in->cursor = strlen(buffer);
	}
}

size_t ctui_input_length(const ctui_input *in)
{
	size_t n = 0;
	for (size_t i = 0; i < len_of(in); i = ctui_input_next_char(in, i))
		n++;
	return n;
}

void ctui_input_set(ctui_input *in, const char *text)
{
	in->cursor = in->scroll = 0;
	in->pending_len = 0;
	if (!in->capacity)
		return;
	in->text[0] = '\0';
	ctui_input_insert(in, text ? text : "");
}

/* ---- editing ---------------------------------------------------- */

static bool insert_char(ctui_input *in, const char *s, size_t n)
{
	if (!in->capacity || len_of(in) + n + 1 > in->capacity)
		return false;
	if (in->max_chars && ctui_input_length(in) >= in->max_chars)
		return false;
	size_t len = len_of(in);
	memmove(in->text + in->cursor + n, in->text + in->cursor,
		len - in->cursor + 1);
	memcpy(in->text + in->cursor, s, n);
	in->cursor += n;
	return true;
}

bool ctui_input_insert(ctui_input *in, const char *s)
{
	bool any = false;
	// NOLINTNEXTLINE(clang-analyzer-security.ArrayBound)
	while (s && *s) {
		unsigned long cp;
		size_t n = ctui_utf8_decode(s, &cp);
		if (!printable(cp) || (cp == 0xFFFD && n == 1) ||
		    !insert_char(in, s, n))
			break;
		any = true;
		s += n;
	}
	return any;
}

static bool remove_range(ctui_input *in, size_t from, size_t to)
{
	if (from >= to)
		return false;
	memmove(in->text + from, in->text + to, len_of(in) - to + 1);
	in->cursor = from;
	return true;
}

bool ctui_input_backspace(ctui_input *in)
{
	return remove_range(in, ctui_input_prev_char(in, in->cursor),
			    in->cursor);
}

bool ctui_input_delete(ctui_input *in)
{
	size_t at = in->cursor;
	if (!remove_range(in, at, ctui_input_next_char(in, at)))
		return false;
	return true;
}

bool ctui_input_delete_word(ctui_input *in)
{
	return remove_range(in, ctui_input_prev_word(in, in->cursor),
			    in->cursor);
}

bool ctui_input_kill_start(ctui_input *in)
{
	return remove_range(in, 0, in->cursor);
}

bool ctui_input_kill_end(ctui_input *in)
{
	return remove_range(in, in->cursor, len_of(in));
}

static bool move_to(ctui_input *in, size_t offset)
{
	bool moved = offset != in->cursor;
	in->cursor = offset;
	return moved;
}

bool ctui_input_left(ctui_input *in)
{
	return move_to(in, ctui_input_prev_char(in, in->cursor));
}

bool ctui_input_right(ctui_input *in)
{
	return move_to(in, ctui_input_next_char(in, in->cursor));
}

bool ctui_input_word_left(ctui_input *in)
{
	return move_to(in, ctui_input_prev_word(in, in->cursor));
}

bool ctui_input_word_right(ctui_input *in)
{
	return move_to(in, ctui_input_next_word(in, in->cursor));
}

bool ctui_input_home(ctui_input *in)
{
	return move_to(in, 0);
}

bool ctui_input_end(ctui_input *in)
{
	return move_to(in, len_of(in));
}

/* Collect the bytes of a typed UTF-8 sequence; insert it when whole. */
static ctui_input_event type_byte(ctui_input *in, int key)
{
	unsigned char b = (unsigned char)key;
	if (in->pending_len == 0) {
		if (b < 0x80) {
			char one[2] = {(char)b, '\0'};
			return key >= 32 && key != 127 &&
				       ctui_input_insert(in, one)
				   ? CTUI_INPUT_CHANGED
				   : CTUI_INPUT_NONE;
		}
		if (b < 0xC2 || b > 0xF4)
			return CTUI_INPUT_NONE; /* stray continuation byte or
						   overlong lead */
	} else if (((unsigned)b & 0xC0U) != 0x80U) {
		in->pending_len = 0; /* the sequence was cut: start again */
		return type_byte(in, key);
	}
	in->pending[in->pending_len++] = b;
	int want = in->pending[0] >= 0xF0 ? 4 : in->pending[0] >= 0xE0 ? 3 : 2;
	if (in->pending_len < want)
		return CTUI_INPUT_NONE;
	char seq[5] = {0};
	memcpy(seq, in->pending, (size_t)want);
	in->pending_len = 0;
	return ctui_input_insert(in, seq) ? CTUI_INPUT_CHANGED
					  : CTUI_INPUT_NONE;
}

ctui_input_event ctui_input_key(ctui_input *in, int key)
{
	switch (key) {
	case KEY_LEFT:
		return ctui_input_left(in) ? CTUI_INPUT_MOVED : CTUI_INPUT_NONE;
	case KEY_RIGHT:
		return ctui_input_right(in) ? CTUI_INPUT_MOVED
					    : CTUI_INPUT_NONE;
	case KEY_SLEFT:
		return ctui_input_word_left(in) ? CTUI_INPUT_MOVED
						: CTUI_INPUT_NONE;
	case KEY_SRIGHT:
		return ctui_input_word_right(in) ? CTUI_INPUT_MOVED
						 : CTUI_INPUT_NONE;
	case KEY_HOME:
	case 1: /* Ctrl-A */
		return ctui_input_home(in) ? CTUI_INPUT_MOVED : CTUI_INPUT_NONE;
	case KEY_END:
	case 5: /* Ctrl-E */
		return ctui_input_end(in) ? CTUI_INPUT_MOVED : CTUI_INPUT_NONE;
	case KEY_BACKSPACE:
	case 127:
	case 8:
		return ctui_input_backspace(in) ? CTUI_INPUT_CHANGED
						: CTUI_INPUT_NONE;
	case KEY_DC:
		return ctui_input_delete(in) ? CTUI_INPUT_CHANGED
					     : CTUI_INPUT_NONE;
	case 23: /* Ctrl-W */
		return ctui_input_delete_word(in) ? CTUI_INPUT_CHANGED
						  : CTUI_INPUT_NONE;
	case 21: /* Ctrl-U */
		return ctui_input_kill_start(in) ? CTUI_INPUT_CHANGED
						 : CTUI_INPUT_NONE;
	case 11: /* Ctrl-K */
		return ctui_input_kill_end(in) ? CTUI_INPUT_CHANGED
					       : CTUI_INPUT_NONE;
	case '\n':
	case '\r':
	case KEY_ENTER:
		return CTUI_INPUT_SUBMITTED;
	case 27:
		return CTUI_INPUT_CANCELLED;
	default:
		if (key >= 32 && key <= 255)
			return type_byte(in, key);
		return CTUI_INPUT_NONE;
	}
}

/* ---- display ---------------------------------------------------- */

static int char_cols(const ctui_input *in, unsigned long cp)
{
	return in->mask ? 1 : ctui_codepoint_width(cp);
}

size_t ctui_input_columns(const ctui_input *in, size_t from, size_t to)
{
	size_t len = len_of(in), cols = 0;
	if (to > len)
		to = len;
	while (from < to) {
		unsigned long cp;
		size_t n = ctui_utf8_decode(in->text + from, &cp);
		cols += (size_t)char_cols(in, cp);
		from += n;
	}
	return cols;
}

/* Columns of the cell the cursor sits on: the character under it, or a
 * blank at the end. */
static int cursor_cell(const ctui_input *in)
{
	if (in->cursor >= len_of(in))
		return 1;
	unsigned long cp;
	ctui_utf8_decode(in->text + in->cursor, &cp);
	int w = char_cols(in, cp);
	return w < 1 ? 1 : w;
}

void ctui_input_fit(ctui_input *in, int width)
{
	size_t len = len_of(in);
	if (in->cursor > len)
		in->cursor = len;
	if (in->scroll > len)
		in->scroll = len;
	if (width < 1)
		return;
	if (in->cursor < in->scroll)
		in->scroll = in->cursor;
	while (in->scroll < in->cursor &&
	       (int)ctui_input_columns(in, in->scroll, in->cursor) +
		       cursor_cell(in) >
		   width)
		in->scroll = ctui_input_next_char(in, in->scroll);
}

bool ctui_input_click(ctui_input *in, int width, int x)
{
	ctui_input_fit(in, width);
	size_t len = len_of(in), i = in->scroll;
	int col = 0;
	while (i < len) {
		unsigned long cp;
		size_t n = ctui_utf8_decode(in->text + i, &cp);
		int w = char_cols(in, cp);
		if (x < col + w)
			break;
		col += w;
		i += n;
	}
	return move_to(in, i);
}

void ctui_input_draw(ctui_input *in, int y, int x, int width, bool focused)
{
	if (width < 1)
		return;
	ctui_style_begin(in->style);
	ctui_input_fit(in, width);
	int attr = ctui_attr(focused ? CTUI_PAIR_SELECTION : CTUI_PAIR_DIALOG,
			     focused);
	ctui_fill(y, x, width, attr);
	size_t len = len_of(in);
	if (len == 0 && in->placeholder)
		ctui_put(y, x, width, in->placeholder,
			 (int)((unsigned)attr | (unsigned)A_DIM));
	char shown[CTUI_TEXT_MAX];
	size_t out = 0, i = in->scroll;
	int cols = 0, cursor_col = -1;
	while (i < len && out + 5 < sizeof shown) {
		unsigned long cp;
		size_t n = ctui_utf8_decode(in->text + i, &cp);
		int w = char_cols(in, cp);
		if (cols + w > width)
			break;
		if (i == in->cursor)
			cursor_col = cols;
		if (in->mask) {
			shown[out++] = MASK_GLYPH[0];
		} else {
			memcpy(shown + out, in->text + i, n);
			out += n;
		}
		cols += w;
		i += n;
	}
	shown[out] = '\0';
	if (out)
		ctui_put(y, x, width, shown, attr);
	if (i >= len && in->cursor == len && cols < width)
		cursor_col = cols;
	if (focused && cursor_col >= 0) {
		char under[8] = " ";
		if (len == 0 && in->placeholder && *in->placeholder) {
			unsigned long cp;
			size_t n = ctui_utf8_decode(in->placeholder, &cp);
			if (n < sizeof under) {
				memcpy(under, in->placeholder, n);
				under[n] = '\0';
			}
		} else if (in->cursor < len) {
			if (in->mask) {
				under[0] = MASK_GLYPH[0];
				under[1] = '\0';
			} else {
				unsigned long cp;
				size_t n = ctui_utf8_decode(
				    in->text + in->cursor, &cp);
				memcpy(under, in->text + in->cursor, n);
				under[n] = '\0';
			}
		}
		ctui_put(y, x + cursor_col, width - cursor_col, under,
			 (int)((unsigned)attr | (unsigned)A_REVERSE));
	}
	ctui_style_end();
}
