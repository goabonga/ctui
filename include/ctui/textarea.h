/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * textarea.h - a multi-line UTF-8 text editor.
 *
 * The editable sibling of textview: the area owns its text (a growing,
 * NUL-terminated buffer) split at '\n'; a trailing newline starts a last,
 * empty line. Cursor and selection are byte offsets at character
 * boundaries. Columns follow textview: tabs use CTUI_TAB_WIDTH-column stops,
 * wide characters take two cells, control characters and invalid bytes are
 * drawn as '?', so the text never becomes terminal escape sequences.
 *
 * Editing operations (ctui_textarea_replace, _insert, _delete) work on byte
 * offsets and are independent of the keyboard and of the screen.
 *
 * Keys (ctui_textarea_key): printable characters, Enter, Tab (inserts a tab),
 * Backspace and Delete remove whole characters; arrows, Home / End and
 * PgUp / PgDn move the cursor and keep a goal column across lines; the
 * shifted arrows (KEY_SLEFT, KEY_SRIGHT, KEY_SR, KEY_SF, KEY_SHOME,
 * KEY_SEND, KEY_SPREVIOUS, KEY_SNEXT) extend the selection; Ctrl-A selects
 * everything; Esc clears the selection. Typing over a selection replaces
 * it. curses delivers UTF-8 input one byte at a time: the handler gathers
 * the bytes of a character before inserting it. Curses has no portable key
 * codes for word moves, so there are none.
 *
 * Ctrl-Z undoes and Ctrl-Y redoes (KEY_UNDO and KEY_REDO too). The history
 * keeps CTUI_TEXTAREA_HISTORY records; consecutive typed characters share one
 * record, any other edit or a cursor move starts a new one. Ctrl-C copies
 * the selection to the area's own clipboard, Ctrl-X cuts it and Ctrl-V
 * pastes it; the terminal must deliver these keys (raw mode) for the
 * keyboard to reach them, and the same operations are plain functions.
 *
 * Mouse: ctui_textarea_press() / _drag() / _release() take a position in the
 * area given to ctui_textarea_draw(): a click places the cursor, a drag
 * selects, and dragging just past the top or bottom scrolls at the next
 * draw. ctui_textarea_hit() is the pure lookup behind them.
 */
#ifndef CTUI_TEXTAREA_H
#define CTUI_TEXTAREA_H

#include <stdbool.h>
#include <stddef.h>

#include <ctui/theme.h>

#define CTUI_TEXTAREA_HISTORY 256 /* edit records kept for undo */

/* One undoable change: removed_len bytes at pos became added_len bytes. */
typedef struct ctui_textarea_edit {
	size_t pos;
	char *removed, *added;
	size_t removed_len, added_len;
	size_t before, after; /* cursor before and after the change */
} ctui_textarea_edit;

typedef struct ctui_textarea {
	char *text;	   /* NUL-terminated, owned */
	size_t len, cap;   /* bytes of text without the NUL; allocated bytes */
	size_t *starts;	   /* byte offset of each line */
	size_t line_count; /* at least 1 */
	size_t starts_cap;
	size_t cursor; /* byte offset, 0 .. len */
	bool selecting;
	size_t anchor;	   /* other end of the selection */
	size_t goal;	   /* column kept across vertical moves */
	size_t top, left;  /* first visible line and column */
	bool modified;	   /* the text changed since init / set_text */
	bool read_only;	   /* refuses every edit, keeps moving and selecting */
	bool line_numbers; /* draw a line-number gutter */
	size_t max_bytes;  /* size limit of the text; 0 = none */
	ctui_textarea_edit *undo,
	    *redo; /* stacks, the last element is the next to undo / redo */
	size_t undo_len, undo_cap, redo_len, redo_cap;
	bool dragging; /* a mouse drag is in progress */
	bool merge; /* the next typed character extends the last undo record */
	char *clip; /* the clipboard, NUL-terminated, owned; NULL when empty */
	size_t clip_len;
	unsigned char pending[4]; /* bytes of a UTF-8 character being typed */
	unsigned pending_len;
	const ctui_style *style; /* colours and glyphs of this instance, may be
				    NULL; set after init */
} ctui_textarea;

/* Start with a copy of initial (NULL = empty). False when out of memory. */
bool ctui_textarea_init(ctui_textarea *t, const char *initial);
void ctui_textarea_free(ctui_textarea *t);

/* The text, NUL-terminated; valid until the next edit. */
const char *ctui_textarea_text(const ctui_textarea *t);
size_t ctui_textarea_length(const ctui_textarea *t);

