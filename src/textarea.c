/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * textarea.c - a multi-line UTF-8 text editor (textarea.h).
 */
#include <ctui/textarea.h>

#include <curses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ctui/text.h>
#include <ctui/textview.h>

/* ---- buffer and line index ---- */

static size_t count_newlines(const char *s, size_t n)
{
	size_t k = 0;
	for (const char *p = s, *end = s + n;
	     (p = memchr(p, '\n', (size_t)(end - p))); p++)
		k++;
	return k;
}

static bool reserve_text(ctui_textarea *t, size_t need)
{
	if (need + 1 <= t->cap)
		return true;
	size_t cap = t->cap ? t->cap : 64;
	while (cap < need + 1) {
		if (cap > ((size_t)-1) / 2)
			return false;
		cap *= 2;
	}
	char *p = realloc(t->text, cap);
	if (!p)
		return false;
	t->text = p;
	t->cap = cap;
	return true;
}

static bool reserve_lines(ctui_textarea *t, size_t lines)
{
	if (lines <= t->starts_cap)
		return true;
	size_t cap = t->starts_cap ? t->starts_cap : 16;
	while (cap < lines) {
		if (cap > ((size_t)-1) / 2 / sizeof *t->starts)
			return false;
		cap *= 2;
	}
	size_t *p = realloc(t->starts, cap * sizeof *p);
	if (!p)
		return false;
	t->starts = p;
	t->starts_cap = cap;
	return true;
}

/* Rebuild the line index; the room was reserved by reserve_lines(). */
static void index_lines(ctui_textarea *t)
{
	size_t n = 0;
	t->starts[n++] = 0;
	for (size_t i = 0; i < t->len; i++)
		if (t->text[i] == '\n')
			t->starts[n++] = i + 1;
	t->line_count = n;
}

/* Replace del bytes at pos by n bytes of ins, memory permitting. Moves
 * nothing else: no cursor, no history, no limit. ins may not alias text. */
static bool splice(ctui_textarea *t, size_t pos, size_t del, const char *ins,
		   size_t n)
{
	size_t lines = t->line_count - count_newlines(t->text + pos, del) +
		       count_newlines(ins, n);
	if (!reserve_text(t, t->len - del + n) || !reserve_lines(t, lines))
		return false;
	memmove(t->text + pos + n, t->text + pos + del, t->len - pos - del);
	memcpy(t->text + pos, ins, n);
	t->len = t->len - del + n;
	t->text[t->len] = '\0';
	index_lines(t);
	return true;
}

bool ctui_textarea_init(ctui_textarea *t, const char *initial)
{
	*t = (ctui_textarea){0};
	initial = initial ? initial : "";
	size_t n = strlen(initial);
	t->line_count = 1;
	if (!reserve_text(t, n) ||
	    !reserve_lines(t, count_newlines(initial, n) + 1)) {
		free(t->text);
		free(t->starts);
		*t = (ctui_textarea){0};
		return false;
	}
	memcpy(t->text, initial, n + 1);
	t->len = n;
	index_lines(t);
	return true;
}

static void edit_free(ctui_textarea_edit *e)
{
	free(e->removed);
	free(e->added);
	*e = (ctui_textarea_edit){0};
}

static void stack_clear(ctui_textarea_edit *stack, size_t *len)
{
	while (*len)
		edit_free(&stack[--*len]);
}

static bool stack_reserve(ctui_textarea_edit **stack, size_t *cap, size_t need)
{
	if (need <= *cap)
		return true;
	size_t n = *cap ? *cap * 2 : 16;
	if (n < need)
		n = need;
	ctui_textarea_edit *p = realloc(*stack, n * sizeof *p);
	if (!p)
		return false;
	*stack = p;
	*cap = n;
	return true;
}

void ctui_textarea_free(ctui_textarea *t)
{
	stack_clear(t->undo, &t->undo_len);
	stack_clear(t->redo, &t->redo_len);
	free(t->undo);
	free(t->redo);
	free(t->clip);
	free(t->text);
	free(t->starts);
	*t = (ctui_textarea){0};
}

