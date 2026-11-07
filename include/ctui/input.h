/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * input.h - a single-line text input usable on its own or beside a form.
 *
 * The text lives in a caller buffer (UTF-8, NUL-terminated); the cursor and
 * the scroll position are byte offsets on character boundaries. The editing
 * operations are pure (no screen) and return true when they changed
 * something. ctui_form keeps its own append-only text field: the input is the
 * editor with a cursor.
 *
 * Keys (ctui_input_key): Left / Right move by a character, Shift-Left /
 * Shift-Right (KEY_SLEFT / KEY_SRIGHT) by a word, Home / Ctrl-A and End /
 * Ctrl-E to the ends; Backspace and Delete remove a character, Ctrl-W the
 * word before the cursor, Ctrl-U everything before it and Ctrl-K everything
 * after it. Enter submits and Esc cancels. Typed bytes of a UTF-8 sequence
 * (curses delivers them one by one) are collected until the character is
 * whole. There is no selection.
 *
 * `mask` shows one '*' per character, `max_chars` bounds the length in
 * characters (0 = only the buffer), and `placeholder` is drawn muted while
 * the text is empty. The text scrolls sideways to keep the cursor in view.
 * The mouse sets the cursor with ctui_input_click(); coordinates are relative
 * to the top-left cell of the field as drawn.
 */
#ifndef CTUI_INPUT_H
#define CTUI_INPUT_H

#include <stdbool.h>
#include <stddef.h>

#include <ctui/inputrule.h>
#include <ctui/theme.h>

typedef struct ctui_input {
	char *text;	  /* caller buffer, NUL-terminated */
	size_t capacity;  /* bytes, including the NUL */
	size_t cursor;	  /* byte offset, on a character boundary */
	size_t scroll;	  /* byte offset of the first visible character */
	bool mask;	  /* show '*' instead of the characters */
	size_t max_chars; /* 0 = limited by capacity only */
	const char *placeholder; /* shown while empty, may be NULL */
	const ctui_input_rule
	    *rule;		  /* see ctui_input_set_rule(), may be NULL */
	unsigned char pending[4]; /* bytes of a UTF-8 sequence being typed */
	int pending_len;
	const ctui_style
	    *style; /* colours of this instance, may be NULL; set after init */
} ctui_input;

typedef enum ctui_input_event {
	CTUI_INPUT_NONE = 0,
	CTUI_INPUT_MOVED,   /* the cursor moved */
	CTUI_INPUT_CHANGED, /* the text changed */
	CTUI_INPUT_SUBMITTED,
	CTUI_INPUT_CANCELLED
} ctui_input_event;

/* Use buffer (capacity bytes) as the text, which must be valid UTF-8 or
 * empty; the cursor goes to the end. A zero capacity gives an input that
 * accepts no text. */
void ctui_input_init(ctui_input *in, char *buffer, size_t capacity);

typedef enum ctui_input_state {
	CTUI_INPUT_EMPTY = 0,
	CTUI_INPUT_INVALID, /* text the rule does not accept (yet) */
	CTUI_INPUT_VALID    /* text, and the rule accepts it */
} ctui_input_state;

/* Follow a rule (inputrule.h): only the characters it accepts can be typed,
 * the text is laid out as it wants (the cursor stays on the same character),
 * its mask and its hint apply, and Backspace / Delete take the laid out
 * separators with the character next to them. The text already there is laid
 * out at once; NULL gives the rule up. A rule with a `valid` callback makes
 * the input draw a check mark or a cross at its right end. */
void ctui_input_set_rule(ctui_input *in, const ctui_input_rule *rule);

/* Whether the text is empty, accepted by the rule (always without one) or
 * not. */
ctui_input_state ctui_input_state_of(const ctui_input *in);

/* Characters in the text. */
size_t ctui_input_length(const ctui_input *in);

/* Replace the text (cut where it stops fitting); cursor to the end. */
void ctui_input_set(ctui_input *in, const char *text);

/* ---- editing: pure, on byte offsets ---- */

/* Insert the well-formed printable characters of s at the cursor, stopping
 * at the first one that does not fit or is rejected (control characters,
 * invalid UTF-8). True when something was inserted. */
bool ctui_input_insert(ctui_input *in, const char *s);

bool ctui_input_backspace(ctui_input *in);
bool ctui_input_delete(ctui_input *in);
bool ctui_input_delete_word(ctui_input *in); /* word before the cursor */
bool ctui_input_kill_start(ctui_input *in);  /* everything before the cursor */
bool ctui_input_kill_end(ctui_input *in);    /* everything after the cursor */

/* Offsets of the character before / after `offset`, of the start of the
 * word before it and of the start of the next word (or the end). */
size_t ctui_input_prev_char(const ctui_input *in, size_t offset);
size_t ctui_input_next_char(const ctui_input *in, size_t offset);
size_t ctui_input_prev_word(const ctui_input *in, size_t offset);
size_t ctui_input_next_word(const ctui_input *in, size_t offset);

/* Move the cursor; true when it moved. */
bool ctui_input_left(ctui_input *in);
bool ctui_input_right(ctui_input *in);
bool ctui_input_word_left(ctui_input *in);
bool ctui_input_word_right(ctui_input *in);
bool ctui_input_home(ctui_input *in);
bool ctui_input_end(ctui_input *in);

/* Apply a key (see above). */
ctui_input_event ctui_input_key(ctui_input *in, int key);

/* ---- display ---- */

/* Columns of the text between two byte offsets, as drawn (a masked
 * character is one column). */
size_t ctui_input_columns(const ctui_input *in, size_t from, size_t to);

/* Scroll so that the cursor is inside a field of width columns. */
void ctui_input_fit(ctui_input *in, int width);

/* Click at column x of a field width columns wide: the cursor goes to the
 * character under it (the end when past the text). True when it moved. */
bool ctui_input_click(ctui_input *in, int width, int x);

/* Draw in width columns at (y, x); the cursor is drawn when focused. This
 * updates the scroll position. */
void ctui_input_draw(ctui_input *in, int y, int x, int width, bool focused);

#endif /* CTUI_INPUT_H */
