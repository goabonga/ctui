/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * wizard.h - the frame of a multi-step flow: a step indicator on top and
 * Cancel / Back / Next buttons below, meant for dialogs.
 *
 * The wizard only keeps the current step. What a step shows (a form, a
 * summary) and what the buttons do beyond moving between steps is up to
 * the caller, who also decides which part has the keyboard. Going back
 * keeps whatever the earlier steps hold, since their state is the
 * caller's.
 *
 * On the last step the Next button reads Finish. Ctrl-B and Ctrl-N are
 * shortcuts for Back and Next that work from any field.
 */
#ifndef CTUI_WIZARD_H
#define CTUI_WIZARD_H

#include <stdbool.h>

#include <ctui/theme.h>
#include <stddef.h>

#include <ctui/labels.h>
#define CTUI_WIZARD_KEY_BACK 2	/* Ctrl-B */
#define CTUI_WIZARD_KEY_NEXT 14 /* Ctrl-N */

typedef struct ctui_wizard {
	const char *const *titles;
	size_t count;
	size_t step;
	const ctui_labels *labels; /* override of the global labels, may be
				    NULL; set it after ctui_wizard_init() */
	const ctui_style *style; /* colours and glyphs of this instance, may be
				    NULL; set after init */
} ctui_wizard;

typedef enum ctui_wizard_button {
	CTUI_WIZARD_CANCEL = 0,
	CTUI_WIZARD_BACK,
	CTUI_WIZARD_NEXT, /* "Finish" on the last step */
	CTUI_WIZARD_BUTTONS
} ctui_wizard_button;

typedef enum ctui_wizard_action {
	CTUI_WIZARD_NONE = 0,
	CTUI_WIZARD_GO_BACK,
	CTUI_WIZARD_GO_NEXT,
	CTUI_WIZARD_DONE,
	CTUI_WIZARD_ABORT
} ctui_wizard_action;

void ctui_wizard_init(ctui_wizard *w, const char *const *titles, size_t count);

bool ctui_wizard_first(const ctui_wizard *w);
bool ctui_wizard_last(const ctui_wizard *w);

/* What a button does on the current step: Back does nothing on the first
 * one, Next finishes on the last one. */
ctui_wizard_action ctui_wizard_press(const ctui_wizard *w,
				     ctui_wizard_button button);

/* The action of a shortcut key (Ctrl-B, Ctrl-N), or CTUI_WIZARD_NONE. */
ctui_wizard_action ctui_wizard_key(const ctui_wizard *w, int key);

/* Carry out GO_BACK / GO_NEXT; true when the step changed. */
bool ctui_wizard_apply(ctui_wizard *w, ctui_wizard_action action);

/* The buttons are right-aligned in a row width columns wide. Columns of
 * button b relative to the row: false when they do not fit. */
bool ctui_wizard_button_span(const ctui_wizard *w, ctui_wizard_button button,
			     int width, int *offset, int *columns);

/* Button under column x of the row, or -1. */
int ctui_wizard_hit(const ctui_wizard *w, int width, int x);

/* One row: "done title - 2 current - 3 next", clipped to width. */
void ctui_wizard_steps_draw(const ctui_wizard *w, int y, int x, int width);

/* One row of buttons; focused is the one drawn highlighted, or -1. */
void ctui_wizard_buttons_draw(const ctui_wizard *w, int y, int x, int width,
			      int focused);

#endif /* CTUI_WIZARD_H */