const char *ctui_textarea_text(const ctui_textarea *t)
{
	return t->text ? t->text : "";
}

size_t ctui_textarea_length(const ctui_textarea *t)
{
	return t->len;
}

const char *ctui_textarea_line(const ctui_textarea *t, size_t i, size_t *len)
{
	if (i >= t->line_count)
		i = t->line_count - 1;
	size_t start = t->starts[i];
	*len = (i + 1 < t->line_count ? t->starts[i + 1] - 1 : t->len) - start;
	return t->text + start;
}

void ctui_textarea_locate(const ctui_textarea *t, size_t offset, size_t *line,
			  size_t *byte)
{
	if (offset > t->len)
		offset = t->len;
	size_t lo = 0,
	       hi = t->line_count; /* last line whose start is <= offset */
	while (hi - lo > 1) {
		size_t mid = lo + (hi - lo) / 2;
		if (t->starts[mid] <= offset)
			lo = mid;
		else
			hi = mid;
	}
	*line = lo;
	*byte = offset - t->starts[lo];
}

size_t ctui_textarea_offset(const ctui_textarea *t, size_t line, size_t byte)
{
	if (line >= t->line_count)
		line = t->line_count - 1;
	size_t len;
	ctui_textarea_line(t, line, &len);
	return t->starts[line] + (byte > len ? len : byte);
}

size_t ctui_textarea_prev(const ctui_textarea *t, size_t offset)
{
	if (offset > t->len)
		offset = t->len;
	while (offset > 0) {
		offset--;
		if (((unsigned)(unsigned char)t->text[offset] & 0xC0U) != 0x80U)
			break;
	}
	return offset;
}

size_t ctui_textarea_next(const ctui_textarea *t, size_t offset)
{
	if (offset >= t->len)
		return t->len;
	unsigned long cp;
	size_t n = ctui_utf8_decode(t->text + offset, &cp);
	return offset + n > t->len ? t->len : offset + n;
}

size_t ctui_textarea_column(const ctui_textarea *t)
{
	size_t line, byte, len;
	ctui_textarea_locate(t, t->cursor, &line, &byte);
	const char *s = ctui_textarea_line(t, line, &len);
	return ctui_line_column(s, len, byte);
}

bool ctui_textarea_range(const ctui_textarea *t, size_t *first, size_t *last)
{
	if (!t->selecting || t->anchor == t->cursor)
		return false;
	*first = t->anchor < t->cursor ? t->anchor : t->cursor;
	*last = t->anchor < t->cursor ? t->cursor : t->anchor;
	return true;
}

size_t ctui_textarea_selected_length(const ctui_textarea *t)
{
	size_t a, b;
	return ctui_textarea_range(t, &a, &b) ? b - a : 0;
}

/* ---- editing ---- */

static void set_goal(ctui_textarea *t)
{
	t->goal = ctui_textarea_column(t);
}

/* A NUL-terminated copy of n bytes (never NULL on success). */
static char *dup_bytes(const char *s, size_t n)
{
	char *p = malloc(n + 1);
	if (p) {
		memcpy(p, s, n);
		p[n] = '\0';
	}
	return p;
}

/* Room for one more undo record; the oldest goes when the history is full. */
static bool undo_room(ctui_textarea *t)
{
	size_t need = t->undo_len < CTUI_TEXTAREA_HISTORY
			  ? t->undo_len + 1
			  : CTUI_TEXTAREA_HISTORY;
	return stack_reserve(&t->undo, &t->undo_cap, need);
}

static void undo_push(ctui_textarea *t, ctui_textarea_edit e)
{
	if (t->undo_len == CTUI_TEXTAREA_HISTORY) {
		edit_free(&t->undo[0]);
		memmove(t->undo, t->undo + 1,
			(t->undo_len - 1) * sizeof *t->undo);
		t->undo_len--;
	}
	// NOLINTNEXTLINE(clang-analyzer-security.ArrayBound)
	t->undo[t->undo_len++] = e;
}

