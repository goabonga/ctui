/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * textview.c - a read-only text view with a line-number gutter (textview.h).
 */
#include <ctui/textview.h>

#include <curses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ctui/text.h>
#include <ctui/theme.h>

bool ctui_textview_init(ctui_textview *v, const char *text)
{
	*v = (ctui_textview){.text = text ? text : ""};
	size_t lines = 1;
	for (const char *p = v->text; *p; p++)
		if (*p == '\n' && p[1])
			lines++;
	v->starts = malloc(lines * sizeof *v->starts);
	if (!v->starts)
		return false;
	v->line_count = lines;
	size_t n = 0;
	v->starts[n++] = 0;
	for (const char *p = v->text; *p && n < lines; p++)
		if (*p == '\n' && p[1])
			v->starts[n++] = (size_t)(p - v->text) + 1;
	for (size_t i = 0; i < lines; i++) {
		size_t len;
		const char *line = ctui_textview_line(v, i, &len);
		size_t cols = ctui_line_column(line, len, len);
		if (cols > v->width)
			v->width = cols;
	}
	return true;
}

void ctui_textview_free(ctui_textview *v)
{
	free(v->starts);
	*v = (ctui_textview){0};
}

const char *ctui_textview_line(const ctui_textview *v, size_t i, size_t *len)
{
	const char *line = v->text + v->starts[i];
	const char *nl = strchr(line, '\n');
	*len = nl ? (size_t)(nl - line) : strlen(line);
	return line;
}

/* Columns taken by one character starting at column col (see textview.h). */
size_t ctui_cell_width(unsigned long cp, size_t col)
{
	if (cp == '\t')
		return CTUI_TAB_WIDTH - col % CTUI_TAB_WIDTH;
	if (cp < 0x20 || cp == 0x7F || cp == 0xFFFD)
		return 1; /* drawn as '?' */
	int w = ctui_codepoint_width(cp);
	return w < 0 ? 1 : (size_t)w;
}

size_t ctui_line_column(const char *line, size_t len, size_t byte)
{
	size_t col = 0, i = 0;
	while (i < len && i < byte) {
		unsigned long cp;
		size_t n = ctui_utf8_decode(line + i, &cp);
		if (i + n > len)
			n = len - i;
		col += ctui_cell_width(cp, col);
		i += n;
	}
	return col;
}

size_t ctui_line_byte(const char *line, size_t len, size_t column)
{
	size_t col = 0, i = 0;
	while (i < len) {
		unsigned long cp;
		size_t n = ctui_utf8_decode(line + i, &cp);
		if (i + n > len)
			n = len - i;
		size_t w = ctui_cell_width(cp, col);
		if (column < col + w)
			return i;
		col += w;
		i += n;
	}
	return len;
}

int ctui_textview_gutter(const ctui_textview *v)
{
	int digits = 1;
	for (size_t n = v->line_count; n >= 10; n /= 10)
		digits++;
	return digits + 2;
}

static int order(size_t l1, size_t b1, size_t l2, size_t b2)
{
	return l1 != l2 ? (l1 < l2 ? -1 : 1) : b1 == b2 ? 0 : b1 < b2 ? -1 : 1;
}

bool ctui_textview_selected(const ctui_textview *v, size_t line, size_t byte)
{
	if (!v->selecting ||
	    order(v->anchor_line, v->anchor_byte, v->line, v->byte) == 0)
		return false;
	size_t l1 = v->anchor_line, b1 = v->anchor_byte, l2 = v->line,
	       b2 = v->byte;
	if (order(l1, b1, l2, b2) > 0) {
		size_t tl = l1, tb = b1;
		l1 = l2;
		b1 = b2;
		l2 = tl;
		b2 = tb;
	}
	return order(line, byte, l1, b1) >= 0 && order(line, byte, l2, b2) < 0;
}

static void clamp_cursor(ctui_textview *v)
{
	if (v->line >= v->line_count)
		v->line = v->line_count - 1;
	size_t len;
	ctui_textview_line(v, v->line, &len);
	if (v->byte > len)
		v->byte = len;
}

static void horizontal(ctui_textview *v, int direction)
{
	size_t len;
	const char *line = ctui_textview_line(v, v->line, &len);
	if (direction > 0) {
		if (v->byte < len) {
			unsigned long cp;
			size_t n = ctui_utf8_decode(line + v->byte, &cp);
			v->byte += v->byte + n > len ? len - v->byte : n;
		} else if (v->line + 1 < v->line_count) {
			v->line++;
			v->byte = 0;
		}
	} else if (v->byte > 0) {
		do
			v->byte--;
		while (v->byte && ((unsigned)(unsigned char)line[v->byte] &
				   0xC0U) == 0x80U);
	} else if (v->line > 0) {
		v->line--;
		ctui_textview_line(v, v->line, &len);
		v->byte = len;
	}
	line = ctui_textview_line(v, v->line, &len);
	v->goal = ctui_line_column(line, len, v->byte);
}