/* Replace the whole text: the cursor and the scroll go to the start and the
 * modified flag is cleared. False (nothing changed) when it does not fit
 * max_bytes or memory runs out. */
bool ctui_textarea_set_text(ctui_textarea *t, const char *text);

/* Line i without its newline; *len receives its length in bytes. */
const char *ctui_textarea_line(const ctui_textarea *t, size_t i, size_t *len);

/* Line and byte within the line of a byte offset, and back (the byte is
 * clamped to the line). */
void ctui_textarea_locate(const ctui_textarea *t, size_t offset, size_t *line,
			  size_t *byte);
size_t ctui_textarea_offset(const ctui_textarea *t, size_t line, size_t byte);

/* Offset of the character before / after offset (clamped to the text). */
size_t ctui_textarea_prev(const ctui_textarea *t, size_t offset);
size_t ctui_textarea_next(const ctui_textarea *t, size_t offset);

/* Display column of the cursor within its line. */
size_t ctui_textarea_column(const ctui_textarea *t);

/* Selection as the byte range [*first, *last); false when it is empty. */
bool ctui_textarea_range(const ctui_textarea *t, size_t *first, size_t *last);

/* Bytes selected. */
size_t ctui_textarea_selected_length(const ctui_textarea *t);

/* Replace del bytes at pos by n bytes of ins. False, and nothing changed,
 * when read-only, out of range, over max_bytes or out of memory. The
 * selection is dropped; the cursor keeps its place in the text (it moves to
 * the end of the insertion when it was inside the replaced bytes). */
bool ctui_textarea_replace(ctui_textarea *t, size_t pos, size_t del,
			   const char *ins, size_t n);
bool ctui_textarea_insert(ctui_textarea *t, size_t pos, const char *s,
			  size_t n);
bool ctui_textarea_delete(ctui_textarea *t, size_t pos, size_t n);

/* Undo / redo the last change and put the cursor where it was; false when
 * there is nothing to undo / redo (or read-only). Any new edit drops the
 * redo history. The modified flag is set. */
bool ctui_textarea_undo(ctui_textarea *t);
bool ctui_textarea_redo(ctui_textarea *t);

/* The clipboard of this area (never NULL; "" when empty). */
const char *ctui_textarea_clipboard(const ctui_textarea *t);
bool ctui_textarea_set_clipboard(ctui_textarea *t, const char *text);

/* Copy the selection to the clipboard; false when nothing is selected. */
bool ctui_textarea_copy(ctui_textarea *t);

/* Copy and delete the selection; false when nothing is selected or the area
 * is read-only. */
bool ctui_textarea_cut(ctui_textarea *t);

/* Insert the clipboard at the cursor over the selection; false when it is
 * empty or the edit is refused. */
bool ctui_textarea_paste(ctui_textarea *t);

/* The byte offset under (y, x), relative to the area given to the draw call.
 * The gutter counts as the start of its line, positions past the end of a
 * line or of the text give that end. False outside the width x height area. */
bool ctui_textarea_hit(const ctui_textarea *t, int width, int height, int y,
		       int x, size_t *offset);

/* Left button pressed: moves the cursor, drops the selection and starts a
 * drag. True when it hit the text area. */
bool ctui_textarea_press(ctui_textarea *t, int width, int height, int y, int x);

/* Pointer moved with the button down: extends the selection to the offset
 * under it (the nearest one when outside); true when anything changed. */
bool ctui_textarea_drag(ctui_textarea *t, int width, int height, int y, int x);

/* Button released; true when a drag ended. */
bool ctui_textarea_release(ctui_textarea *t);

/* Apply a key; rows is the page size. Returns true when anything changed. */
bool ctui_textarea_key(ctui_textarea *t, int key, size_t rows);

/* Columns taken by the gutter: 0 without line numbers, else the digits of
 * the last line number plus a margin. */
int ctui_textarea_gutter(const ctui_textarea *t);

/* Scroll so that the cursor is inside a rows x cols text area. */
void ctui_textarea_reveal(ctui_textarea *t, size_t rows, size_t cols);

/* Show the window starting at line top (clamped so the window stays full)
 * and pull the cursor into it. */
void ctui_textarea_scroll_to(ctui_textarea *t, size_t rows, size_t top);

/* Draw the gutter and the text in a width x height area. The cursor cell
 * is shown reversed when focused. */
void ctui_textarea_draw(ctui_textarea *t, int y, int x, int width, int height,
			bool focused);

#endif /* CTUI_TEXTAREA_H */