/* Replace and put the cursor at cursor_after, dropping the selection and
 * recording the change. merge extends the previous record when it was a
 * typed insertion that ends where this one starts. */
static bool apply(ctui_textarea *t, size_t pos, size_t del, const char *ins,
		  size_t n, size_t cursor_after, bool merge)
{
	if (t->read_only || pos > t->len || del > t->len - pos ||
	    (del == 0 && n == 0))
		return false;
	if (t->max_bytes && n > del && t->len - del + n > t->max_bytes)
		return false;
	ctui_textarea_edit *last =
	    t->undo_len ? &t->undo[t->undo_len - 1] : NULL;
	if (merge && t->merge && last && del == 0 &&
	    last->pos + last->added_len == pos && t->cursor == pos) {
		char *grown = realloc(last->added, last->added_len + n + 1);
		if (!grown)
			return false;
		last->added =
		    grown; /* longer than needed is fine if the splice fails */
		if (!splice(t, pos, 0, ins, n))
			return false;
		memcpy(last->added + last->added_len, ins, n);
		last->added_len += n;
		last->added[last->added_len] = '\0';
		last->after = cursor_after;
	} else {
		ctui_textarea_edit e = {.pos = pos,
					.removed_len = del,
					.added_len = n,
					.before = t->cursor,
					.after = cursor_after};
		e.removed = dup_bytes(t->text + pos, del);
		e.added = dup_bytes(ins, n);
		if (!e.removed || !e.added || !undo_room(t) ||
		    !splice(t, pos, del, ins, n)) {
			edit_free(&e);
			return false;
		}
		undo_push(t, e);
	}
	stack_clear(t->redo, &t->redo_len);
	t->merge = merge;
	t->cursor = cursor_after;
	t->selecting = false;
	t->modified = true;
	set_goal(t);
	return true;
}

bool ctui_textarea_replace(ctui_textarea *t, size_t pos, size_t del,
			   const char *ins, size_t n)
{
	size_t c = t->cursor;
	if (c >= pos + del)
		c = c - del + n;
	else if (c > pos)
		c = pos + n;
	return apply(t, pos, del, ins, n, c, false);
}

bool ctui_textarea_insert(ctui_textarea *t, size_t pos, const char *s, size_t n)
{
	return ctui_textarea_replace(t, pos, 0, s, n);
}

bool ctui_textarea_delete(ctui_textarea *t, size_t pos, size_t n)
{
	return ctui_textarea_replace(t, pos, n, "", 0);
}

bool ctui_textarea_set_text(ctui_textarea *t, const char *text)
{
	text = text ? text : "";
	size_t n = strlen(text);
	if (t->max_bytes && n > t->max_bytes)
		return false;
	/* text may point into our own buffer: copy before the splice moves it
	 */
	char *copy = malloc(n + 1);
	if (!copy)
		return false;
	memcpy(copy, text, n + 1);
	bool ok = splice(t, 0, t->len, copy, n);
	free(copy);
	if (!ok)
		return false;
	stack_clear(t->undo, &t->undo_len);
	stack_clear(t->redo, &t->redo_len);
	t->merge = false;
	t->cursor = t->anchor = 0;
	t->selecting = false;
	t->goal = 0;
	t->top = t->left = 0;
	t->modified = false;
	t->pending_len = 0;
	return true;
}