static void vertical(ctui_textview *v, long delta)
{
	long target = (long)v->line + delta;
	if (target < 0)
		target = 0;
	if (target >= (long)v->line_count)
		target = (long)v->line_count - 1;
	v->line = (size_t)target;
	size_t len;
	const char *line = ctui_textview_line(v, v->line, &len);
	v->byte = ctui_line_byte(line, len, v->goal);
}

bool ctui_textview_key(ctui_textview *v, int key, size_t rows)
{
	size_t line = v->line, byte = v->byte;
	bool was_selecting = v->selecting;
	bool extend = key == KEY_SLEFT || key == KEY_SRIGHT || key == KEY_SR ||
		      key == KEY_SF;
	switch (key) {
	case 1: /* Ctrl-A */
	{
		size_t len;
		v->selecting = true;
		v->detached = false;
		v->anchor_line = 0;
		v->anchor_byte = 0;
		v->line = v->line_count - 1;
		ctui_textview_line(v, v->line, &len);
		v->byte = len;
		return true;
	}
	case 27:
		v->selecting = false;
		return was_selecting;
	case KEY_LEFT:
	case KEY_SLEFT:
	case KEY_RIGHT:
	case KEY_SRIGHT:
	case KEY_UP:
	case KEY_SR:
	case KEY_DOWN:
	case KEY_SF:
	case KEY_HOME:
	case KEY_END:
	case KEY_NPAGE:
	case KEY_PPAGE:
		break;
	default:
		return false;
	}
	v->detached = false;
	if (extend && !v->selecting) {
		v->selecting = true;
		v->anchor_line = line;
		v->anchor_byte = byte;
	} else if (!extend) {
		v->selecting = false;
	}
	size_t len;
	const char *text;
	switch (key) {
	case KEY_LEFT:
	case KEY_SLEFT:
		horizontal(v, -1);
		break;
	case KEY_RIGHT:
	case KEY_SRIGHT:
		horizontal(v, 1);
		break;
	case KEY_UP:
	case KEY_SR:
		vertical(v, -1);
		break;
	case KEY_DOWN:
	case KEY_SF:
		vertical(v, 1);
		break;
	case KEY_PPAGE:
		vertical(v, -(long)(rows ? rows : 1));
		break;
	case KEY_NPAGE:
		vertical(v, (long)(rows ? rows : 1));
		break;
	case KEY_HOME:
		v->byte = 0;
		v->goal = 0;
		break;
	case KEY_END:
		text = ctui_textview_line(v, v->line, &len);
		v->byte = len;
		v->goal = ctui_line_column(text, len, len);
		break;
	default:
		break;
	}
	clamp_cursor(v);
	return v->line != line || v->byte != byte ||
	       v->selecting != was_selecting;
}

void ctui_textview_scroll_to(ctui_textview *v, size_t rows, size_t top)
{
	if (rows == 0)
		rows = 1;
	size_t max = v->line_count > rows ? v->line_count - rows : 0;
	v->top = top > max ? max : top;
	if (v->line < v->top)
		v->line = v->top;
	else if (v->line >= v->top + rows)
		v->line = v->top + rows - 1;
	clamp_cursor(v);
}

static size_t max_left(const ctui_textview *v, size_t cols)
{
	size_t total = v->width + 1; /* the cursor may sit past the end */
	return total > cols ? total - cols : 0;
}

void ctui_textview_scroll_left_to(ctui_textview *v, size_t cols, size_t left)
{
	if (cols == 0)
		cols = 1;
	size_t max = max_left(v, cols);
	v->left = left > max ? max : left;
	size_t len;
	const char *line = ctui_textview_line(v, v->line, &len);
	size_t col = ctui_line_column(line, len, v->byte);
	v->detached = false;
	if (col >= v->left && col < v->left + cols)
		return;
	size_t target = col < v->left ? v->left : v->left + cols - 1;
	v->byte = ctui_line_byte(line, len, target);
	if (col < v->left && ctui_line_column(line, len, v->byte) < v->left &&
	    v->byte < len) {
		unsigned long cp;
		size_t n = ctui_utf8_decode(line + v->byte, &cp);
		v->byte += v->byte + n > len ? len - v->byte : n;
	}
	v->goal = ctui_line_column(line, len, v->byte);
	/* a line too short to reach the window keeps the cursor outside it */
	col = ctui_line_column(line, len, v->byte);
	v->detached = col < v->left || col >= v->left + cols;
}

