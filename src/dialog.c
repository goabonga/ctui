/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * dialog.c - confirm, prompt and text viewer dialogs (dialog.h).
 */
#include <ctui/dialog.h>

#include <curses.h>
#include <stdio.h>
#include <string.h>

#include <ctui/frame.h>
#include <ctui/text.h>
#include <ctui/theme.h>

#define BUTTON_WIDTH 9
#define MAX_TEXT_ROWS 3

bool ctui_dialog_box(int lines, int cols, int width, int height, ctui_rect *box)
{
	if (width > cols - 2)
		width = cols - 2;
	if (height > lines - 2)
		height = lines - 2;
	if (width < 10 || height < 3)
		return false;
	*box = (ctui_rect){(lines - height) / 2, (cols - width) / 2, height,
			   width};
	return true;
}

/* ---- confirm ---------------------------------------------------- */

void ctui_confirm_init(ctui_confirm *c, const char *action, const char *name)
{
	*c = (ctui_confirm){
	    .action = action, .name = name, .yes_selected = false};
}

ctui_dialog_result ctui_confirm_key(ctui_confirm *c, int key)
{
	switch (key) {
	case 27:
	case 'q':
	case 'n':
	case 'N':
		return CTUI_DIALOG_CANCELLED;
	case 'y':
	case 'Y':
		return CTUI_DIALOG_ACCEPTED;
	case KEY_LEFT:
	case 'h':
		c->yes_selected = true;
		break;
	case KEY_RIGHT:
	case 'l':
		c->yes_selected = false;
		break;
	case '\n':
	case KEY_ENTER:
	case ' ':
		return c->yes_selected ? CTUI_DIALOG_ACCEPTED
				       : CTUI_DIALOG_CANCELLED;
	default:
		break;
	}
	return CTUI_DIALOG_OPEN;
}

/* Rows text takes wrapped to width columns, at least 1 and at most max. */
static int wrapped_lines(const char *text, int width, int max)
{
	int n = 0;
	while (text && *text && n < max) {
		size_t skip =
		    ctui_wrap_skip(text, ctui_wrap_chunk(text, width));
		if (skip == 0)
			break;
		text += skip;
		n++;
	}
	return n ? n : 1;
}

bool ctui_confirm_geometry(const ctui_confirm *c, int lines, int cols,
			   ctui_rect *box)
{
	if (lines < 10 || cols < 40)
		return false;
	int width = cols - 8 > 76 ? 76 : cols - 8;
	/* the action, the name, a blank row, the buttons */
	int height = wrapped_lines(c->action, width - 4, MAX_TEXT_ROWS) +
		     (c->name ? 1 : 0) + 4;
	*box = (ctui_rect){(lines - height) / 2, (cols - width) / 2, height,
			   width};
	return true;
}

/* Both buttons are as wide as the longer label needs, at least BUTTON_WIDTH. */
static int button_width(const ctui_labels *l)
{
	int w = (int)ctui_utf8_columns(l->yes),
	    n = (int)ctui_utf8_columns(l->no);
	w = (w > n ? w : n) + 4;
	return w > BUTTON_WIDTH ? w : BUTTON_WIDTH;
}

static void confirm_buttons(const ctui_rect *box, int bw, int *y, int *yes_x,
			    int *no_x)
{
	*y = box->y + box->height - 2;
	*yes_x = box->x + box->width / 2 - 2 - bw;
	*no_x = box->x + box->width / 2 + 2;
}

/* A button caption centred in bw columns. */
static void button_draw(int y, int x, int bw, const char *label, int attr)
{
	ctui_fill(y, x, bw, attr);
	int pad = (bw - (int)ctui_utf8_columns(label)) / 2;
	ctui_put(y, x + (pad > 0 ? pad : 0), bw - (pad > 0 ? pad : 0), label,
		 attr);
}

int ctui_confirm_hit(const ctui_confirm *c, int lines, int cols, int y, int x)
{
	ctui_rect box;
	int by, yes, no;
	if (!ctui_confirm_geometry(c, lines, cols, &box))
		return -1;
	ctui_labels l = ctui_labels_resolve(c->labels);
	int bw = button_width(&l);
	confirm_buttons(&box, bw, &by, &yes, &no);
	if (ctui_frame_close_hit(box.y, box.x, box.width, y, x))
		return 2;
	if (y != by)
		return -1;
	if (x >= yes && x < yes + bw)
		return 1;
	if (x >= no && x < no + bw)
		return 0;
	return -1;
}