static bool step(ctui_textarea *t, bool undo)
{
	ctui_textarea_edit **from = undo ? &t->undo : &t->redo;
	size_t *from_len = undo ? &t->undo_len : &t->redo_len;
	if (t->read_only || *from_len == 0)
		return false;
	ctui_textarea_edit e = (*from)[*from_len - 1];
	bool room = undo
			? stack_reserve(&t->redo, &t->redo_cap, t->redo_len + 1)
			: undo_room(t);
	if (!room)
		return false;
	bool ok = undo ? splice(t, e.pos, e.added_len, e.removed, e.removed_len)
		       : splice(t, e.pos, e.removed_len, e.added, e.added_len);
	if (!ok)
		return false;
	--*from_len;
	if (undo)
		t->redo[t->redo_len++] = e;
	else
		undo_push(t, e);
	t->cursor = undo ? e.before : e.after;
	t->selecting = false;
	t->modified = true;
	t->merge = false;
	set_goal(t);
	return true;
}

bool ctui_textarea_undo(ctui_textarea *t)
{
	return step(t, true);
}

bool ctui_textarea_redo(ctui_textarea *t)
{
	return step(t, false);
}

const char *ctui_textarea_clipboard(const ctui_textarea *t)
{
	return t->clip ? t->clip : "";
}

bool ctui_textarea_set_clipboard(ctui_textarea *t, const char *text)
{
	size_t n = text ? strlen(text) : 0;
	char *copy = dup_bytes(text ? text : "", n);
	if (!copy)
		return false;
	free(t->clip);
	t->clip = copy;
	t->clip_len = n;
	return true;
}

bool ctui_textarea_copy(ctui_textarea *t)
{
	size_t a, b;
	if (!ctui_textarea_range(t, &a, &b))
		return false;
	char *copy = dup_bytes(t->text + a, b - a);
	if (!copy)
		return false;
	free(t->clip);
	t->clip = copy;
	t->clip_len = b - a;
	return true;
}

bool ctui_textarea_cut(ctui_textarea *t)
{
	size_t a, b;
	if (t->read_only || !ctui_textarea_range(t, &a, &b))
		return false;
	return ctui_textarea_copy(t) && apply(t, a, b - a, "", 0, a, false);
}

bool ctui_textarea_paste(ctui_textarea *t)
{
	if (t->clip_len == 0)
		return false;
	size_t a, b;
	if (!ctui_textarea_range(t, &a, &b))
		a = b = t->cursor;
	return apply(t, a, b - a, t->clip, t->clip_len, a + t->clip_len, false);
}

/* Type text at the cursor, over the selection. */
static bool type_text(ctui_textarea *t, const char *s, size_t n)
{
	size_t a, b;
	if (!ctui_textarea_range(t, &a, &b))
		a = b = t->cursor;
	bool plain = !memchr(s, '\n', n);
	return apply(t, a, b - a, s, n, a + n, plain);
}

static bool remove_char(ctui_textarea *t, bool backwards)
{
	size_t a, b;
	if (ctui_textarea_range(t, &a, &b))
		return apply(t, a, b - a, "", 0, a, false);
	size_t other = backwards ? ctui_textarea_prev(t, t->cursor)
				 : ctui_textarea_next(t, t->cursor);
	a = other < t->cursor ? other : t->cursor;
	b = other < t->cursor ? t->cursor : other;
	return apply(t, a, b - a, "", 0, a, false);
}

/* One more byte of a typed UTF-8 character; inserts it once complete. */
static bool feed_utf8(ctui_textarea *t, unsigned char b)
{
	if (t->pending_len > 0 && ((unsigned)b & 0xC0U) != 0x80U)
		t->pending_len = 0; /* the sequence was cut short */
	if (t->pending_len == 0 && (b < 0xC2 || b > 0xF4))
		return false; /* stray continuation or invalid lead */
	t->pending[t->pending_len++] = b;
	unsigned need = t->pending[0] >= 0xF0	? 4
			: t->pending[0] >= 0xE0 ? 3
						: 2;
	if (t->pending_len < need)
		return false;
	t->pending_len = 0;
	unsigned long cp;
	if (ctui_utf8_decode((const char *)t->pending, &cp) != need ||
	    cp < 0xA0)
		return false; /* malformed, or a C1 control */
	return type_text(t, (const char *)t->pending, (size_t)need);
}

/* ---- keys ---- */

