/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * picker.h - file and folder chooser dialogs.
 *
 * A picker browses the file system one folder at a time, in a dialog
 * centred on the screen: the current path on top, the folders then the
 * files of that folder below (hidden ones on demand), and a hint line.
 * Symbolic links are followed. Like the other dialogs it is a plain state
 * struct: feed keys to ctui_picker_key(), redraw with ctui_picker_draw().
 *
 *   file mode    lists folders and files. Enter opens a folder or chooses
 *                the file.
 *   folder mode  lists folders only, with a first row "." that stands for
 *                the folder being browsed. Enter opens a folder or, on ".",
 *                chooses the current folder; Space chooses the highlighted
 *                folder without opening it.
 *
 * Up/Down/j/k, Home/End and PgUp/PgDn move; Enter or Right activate; Left
 * or Backspace go to the parent folder; '.' shows or hides hidden entries;
 * Esc or q cancel. The chosen absolute path is in picker->chosen once the
 * result is CTUI_DIALOG_ACCEPTED.
 */
#ifndef CTUI_PICKER_H
#define CTUI_PICKER_H

#include <stdbool.h>
#include <stddef.h>

#include <ctui/dialog.h>
#include <ctui/scrollbar.h>

#define CTUI_PICKER_PATH 4096

typedef enum ctui_picker_mode {
	CTUI_PICKER_FILE = 0,
	CTUI_PICKER_FOLDER,
	CTUI_PICKER_ANY /* a file or a folder */
} ctui_picker_mode;

typedef struct ctui_picker_entry {
	char *name;
	bool directory, symlink;
	unsigned long long size; /* bytes, files only */
} ctui_picker_entry;

typedef struct ctui_picker {
	ctui_picker_mode mode;
	char path[CTUI_PICKER_PATH];   /* the folder being browsed, absolute */
	char chosen[CTUI_PICKER_PATH]; /* the result */
	ctui_picker_entry *entries; /* "..", then "." in folder mode, then the
				       folder's content */
	size_t count;
	size_t selected, top;
	ctui_scrollbar bar; /* drag state; its geometry is derived on demand */
	bool hidden;	    /* show entries starting with a dot */
	char error[160];    /* last failure, empty when none */
	const ctui_labels
	    *labels;	   /* override of the global labels, may be NULL */
	int width, height; /* size of the dialog, 0 = 72 x 20; set after open */
	const ctui_style *style; /* colours and glyphs of this instance, may be
				    NULL; set after init */
} ctui_picker;

/* Start browsing in start (the current folder when NULL); false when no
 * folder can be read, with picker->error set. */
bool ctui_picker_open(ctui_picker *p, ctui_picker_mode mode, const char *start);
void ctui_picker_free(ctui_picker *p);
/* Set `labels` after ctui_picker_open(), which resets the whole struct. */

/* Browse dir (absolute or relative to the current folder). On failure the
 * picker stays where it was and error says why. */
bool ctui_picker_enter(ctui_picker *p, const char *dir);

/* rows: the rows of the list, ctui_picker_rows() of the dialog height; PgUp/
 * PgDn jump by one row less. */
ctui_dialog_result ctui_picker_key(ctui_picker *p, int key, size_t rows);

/* Where the dialog goes: centred on both axes, at most 72 x 20. False when
 * the screen is smaller than 40 x 10. */
bool ctui_picker_geometry(int lines, int cols, ctui_rect *box);

/* The same for this picker's own width and height (when set); a size
 * smaller than 40 x 10 is raised to it. */
bool ctui_picker_geometry_for(const ctui_picker *p, int lines, int cols,
			      ctui_rect *box);

/* Rows of the list for a dialog of box height rows. */
size_t ctui_picker_rows(int box_height);

/* Entry under screen cell (y, x), or -1. */
int ctui_picker_hit(const ctui_picker *p, int lines, int cols, int y, int x);

/* Click at (y, x): selects the entry, and activates it when it was already
 * selected; the close button cancels. Returns the dialog result. */
ctui_dialog_result ctui_picker_click(ctui_picker *p, int lines, int cols, int y,
				     int x);

/* Scrollbar in the right margin of the list, shown when the folder has
 * more entries than rows. A press goes through ctui_picker_click(); the
 * rest are these (screen coordinates; each returns true when used). */
bool ctui_picker_drag(ctui_picker *p, int lines, int cols, int y);
bool ctui_picker_release(ctui_picker *p);
bool ctui_picker_wheel(ctui_picker *p, int lines, int cols, int rows);

void ctui_picker_draw(const ctui_picker *p);

#endif /* CTUI_PICKER_H */