static void confirm_draw_impl(const ctui_confirm *c)
{
	ctui_rect box;
	if (!ctui_confirm_geometry(c, LINES, COLS, &box))
		return;
	ctui_labels l = ctui_labels_resolve(c->labels);
	ctui_frame_draw(box.y, box.x, box.height, box.width, true, true,
			l.confirm_title);
	ctui_frame_close_draw(box.y, box.x, box.width, true);
	int dialog = ctui_attr(CTUI_PAIR_DIALOG, false);
	int rows = ctui_wrapped(box.y + 1, box.x + 2, box.width - 4,
				MAX_TEXT_ROWS, c->action, dialog);
	if (c->name)
		ctui_wrapped(box.y + 1 + rows, box.x + 2, box.width - 4, 1,
			     c->name, ctui_attr(CTUI_PAIR_DIALOG, true));
	int by, yes, no;
	int bw = button_width(&l);
	confirm_buttons(&box, bw, &by, &yes, &no);
	int on = ctui_attr(CTUI_PAIR_SELECTION, true);
	button_draw(by, yes, bw, l.yes, c->yes_selected ? on : dialog);
	button_draw(by, no, bw, l.no, c->yes_selected ? dialog : on);
}

void ctui_confirm_draw(const ctui_confirm *c)
{
	ctui_style_begin(c->style);
	confirm_draw_impl(c);
	ctui_style_end();
}

/* ---- prompt ----------------------------------------------------- */

void ctui_prompt_init(ctui_prompt *p, const char *label, char *value,
		      size_t capacity)
{
	*p =
	    (ctui_prompt){.label = label, .value = value, .capacity = capacity};
	if (capacity)
		value[capacity - 1] = '\0';
	p->used = capacity ? strlen(value) : 0;
}

ctui_dialog_result ctui_prompt_key(ctui_prompt *p, int key)
{
	if (key == 27 || key == 3 || key == 4)
		return CTUI_DIALOG_CANCELLED;
	if (key == '\n' || key == KEY_ENTER)
		return CTUI_DIALOG_ACCEPTED;
	if (key == 21) { /* Ctrl-U */
		p->used = 0;
		p->value[0] = '\0';
	} else if (key == KEY_BACKSPACE || key == 127 || key == 8) {
		if (p->used) {
			/* Remove a whole UTF-8 character: skip continuation
			 * bytes. */
			do
				p->used--;
			while (p->used &&
			       ((unsigned)(unsigned char)p->value[p->used] &
				0xC0U) == 0x80U);
			p->value[p->used] = '\0';
		}
	} else if (key >= 32 && key <= 255 && key != 127 &&
		   p->used + 1 < p->capacity) {
		p->value[p->used++] = (char)key;
		p->value[p->used] = '\0';
	}
	return CTUI_DIALOG_OPEN;
}

bool ctui_prompt_geometry(const ctui_prompt *p, int lines, int cols,
			  ctui_rect *box)
{
	if (lines < 10 || cols < 30)
		return false;
	int width = cols - 4 > 70 ? 70 : cols - 4;
	/* the label and the input line */
	int height = wrapped_lines(p->label, width - 4, MAX_TEXT_ROWS) + 3;
	*box = (ctui_rect){(lines - height) / 2, (cols - width) / 2, height,
			   width};
	return true;
}

bool ctui_prompt_close_hit(const ctui_prompt *p, int lines, int cols, int y,
			   int x)
{
	ctui_rect box;
	return ctui_prompt_geometry(p, lines, cols, &box) &&
	       ctui_frame_close_hit(box.y, box.x, box.width, y, x);
}

static void prompt_draw_impl(const ctui_prompt *p)
{
	ctui_rect box;
	if (!ctui_prompt_geometry(p, LINES, COLS, &box))
		return;
	ctui_frame_draw(box.y, box.x, box.height, box.width, true, true,
			ctui_labels_resolve(p->labels).prompt_title);
	ctui_frame_close_draw(box.y, box.x, box.width, true);
	ctui_wrapped(box.y + 1, box.x + 2, box.width - 4, box.height - 3,
		     p->label, ctui_attr(CTUI_PAIR_DIALOG, false));
	int row = box.y + box.height - 2, left = box.x + 2,
	    width = box.width - 4;
	int field = ctui_attr(CTUI_PAIR_SELECTION, false);
	ctui_fill(row, left, width, field);
	/* Show the tail of the text when it is longer than the field. */
	size_t visible = (size_t)(width > 1 ? width - 1 : 1);
	const char *shown = p->value;
	size_t cols = ctui_utf8_columns(shown);
	while (cols > visible && *shown) {
		shown++;
		while (((unsigned)(unsigned char)*shown & 0xC0U) == 0x80U)
			shown++;
		cols = ctui_utf8_columns(shown);
	}
	ctui_put(row, left, width, shown, field);
	move(row, left + (int)cols);
}