static void move_to(ctui_textarea *t, size_t target, bool extend,
		    bool keep_goal)
{
	t->merge = false;
	if (extend && !t->selecting) {
		t->selecting = true;
		t->anchor = t->cursor;
	} else if (!extend) {
		t->selecting = false;
	}
	t->cursor = target;
	if (!keep_goal)
		set_goal(t);
}

static size_t vertical_target(const ctui_textarea *t, long delta)
{
	size_t line, byte, len;
	ctui_textarea_locate(t, t->cursor, &line, &byte);
	long to = (long)line + delta;
	if (to < 0)
		to = 0;
	if (to >= (long)t->line_count)
		to = (long)t->line_count - 1;
	const char *s = ctui_textarea_line(t, (size_t)to, &len);
	return t->starts[to] + ctui_line_byte(s, len, t->goal);
}

static bool navigate(ctui_textarea *t, int key, size_t rows)
{
	bool extend = false;
	switch (key) {
	case KEY_SLEFT:
		key = KEY_LEFT;
		extend = true;
		break;
	case KEY_SRIGHT:
		key = KEY_RIGHT;
		extend = true;
		break;
	case KEY_SR:
		key = KEY_UP;
		extend = true;
		break;
	case KEY_SF:
		key = KEY_DOWN;
		extend = true;
		break;
	case KEY_SHOME:
		key = KEY_HOME;
		extend = true;
		break;
	case KEY_SEND:
		key = KEY_END;
		extend = true;
		break;
	case KEY_SPREVIOUS:
		key = KEY_PPAGE;
		extend = true;
		break;
	case KEY_SNEXT:
		key = KEY_NPAGE;
		extend = true;
		break;
	default:
		break;
	}
	size_t cursor = t->cursor, anchor = t->anchor, a = 0, b = 0;
	bool had = ctui_textarea_range(t, &a, &b), keep_goal = false;
	size_t target, line, byte, len;
	ctui_textarea_locate(t, t->cursor, &line, &byte);
	long page = (long)(rows ? rows : 1);
	switch (key) {
	case KEY_LEFT:
		target = had && !extend ? a : ctui_textarea_prev(t, t->cursor);
		break;
	case KEY_RIGHT:
		target = had && !extend ? b : ctui_textarea_next(t, t->cursor);
		break;
	case KEY_UP:
		target = vertical_target(t, -1);
		keep_goal = true;
		break;
	case KEY_DOWN:
		target = vertical_target(t, 1);
		keep_goal = true;
		break;
	case KEY_PPAGE:
		target = vertical_target(t, -page);
		keep_goal = true;
		break;
	case KEY_NPAGE:
		target = vertical_target(t, page);
		keep_goal = true;
		break;
	case KEY_HOME:
		target = t->starts[line];
		break;
	case KEY_END:
		ctui_textarea_line(t, line, &len);
		target = t->starts[line] + len;
		break;
	default:
		return false;
	}
	move_to(t, target, extend, keep_goal);
	bool has = ctui_textarea_range(t, &a, &b);
	return t->cursor != cursor || has != had ||
	       (has && t->anchor != anchor);
}

bool ctui_textarea_key(ctui_textarea *t, int key, size_t rows)
{
	if (key >= 0x80 && key <= 0xFF)
		return feed_utf8(t, (unsigned char)key);
	t->pending_len = 0;
	if (key >= 0x20 && key < 0x7F) {
		char c = (char)key;
		return type_text(t, &c, 1);
	}
	switch (key) {
	case '\n':
	case '\r':
	case KEY_ENTER:
		return type_text(t, "\n", 1);
	case '\t':
		return type_text(t, "\t", 1);
	case KEY_BACKSPACE:
	case 127:
	case 8:
		return remove_char(t, true);
	case KEY_DC:
		return remove_char(t, false);
	case 3: /* Ctrl-C */
		return ctui_textarea_copy(t);
	case 24: /* Ctrl-X */
		return ctui_textarea_cut(t);
	case 22: /* Ctrl-V */
		return ctui_textarea_paste(t);
	case 26: /* Ctrl-Z */
	case KEY_UNDO:
		return ctui_textarea_undo(t);
	case 25: /* Ctrl-Y */
	case KEY_REDO:
		return ctui_textarea_redo(t);
	case 1: /* Ctrl-A */
		t->merge = false;
		t->selecting = true;
		t->anchor = 0;
		t->cursor = t->len;
		set_goal(t);
		return true;
	case 27: {
		bool was = t->selecting;
		t->merge = false;
		t->selecting = false;
		return was;
	}
	default:
		return navigate(t, key, rows);
	}
}

