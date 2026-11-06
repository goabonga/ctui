/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * dialog.h - confirm, prompt and text viewer dialogs.
 *
 * Dialogs are plain state structs: the caller feeds keys to *_key() and
 * redraws with *_draw() each turn, so they fit any event loop and never
 * block. *_key() returns whether the dialog is still open, was accepted or
 * was cancelled.
 *
 *   confirm: a question, the name of the target and Yes / No buttons;
 *            No is the default, y / n answer directly.
 *   prompt:  a wrapped label over an input line; edits UTF-8 text in a
 *            caller-owned buffer. The caller shows the cursor
 *            (curs_set(1)); ctui_prompt_draw() places it.
 *   viewer:  a scrollable framed text.
 *
 * Every dialog draws a close button (x) in the top right corner of its
 * frame; the *_hit() functions tell when a click lands on it.
 *
 * ctui_dialog_box() places a dialog of any size for the caller to fill.
 */
#ifndef CTUI_DIALOG_H
#define CTUI_DIALOG_H

#include <stdbool.h>

#include <ctui/theme.h>
#include <stddef.h>

#include <ctui/labels.h>
typedef enum ctui_dialog_result {
	CTUI_DIALOG_OPEN = 0,
	CTUI_DIALOG_ACCEPTED,
	CTUI_DIALOG_CANCELLED
} ctui_dialog_result;

typedef struct ctui_rect {
	int y, x, height, width;
} ctui_rect;

/* Centre a width x height box on a lines x cols screen. It is shrunk to
 * keep a one cell margin; false when less than 10 x 3 cells remain. For
 * dialogs that hold other components (forms, lists, tab panels). */
bool ctui_dialog_box(int lines, int cols, int width, int height,
		     ctui_rect *box);

/* ---- confirm ---------------------------------------------------- */

typedef struct ctui_confirm {
	const char *action;
	const char *name; /* may be NULL */
	bool yes_selected;
	const ctui_labels
	    *labels; /* override of the global labels, may be NULL */
	const ctui_style *style; /* colours and glyphs of this instance, may be
				    NULL; set after init */
} ctui_confirm;

void ctui_confirm_init(ctui_confirm *c, const char *action, const char *name);
ctui_dialog_result ctui_confirm_key(ctui_confirm *c, int key);

/* Where the dialog goes on a lines x cols screen; false when too small.
 * The box is as high as its text needs (at most three rows of text). */
bool ctui_confirm_geometry(const ctui_confirm *c, int lines, int cols,
			   ctui_rect *box);

/* 1 on the Yes button, 0 on No, 2 on the close button, -1 elsewhere. */
int ctui_confirm_hit(const ctui_confirm *c, int lines, int cols, int y, int x);

void ctui_confirm_draw(const ctui_confirm *c);

/* ---- prompt ----------------------------------------------------- */

typedef struct ctui_prompt {
	const char *label;
	char *value;	 /* caller buffer, NUL-terminated */
	size_t capacity; /* bytes, including the NUL */
	size_t used;	 /* strlen(value) */
	const ctui_labels
	    *labels; /* override of the global labels, may be NULL */
	const ctui_style *style; /* colours and glyphs of this instance, may be
				    NULL; set after init */
} ctui_prompt;

void ctui_prompt_init(ctui_prompt *p, const char *label, char *value,
		      size_t capacity);
ctui_dialog_result ctui_prompt_key(ctui_prompt *p, int key);
bool ctui_prompt_geometry(const ctui_prompt *p, int lines, int cols,
			  ctui_rect *box);

/* Whether screen cell (y, x) is on the close button of the prompt. */
bool ctui_prompt_close_hit(const ctui_prompt *p, int lines, int cols, int y,
			   int x);

/* Draw the prompt and leave the cursor on the input line. */
void ctui_prompt_draw(const ctui_prompt *p);

/* ---- viewer ----------------------------------------------------- */

typedef struct ctui_viewer {
	const char *title;
	const char *text;
	size_t top;		 /* first visible line */
	const ctui_style *style; /* colours and glyphs of this instance, may be
				    NULL; set after init */
} ctui_viewer;

void ctui_viewer_init(ctui_viewer *v, const char *title, const char *text);

/* Number of lines of text (a trailing newline does not add one). */
size_t ctui_viewer_lines(const ctui_viewer *v);

/* rows: visible text rows, used by paging and clamping. */
ctui_dialog_result ctui_viewer_key(ctui_viewer *v, int key, size_t rows);

/* Draw framed in the given area. */
void ctui_viewer_draw(const ctui_viewer *v, int y, int x, int height,
		      int width);

/* Whether (my, mx) is on the close button of a viewer drawn in that area. */
bool ctui_viewer_close_hit(const ctui_viewer *v, int y, int x, int height,
			   int width, int my, int mx);

#endif /* CTUI_DIALOG_H */