void ctui_prompt_draw(const ctui_prompt *p)
{
	ctui_style_begin(p->style);
	prompt_draw_impl(p);
	ctui_style_end();
}

/* ---- viewer ----------------------------------------------------- */

void ctui_viewer_init(ctui_viewer *v, const char *title, const char *text)
{
	*v = (ctui_viewer){.title = title, .text = text ? text : ""};
}

size_t ctui_viewer_lines(const ctui_viewer *v)
{
	size_t n = 0;
	for (const char *p = v->text; *p; p++)
		if (*p == '\n' && p[1])
			n++;
	return *v->text ? n + 1 : 0;
}

ctui_dialog_result ctui_viewer_key(ctui_viewer *v, int key, size_t rows)
{
	size_t lines = ctui_viewer_lines(v);
	size_t max_top = lines > rows ? lines - rows : 0;
	size_t page = rows > 1 ? rows - 1 : 1;
	switch (key) {
	case 'q':
	case 27:
	case '\n':
		return CTUI_DIALOG_CANCELLED;
	case KEY_HOME:
		v->top = 0;
		break;
	case KEY_END:
		v->top = max_top;
		break;
	case KEY_DOWN:
	case 'j':
		v->top++;
		break;
	case KEY_UP:
	case 'k':
		v->top = v->top ? v->top - 1 : 0;
		break;
	case KEY_NPAGE:
		v->top += page;
		break;
	case KEY_PPAGE:
		v->top = v->top > page ? v->top - page : 0;
		break;
	default:
		break;
	}
	if (v->top > max_top)
		v->top = max_top;
	return CTUI_DIALOG_OPEN;
}

bool ctui_viewer_close_hit(const ctui_viewer *v, int y, int x, int height,
			   int width, int my, int mx)
{
	(void)v;
	return height >= 3 && ctui_frame_close_hit(y, x, width, my, mx);
}

static void viewer_draw_impl(const ctui_viewer *v, int y, int x, int height,
			     int width)
{
	for (int r = 0; r < height; r++)
		ctui_fill(y + r, x, width, ctui_attr(CTUI_PAIR_BODY, false));
	ctui_frame_draw(y, x, height, width, false, false, v->title);
	ctui_frame_close_draw(y, x, width, false);
	int rows = height - 2;
	const char *p = v->text;
	for (size_t i = 0; i < v->top && *p; i++) {
		const char *n = strchr(p, '\n');
		p = n ? n + 1 : p + strlen(p);
	}
	for (int r = 0; r < rows && *p; r++) {
		const char *n = strchr(p, '\n');
		size_t len = n ? (size_t)(n - p) : strlen(p);
		char line[CTUI_TEXT_MAX];
		size_t take = len < sizeof line - 1 ? len : sizeof line - 1;
		memcpy(line, p, take);
		line[take] = '\0';
		ctui_put(y + 1 + r, x + 2, width - 4, line,
			 ctui_attr(CTUI_PAIR_BODY, false));
		p = n ? n + 1 : p + len;
	}
	size_t lines = ctui_viewer_lines(v);
	if (lines > (size_t)rows && width > 20) {
		char pos[48];
		size_t last = v->top + (size_t)rows < lines
				  ? v->top + (size_t)rows
				  : lines;
		snprintf(pos, sizeof pos, " %zu-%zu/%zu ", v->top + 1, last,
			 lines);
		ctui_put(y + height - 1, x + width - 2 - (int)strlen(pos),
			 (int)strlen(pos), pos,
			 ctui_attr(CTUI_PAIR_MUTED, false));
	}
}

void ctui_viewer_draw(const ctui_viewer *v, int y, int x, int height, int width)
{
	ctui_style_begin(v->style);
	viewer_draw_impl(v, y, x, height, width);
	ctui_style_end();
}