/* ---- mouse ---- */

/* Offset nearest to (y, x), which may lie outside the area by one row. */
static size_t nearest(const ctui_textarea *t, int height, int y, int x)
{
	long row = (long)t->top + (y < -1 ? -1 : y > height ? height : y);
	if (row < 0)
		row = 0;
	if (row >= (long)t->line_count)
		row = (long)t->line_count - 1;
	int gutter = ctui_textarea_gutter(t);
	size_t col = x < gutter ? 0 : t->left + (size_t)(x - gutter);
	if (x < 0)
		col = t->left ? t->left - 1 : 0;
	size_t len;
	const char *s = ctui_textarea_line(t, (size_t)row, &len);
	return t->starts[row] + ctui_line_byte(s, len, col);
}

bool ctui_textarea_hit(const ctui_textarea *t, int width, int height, int y,
		       int x, size_t *offset)
{
	if (y < 0 || y >= height || x < 0 || x >= width)
		return false;
	*offset = nearest(t, height, y, x);
	return true;
}

bool ctui_textarea_press(ctui_textarea *t, int width, int height, int y, int x)
{
	size_t offset;
	if (!ctui_textarea_hit(t, width, height, y, x, &offset))
		return false;
	t->cursor = t->anchor = offset;
	t->selecting = false;
	t->dragging = true;
	t->merge = false;
	set_goal(t);
	return true;
}

bool ctui_textarea_drag(ctui_textarea *t, int width, int height, int y, int x)
{
	(void)width;
	if (!t->dragging)
		return false;
	size_t offset = nearest(t, height, y, x);
	if (offset == t->cursor && t->selecting)
		return false;
	t->selecting = true;
	t->cursor = offset;
	set_goal(t);
	return true;
}

bool ctui_textarea_release(ctui_textarea *t)
{
	if (!t->dragging)
		return false;
	t->dragging = false;
	if (t->anchor == t->cursor)
		t->selecting = false;
	return true;
}

/* ---- view ---- */

int ctui_textarea_gutter(const ctui_textarea *t)
{
	if (!t->line_numbers)
		return 0;
	int digits = 1;
	for (size_t n = t->line_count; n >= 10; n /= 10)
		digits++;
	return digits + 2;
}

void ctui_textarea_reveal(ctui_textarea *t, size_t rows, size_t cols)
{
	if (rows == 0)
		rows = 1;
	if (cols == 0)
		cols = 1;
	size_t max_top = t->line_count > rows ? t->line_count - rows : 0;
	if (t->top > max_top)
		t->top = max_top;
	size_t line, byte, len;
	ctui_textarea_locate(t, t->cursor, &line, &byte);
	if (line < t->top)
		t->top = line;
	if (line >= t->top + rows)
		t->top = line - rows + 1;
	const char *s = ctui_textarea_line(t, line, &len);
	size_t col = ctui_line_column(s, len, byte);
	if (col < t->left)
		t->left = col;
	if (col >= t->left + cols)
		t->left = col - cols + 1;
}