bool ctui_textview_hscroll(ctui_textview *v, size_t cols, int delta)
{
	size_t before = v->left, max = max_left(v, cols ? cols : 1);
	size_t left = v->left > max ? max : v->left;
	if (delta < 0)
		left = left > (size_t)-(long)delta ? left - (size_t)-(long)delta
						   : 0;
	else
		left = left + (size_t)delta > max ? max : left + (size_t)delta;
	ctui_textview_scroll_left_to(v, cols, left);
	return v->left != before;
}

static void apply_hbar(ctui_textview *v)
{
	ctui_textview_scroll_left_to(v, v->hbar.visible, v->hbar.offset);
}

bool ctui_textview_hpress(ctui_textview *v, int y, int x)
{
	if (!v->hscroll || !ctui_scrollbar_press(&v->hbar, y, x))
		return false;
	apply_hbar(v);
	return true;
}

bool ctui_textview_hdrag(ctui_textview *v, int x)
{
	if (!v->hscroll)
		return false;
	if (ctui_scrollbar_drag(&v->hbar, x))
		apply_hbar(v);
	return v->hbar.dragging;
}

bool ctui_textview_hrelease(ctui_textview *v)
{
	return ctui_scrollbar_release(&v->hbar);
}

bool ctui_textview_hwheel(ctui_textview *v, int columns)
{
	if (!v->hscroll || !ctui_scrollbar_needed(&v->hbar))
		return false;
	if (ctui_scrollbar_wheel(&v->hbar, columns))
		apply_hbar(v);
	return true;
}

void ctui_textview_reveal(ctui_textview *v, size_t rows, size_t cols)
{
	if (rows == 0)
		rows = 1;
	if (cols == 0)
		cols = 1;
	if (v->line < v->top)
		v->top = v->line;
	if (v->line >= v->top + rows)
		v->top = v->line - rows + 1;
	size_t len;
	const char *line = ctui_textview_line(v, v->line, &len);
	size_t col = ctui_line_column(line, len, v->byte);
	if (v->detached)
		return;
	if (col < v->left)
		v->left = col;
	if (col >= v->left + cols)
		v->left = col - cols + 1;
}

static void draw_line(const ctui_textview *v, size_t index, int y, int x,
		      int width, bool focused)
{
	size_t len;
	const char *line = ctui_textview_line(v, index, &len);
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
		bool cursor = focused && index == v->line && i == v->byte;
		int attr = ctui_textview_selected(v, index, i) ? sel : body;
		if (cursor)
			attr = (int)((unsigned)attr | (unsigned)A_REVERSE);
		if (col >= v->left) {
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
			   v->left) { /* a wide character cut by the edge */
			int tail = (int)(col + cells - v->left);
			for (int k = 0; k < tail && used < width; k++, used++)
				ctui_put(y, x + used, 1, " ", attr);
		}
		col += cells;
		i += n;
	}
	/* The cursor (or a selected line break) past the end of the line. */
	if (i >= len && col >= v->left && used < width) {
		bool cursor = focused && index == v->line && v->byte >= len;
		bool selected = ctui_textview_selected(v, index, len);
		if (cursor || selected)
			ctui_put(y, x + used, 1, " ",
				 (int)((unsigned)(selected ? sel : body) |
				       (cursor ? (unsigned)A_REVERSE : 0U)));
	}
}

static void textview_draw_impl(ctui_textview *v, int y, int x, int width,
			       int height, bool focused)
{
	int gutter = ctui_textview_gutter(v);
	if (width <= gutter || height <= 0)
		return;
	int rows = v->hscroll && height > 1 ? height - 1 : height;
	ctui_textview_reveal(v, (size_t)rows, (size_t)(width - gutter));
	if (rows < height) {
		ctui_fill(y + rows, x, width, ctui_attr(CTUI_PAIR_BODY, false));
		ctui_scrollbar_set(&v->hbar, v->width + 1,
				   (size_t)(width - gutter), v->left);
		ctui_scrollbar_place_horizontal(&v->hbar, y + rows, x + gutter,
						width - gutter);
		ctui_scrollbar_draw(&v->hbar, false);
	}
	for (int r = 0; r < rows; r++) {
		size_t index = v->top + (size_t)r;
		ctui_fill(y + r, x, width, ctui_attr(CTUI_PAIR_BODY, false));
		if (index >= v->line_count)
			continue;
		char number[48];
		snprintf(number, sizeof number, "%*zu ",
			 gutter - 1 > 20 ? 20 : gutter - 1, index + 1);
		ctui_put(y + r, x, gutter, number,
			 ctui_attr(CTUI_PAIR_MUTED, index == v->line));
		draw_line(v, index, y + r, x + gutter, width - gutter, focused);
	}
}

void ctui_textview_draw(ctui_textview *v, int y, int x, int width, int height,
			bool focused)
{
	ctui_style_begin(v->style);
	textview_draw_impl(v, y, x, width, height, focused);
	ctui_style_end();
}
