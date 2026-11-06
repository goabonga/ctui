/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * wizard.c - step indicator and buttons of a multi-step flow (wizard.h).
 */
#include <ctui/wizard.h>

#include <curses.h>
#include <stdio.h>
#include <string.h>

#include <ctui/text.h>
#include <ctui/theme.h>

#define BUTTON_GAP 2

void ctui_wizard_init(ctui_wizard *w, const char *const *titles, size_t count)
{
	*w = (ctui_wizard){.titles = titles, .count = count};
}

bool ctui_wizard_first(const ctui_wizard *w)
{
	return w->step == 0;
}

bool ctui_wizard_last(const ctui_wizard *w)
{
	return w->step + 1 >= w->count;
}

ctui_wizard_action ctui_wizard_press(const ctui_wizard *w,
				     ctui_wizard_button button)
{
	switch (button) {
	case CTUI_WIZARD_CANCEL:
		return CTUI_WIZARD_ABORT;
	case CTUI_WIZARD_BACK:
		return ctui_wizard_first(w) ? CTUI_WIZARD_NONE
					    : CTUI_WIZARD_GO_BACK;
	case CTUI_WIZARD_NEXT:
		return ctui_wizard_last(w) ? CTUI_WIZARD_DONE
					   : CTUI_WIZARD_GO_NEXT;
	default:
		return CTUI_WIZARD_NONE;
	}
}

ctui_wizard_action ctui_wizard_key(const ctui_wizard *w, int key)
{
	if (key == CTUI_WIZARD_KEY_BACK)
		return ctui_wizard_press(w, CTUI_WIZARD_BACK);
	if (key == CTUI_WIZARD_KEY_NEXT)
		return ctui_wizard_press(w, CTUI_WIZARD_NEXT);
	return CTUI_WIZARD_NONE;
}

bool ctui_wizard_apply(ctui_wizard *w, ctui_wizard_action action)
{
	if (action == CTUI_WIZARD_GO_BACK && !ctui_wizard_first(w)) {
		w->step--;
		return true;
	}
	if (action == CTUI_WIZARD_GO_NEXT && !ctui_wizard_last(w)) {
		w->step++;
		return true;
	}
	return false;
}

static const char *button_label(const ctui_wizard *w, ctui_wizard_button button)
{
	ctui_labels l = ctui_labels_resolve(w->labels);
	switch (button) {
	case CTUI_WIZARD_CANCEL:
		return l.cancel;
	case CTUI_WIZARD_BACK:
		return l.back;
	default:
		return ctui_wizard_last(w) ? l.finish : l.next;
	}
}

static int button_width(const ctui_wizard *w, ctui_wizard_button button)
{
	return (int)ctui_utf8_columns(button_label(w, button)) + 4;
}

bool ctui_wizard_button_span(const ctui_wizard *w, ctui_wizard_button button,
			     int width, int *offset, int *columns)
{
	if (button < 0 || button >= CTUI_WIZARD_BUTTONS)
		return false;
	int total = BUTTON_GAP * (CTUI_WIZARD_BUTTONS - 1);
	for (int b = 0; b < CTUI_WIZARD_BUTTONS; b++)
		total += button_width(w, (ctui_wizard_button)b);
	if (total > width)
		return false;
	int left = width - total;
	for (int b = 0; b < (int)button; b++)
		left += button_width(w, (ctui_wizard_button)b) + BUTTON_GAP;
	*offset = left;
	*columns = button_width(w, button);
	return true;
}

int ctui_wizard_hit(const ctui_wizard *w, int width, int x)
{
	for (int b = 0; b < CTUI_WIZARD_BUTTONS; b++) {
		int offset, columns;
		if (ctui_wizard_button_span(w, (ctui_wizard_button)b, width,
					    &offset, &columns) &&
		    x >= offset && x < offset + columns)
			return b;
	}
	return -1;
}

static void wizard_steps_draw_impl(const ctui_wizard *w, int y, int x,
				   int width)
{
	int dialog = ctui_attr(CTUI_PAIR_DIALOG, false);
	ctui_fill(y, x, width, dialog);
	int col = 0;
	for (size_t i = 0; i < w->count && col < width; i++) {
		char label[CTUI_TEXT_MAX];
		bool done = i < w->step, current = i == w->step;
		if (done)
			snprintf(label, sizeof label, " %s %s ",
				 ctui_theme_glyphs()->check, w->titles[i]);
		else
			snprintf(label, sizeof label, " %zu %s ", i + 1,
				 w->titles[i]);
		int attr = current ? ctui_attr(CTUI_PAIR_SELECTION, true)
			   : done  ? ctui_attr(CTUI_PAIR_DIALOG, true)
				   : (int)((unsigned)dialog | (unsigned)A_DIM);
		col += ctui_put(y, x + col, width - col, label, attr);
		if (i + 1 < w->count && col < width) {
			char dash[16];
			snprintf(dash, sizeof dash, " %s ",
				 ctui_theme_glyphs()->rule);
			col +=
			    ctui_put(y, x + col, width - col, dash,
				     ctui_attr(CTUI_PAIR_DIALOG_BORDER, false));
		}
	}
}

void ctui_wizard_steps_draw(const ctui_wizard *w, int y, int x, int width)
{
	ctui_style_begin(w->style);
	wizard_steps_draw_impl(w, y, x, width);
	ctui_style_end();
}

static void wizard_buttons_draw_impl(const ctui_wizard *w, int y, int x,
				     int width, int focused)
{
	int dialog = ctui_attr(CTUI_PAIR_DIALOG, false);
	ctui_fill(y, x, width, dialog);
	for (int b = 0; b < CTUI_WIZARD_BUTTONS; b++) {
		int offset, columns;
		if (!ctui_wizard_button_span(w, (ctui_wizard_button)b, width,
					     &offset, &columns))
			return;
		bool disabled = ctui_wizard_press(w, (ctui_wizard_button)b) ==
				CTUI_WIZARD_NONE;
		int attr = b == focused && !disabled
			       ? ctui_attr(CTUI_PAIR_SELECTION, true)
			   : disabled
			       ? (int)((unsigned)dialog | (unsigned)A_DIM)
			       : ctui_attr(CTUI_PAIR_DIALOG, true);
		char text[32];
		snprintf(text, sizeof text, "  %s  ",
			 button_label(w, (ctui_wizard_button)b));
		ctui_fill(y, x + offset, columns, attr);
		ctui_put(y, x + offset, columns, text, attr);
	}
}

void ctui_wizard_buttons_draw(const ctui_wizard *w, int y, int x, int width,
			      int focused)
{
	ctui_style_begin(w->style);
	wizard_buttons_draw_impl(w, y, x, width, focused);
	ctui_style_end();
}