void ctui_textarea_scroll_to(ctui_textarea *t, size_t rows, size_t top)
{
	if (rows == 0)
		rows = 1;
	size_t max = t->line_count > rows ? t->line_count - rows : 0;
	t->top = top > max ? max : top;
	size_t line, byte, len;
	ctui_textarea_locate(t, t->cursor, &line, &byte);
	if (line < t->top)
		line = t->top;
	else if (line >= t->top + rows)
		line = t->top + rows - 1;
	else
		return;
	const char *s = ctui_textarea_line(t, line, &len);
	t->cursor = t->starts[line] + ctui_line_byte(s, len, t->goal);
}

static void draw_line(const ctui_textarea *t, size_t index, int y, int x,
		      int width, bool focused)
{
	size_t len;
	const char *line = ctui_textarea_line(t, index, &len);
	size_t base = t->starts[index], a = 0, b = 0;
	bool has = ctui_textarea_range(t, &a, &b);
	int body = ctui_attr(CTUI_PAIR_BODY, false),
	    sel = ctui_attr(CTUI_PAIR_SELECTION, false);
	size_t col = 0, i = 0;
	int used = 0;
	while (i < len && used < width) {
		unsigned long cp;
		size_t n = ctui_utf8_decode(line + i, &cp);
		if (i + n > len)
			n = len - i;
		size_t cells = ctui_cell_width(cp, col);
		size_t off = base + i;
		int attr = has && off >= a && off < b ? sel : body;
		if (focused && off == t->cursor)
			attr = (int)((unsigned)attr | (unsigned)A_REVERSE);
		if (col >= t->left) {
			if ((int)cells > width - used)
				break;
			if (cp == '\t') {
				for (size_t k = 0; k < cells; k++)
					ctui_put(y, x + used + (int)k, 1, " ",
						 attr);
			} else if (cp < 0x20 || cp == 0x7F || cp == 0xFFFD) {
				ctui_put(y, x + used, 1, "?", attr);
			} else {
				char glyph[8];
				memcpy(glyph, line + i, n);
				glyph[n] = '\0';
				ctui_put(y, x + used, (int)cells, glyph, attr);
			}
			used += (int)cells;
		} else if (col + cells >
			   t->left) { /* a wide character cut by the edge */
			int tail = (int)(col + cells - t->left);
			for (int k = 0; k < tail && used < width; k++, used++)
				ctui_put(y, x + used, 1, " ", attr);
		}
		col += cells;
		i += n;
	}
	/* The cursor (or a selected line break) past the end of the line. */
	if (i >= len && col >= t->left && used < width) {
		size_t off = base + len;
		bool cursor = focused && off == t->cursor;
		bool selected = has && off >= a && off < b;
		if (cursor || selected)
			ctui_put(y, x + used, 1, " ",
				 (int)((unsigned)(selected ? sel : body) |
				       (cursor ? (unsigned)A_REVERSE : 0U)));
	}
}

static void textarea_draw_impl(ctui_textarea *t, int y, int x, int width,
			       int height, bool focused)
{
	int gutter = ctui_textarea_gutter(t);
	if (width <= gutter || height <= 0)
		return;
	ctui_textarea_reveal(t, (size_t)height, (size_t)(width - gutter));
	size_t cursor_line, byte;
	ctui_textarea_locate(t, t->cursor, &cursor_line, &byte);
	for (int r = 0; r < height; r++) {
		size_t index = t->top + (size_t)r;
		ctui_fill(y + r, x, width, ctui_attr(CTUI_PAIR_BODY, false));
		if (index >= t->line_count)
			continue;
		if (gutter) {
			char number[48];
			snprintf(number, sizeof number, "%*zu ",
				 gutter - 1 > 20 ? 20 : gutter - 1, index + 1);
			ctui_put(
			    y + r, x, gutter, number,
			    ctui_attr(CTUI_PAIR_MUTED, index == cursor_line));
		}
		draw_line(t, index, y + r, x + gutter, width - gutter, focused);
	}
}

void ctui_textarea_draw(ctui_textarea *t, int y, int x, int width, int height,
			bool focused)
{
	ctui_style_begin(t->style);
	textarea_draw_impl(t, y, x, width, height, focused);
	ctui_style_end();
}
