/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * input.c - single-line text input (input.h).
 */
#include <ctui/input.h>

#include <curses.h>
#include <stdio.h>
#include <string.h>

#include <ctui/inputrule.h>
#include <ctui/text.h>
#include <ctui/theme.h>

#define MASK_GLYPH "*"
#define RULE_BUF 512 /* bytes of text a rule lays out */

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

/* ---- rule ------------------------------------------------------- */

static bool rule_edits(const ctui_input *in)
{
	return in->rule && (in->rule->accept || in->rule->max_length ||
			    in->rule->separators || in->rule->format);
}

/* Characters before `offset` that are not separators of the rule. */
static size_t significant_before(const ctui_input *in, size_t offset)
{
	size_t n = 0;
	for (size_t i = 0; i < offset && i < len_of(in); i++)
		if (!ctui_rule_is_separator(in->rule,
					    (unsigned char)in->text[i]))
			n++;
	return n;
}

/* Lay the text out as the rule wants, the cursor staying after the same
 * number of characters that are not separators. */
static void reformat(ctui_input *in)
{
	if (!in->rule || !in->rule->format || !in->capacity ||
	    len_of(in) >= RULE_BUF)
		return;
	char compact[RULE_BUF], shown[RULE_BUF];
	size_t before = significant_before(in, in->cursor);
	ctui_rule_compact(in->rule, in->text, compact, sizeof compact);
	ctui_rule_format(in->rule, compact, shown, sizeof shown);
	size_t n = strlen(shown);
	if (n + 1 > in->capacity)
		return;
	memcpy(in->text, shown, n + 1);
	size_t pos = 0, seen = 0;
	while (shown[pos] && seen < before) {
		if (!ctui_rule_is_separator(in->rule,
					    (unsigned char)shown[pos]))
			seen++;
		pos++;
	}
	in->cursor = pos;
}

void ctui_input_set_rule(ctui_input *in, const ctui_input_rule *rule)
{
	in->rule = rule;
	reformat(in);
	in->cursor = len_of(in);
	in->scroll = 0;
}

ctui_input_state ctui_input_state_of(const ctui_input *in)
{
	if (len_of(in) == 0)
		return CTUI_INPUT_EMPTY;
	char compact[RULE_BUF];
	if (len_of(in) >= RULE_BUF)
		return CTUI_INPUT_INVALID;
	ctui_rule_compact(in->rule, in->text, compact, sizeof compact);
	return ctui_rule_valid(in->rule, compact) ? CTUI_INPUT_VALID
						  : CTUI_INPUT_INVALID;
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
		if (rule_edits(in) && n == 1 && printable(cp)) {
			int ch = (unsigned char)*s;
			if (ctui_rule_is_separator(in->rule, ch)) {
				if (in->rule->format) { /* the layout adds it */
					s++;
					continue;
				}
			} else {
				char compact[RULE_BUF];
				ctui_rule_compact(in->rule, in->text, compact,
						  sizeof compact);
				if (!ctui_rule_accepts(
					in->rule, compact,
					significant_before(in, in->cursor), ch))
					break;
			}
		} else if (rule_edits(in) && n > 1 &&
			   (in->rule->accept || in->rule->max_length)) {
			break; /* a rule with a charset takes ASCII only */
		}
		if (!printable(cp) || (cp == 0xFFFD && n == 1) ||
		    !insert_char(in, s, n))
			break;
		any = true;
		s += n;
	}
	if (any)
		reformat(in);
	return any;
}

static bool remove_range(ctui_input *in, size_t from, size_t to)
{
	if (from >= to)
		return false;
	memmove(in->text + from, in->text + to, len_of(in) - to + 1);
	in->cursor = from;
	reformat(in);
	return true;
}

/* Whether the separators of the rule are laid out by it (and so not edited
 * one by one). */
static bool laid_out(const ctui_input *in, size_t offset)
{
	return in->rule && in->rule->format && offset < len_of(in) &&
	       ctui_rule_is_separator(in->rule,
				      (unsigned char)in->text[offset]);
}

bool ctui_input_backspace(ctui_input *in)
{
	size_t to = in->cursor;
	/* the separators laid out by the rule go with the character before */
	while (to > 0 && laid_out(in, to - 1))
		to--;
	if (to == 0)
		return false;
	return remove_range(in, ctui_input_prev_char(in, to), to);
}

bool ctui_input_delete(ctui_input *in)
{
	size_t at = in->cursor;
	while (laid_out(in, at))
		at++;
	return remove_range(in, at, ctui_input_next_char(in, at));
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

static bool masked(const ctui_input *in)
{
	return in->mask || (in->rule && in->rule->mask);
}

static const char *placeholder_of(const ctui_input *in)
{
	return in->placeholder ? in->placeholder
			       : (in->rule ? in->rule->hint : NULL);
}

static int char_cols(const ctui_input *in, unsigned long cp)
{
	return masked(in) ? 1 : ctui_codepoint_width(cp);
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
	/* a rule that can say whether the text is right gets a mark at the end
	 */
	int full = width;
	if (in->rule && in->rule->valid && width > 3)
		width--;
	ctui_input_fit(in, width);
	int attr = ctui_attr(focused ? CTUI_PAIR_SELECTION : CTUI_PAIR_DIALOG,
			     focused);
	ctui_fill(y, x, full, attr);
	if (width < full) {
		ctui_input_state state = ctui_input_state_of(in);
		const ctui_glyphs *g = ctui_theme_glyphs();
		if (state == CTUI_INPUT_VALID)
			ctui_put(y, x + width, 1, g->check,
				 ctui_attr(CTUI_PAIR_GAUGE_GREEN, true));
		else if (state == CTUI_INPUT_INVALID)
			ctui_put(y, x + width, 1, g->cross,
				 ctui_attr(CTUI_PAIR_ERROR, true));
	}
	size_t len = len_of(in);
	if (len == 0 && placeholder_of(in))
		ctui_put(y, x, width, placeholder_of(in),
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
		if (masked(in)) {
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
		if (len == 0 && placeholder_of(in) && *placeholder_of(in)) {
			unsigned long cp;
			size_t n = ctui_utf8_decode(placeholder_of(in), &cp);
			if (n < sizeof under) {
				memcpy(under, placeholder_of(in), n);
				under[n] = '\0';
			}
		} else if (in->cursor < len) {
			if (masked(in)) {
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
