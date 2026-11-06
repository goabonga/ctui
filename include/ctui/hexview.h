/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * hexview.h - a read-only hexadecimal viewer for binary data.
 *
 * Each row shows an offset, the bytes in hexadecimal (a wider gap after
 * every 8) and the same bytes as text, where anything that is not printable
 * ASCII is drawn as '.', so file contents never become terminal escape
 * sequences. The row takes 16, 8 or 4 bytes, whichever fits the width.
 *
 * The data is owned by the caller and must outlive the view, or is read
 * from a file with ctui_hexview_load() or mapped with ctui_hexview_map(). The
 * view keeps a cursor (a byte offset), an optional selection and the scroll
 * position.
 *
 * Keys: arrows move by a byte or a row, PgUp / PgDn by a page, Home / End
 * to the start or end of the row; the shifted arrows (KEY_SLEFT,
 * KEY_SRIGHT, KEY_SR, KEY_SF) extend the selection; Ctrl-A selects
 * everything; Esc clears the selection.
 *
 * Mouse: ctui_hexview_press() / _drag() / _release() take a position in
 * the area given to ctui_hexview_draw(), on the hex bytes or on the text.
 */
#ifndef CTUI_HEXVIEW_H
#define CTUI_HEXVIEW_H

#include <stdbool.h>
#include <stddef.h>

#include <ctui/theme.h>

#define CTUI_HEXVIEW_MAX                                                       \
	((size_t)64 << 20U) /* default size limit of ctui_hexview_load() */

/* A byte replaced by the user. */
typedef struct ctui_hexedit {
	size_t offset;
	unsigned char value;
} ctui_hexedit;

typedef enum ctui_hexpane {
	CTUI_HEXPANE_HEX = 0,
	CTUI_HEXPANE_TEXT
} ctui_hexpane;

typedef struct ctui_hexview {
	const unsigned char *data;
	size_t size;
	size_t cursor; /* byte offset, 0 .. size - 1 */
	bool selecting;
	size_t anchor;	  /* other end of the selection */
	size_t top;	  /* first visible row */
	size_t row_bytes; /* bytes per row, as of the last draw (16 before) */
	bool dragging;
	unsigned char *owned; /* buffer read by ctui_hexview_load(), freed by
				 ctui_hexview_free() */
	void *mapping;	      /* file mapped by ctui_hexview_map(), unmapped by
				 ctui_hexview_free() */
	size_t mapping_len;
	ctui_hexedit
	    *edits; /* edits over the data, sorted by offset, one per byte */
	size_t nedits, cap_edits;
	bool editing;	   /* typing changes bytes; set by the caller */
	ctui_hexpane pane; /* where typing goes while editing */
	bool pending; /* the high nibble of the cursor byte was just typed */
	const ctui_style *style; /* colours and glyphs of this instance, may be
				    NULL; set after init */
} ctui_hexview;

/* Where the parts of a row sit, for a given width. */
typedef struct ctui_hexlayout {
	int per_row; /* bytes per row */
	int digits;  /* hexadecimal digits of the offset */
	int hex_x;   /* column of the first byte */
	int text_x;  /* column of the first text character */
	int width;   /* columns used */
} ctui_hexlayout;

/* Show size bytes at data (which may be NULL when size is 0). */
void ctui_hexview_init(ctui_hexview *v, const void *data, size_t size);

/* Read a whole file. limit is the largest size accepted (0 = CTUI_HEXVIEW_MAX).
 * On failure returns false with errno set (EFBIG when the file is larger
 * than the limit) and the view empty. */
bool ctui_hexview_load(ctui_hexview *v, const char *path, size_t limit);

/* Map a file read-only (MAP_PRIVATE) without any size limit: only the pages
 * that are shown are read. An empty file gives an empty view; anything that
 * is not a regular file (a pipe, a device) is read like ctui_hexview_load()
 * does. On failure returns false with errno set and the view empty.
 *
 * Caveat: if another process truncates the file while it is mapped, reading
 * the bytes past the new end raises SIGBUS. Use ctui_hexview_load() when the
 * file may shrink. */
bool ctui_hexview_map(ctui_hexview *v, const char *path);

/* Release what ctui_hexview_load() or ctui_hexview_map() acquired. */
void ctui_hexview_free(ctui_hexview *v);

/* The byte at offset (0 past the end). Always read bytes through this
 * function rather than through v->data. */
unsigned char ctui_hexview_byte(const ctui_hexview *v, size_t offset);

/* ---- editing ------------------------------------------------------
 *
 * Edits form an overlay over the data: neither the buffer given to
 * ctui_hexview_init() nor a mapped file is ever modified, and
 * ctui_hexview_byte() returns the edited value. Edited bytes are drawn with
 * the CTUI_PAIR_ERROR attribute (overridable through the style).
 *
 * While v->editing is set, ctui_hexview_key() types into the byte under the
 * cursor: in the hex pane two hexadecimal digits set a byte (high nibble,
 * then low nibble, then the cursor advances); in the text pane a printable
 * character sets the byte and advances. Tab switches the pane. Backspace
 * reverts the byte before the cursor and steps back (when a high nibble is
 * pending it reverts the cursor byte instead). Any other key, navigation
 * included, cancels a pending nibble. Leaving the edit mode (clearing
 * v->editing) is up to the caller. */

/* Replace the byte at offset; setting the original value removes the edit.
 * False when offset is past the data. */
bool ctui_hexview_set(ctui_hexview *v, size_t offset, unsigned char value);

/* The byte at offset as it is in the data, without the edits. */
unsigned char ctui_hexview_original(const ctui_hexview *v, size_t offset);

/* Whether the byte at offset has been edited. */
bool ctui_hexview_edited(const ctui_hexview *v, size_t offset);

/* Number of edited bytes. */
size_t ctui_hexview_modified(const ctui_hexview *v);

/* Drop every edit. */
void ctui_hexview_revert(ctui_hexview *v);

/* Write the data with the edits to path, in chunks, through a temporary file
 * in the same directory that then replaces path, so path may be the file the
 * view reads. The edits are kept: reload the file to see it as the new
 * data. On failure returns false with errno set and path untouched. */
bool ctui_hexview_save(const ctui_hexview *v, const char *path);

/* ---- search -------------------------------------------------------
 *
 * Searches see the edited bytes. A pattern is a few bytes; ctui_hexview_parse()
 * builds one from what the user typed. */

typedef enum ctui_hexfind {
	CTUI_HEXFIND_FOUND, /* *pos is the offset of the first byte of a match
			     */
	CTUI_HEXFIND_NOTFOUND, /* the end of the data was reached in that
				  direction */
	CTUI_HEXFIND_MORE /* the budget ran out: call again, *pos is where to
			     resume */
} ctui_hexfind;

/* A pattern from text: "quoted text" is taken byte for byte (without the
 * quotes); otherwise an even number of hexadecimal digits, spaces allowed
 * between them ("48 65 6c" or "48656c"), is read as bytes; anything else is
 * literal text. At most cap bytes. False when the text is empty or too long. */
bool ctui_hexview_parse(const char *text, unsigned char *out, size_t cap,
			size_t *len);

/* Look for the pattern of len bytes from offset *pos, going forward or
 * backward, examining at most budget start offsets (0 = no limit) so that a
 * large file can be searched a slice at a time while the interface stays
 * responsive. See ctui_hexfind for the result and for *pos. */
ctui_hexfind ctui_hexview_find(const ctui_hexview *v,
			       const unsigned char *pattern, size_t len,
			       size_t *pos, bool forward, size_t budget);

/* Search after the cursor (before it when backward), wrap once around the
 * data and select the match, with the cursor on its first byte. Returns
 * false, leaving the view alone, when there is no match. */
bool ctui_hexview_find_next(ctui_hexview *v, const unsigned char *pattern,
			    size_t len, bool forward);

/* Put the cursor on offset and drop the selection; false (view unchanged)
 * when offset is past the data. The next draw scrolls to it. */
bool ctui_hexview_goto(ctui_hexview *v, size_t offset);

/* The layout for a width; false when even 4 bytes per row do not fit. */
bool ctui_hexview_layout(const ctui_hexview *v, int width, ctui_hexlayout *out);

/* Rows of data (at least 1). */
size_t ctui_hexview_rows(const ctui_hexview *v);

/* Whether the byte at offset is inside the selection. */
bool ctui_hexview_selected(const ctui_hexview *v, size_t offset);

/* Selection as [*first, *last] inclusive; false when none. */
bool ctui_hexview_range(const ctui_hexview *v, size_t *first, size_t *last);

/* Apply a key; rows is the page size. Returns true when anything changed. */
bool ctui_hexview_key(ctui_hexview *v, int key, size_t rows);

/* Scroll so that the cursor row is inside a window of rows. */
void ctui_hexview_reveal(ctui_hexview *v, size_t rows);

/* Show the window starting at row top (clamped so the window stays full)
 * and pull the cursor into it. */
void ctui_hexview_scroll_to(ctui_hexview *v, size_t rows, size_t top);

/* The byte under (y, x), relative to the area given to the draw call, on
 * the hex or the text part of a row. False between bytes or past the data. */
bool ctui_hexview_hit(const ctui_hexview *v, int width, int y, int x,
		      size_t *offset);

/* Left button pressed: moves the cursor, drops the selection and starts a
 * drag. True when it hit a byte. */
bool ctui_hexview_press(ctui_hexview *v, int width, int y, int x);

/* Pointer moved with the button down: extends the selection to the byte
 * under it (the nearest one when outside, so the view scrolls at the
 * edges at the next draw); true when anything changed. */
bool ctui_hexview_drag(ctui_hexview *v, int width, int y, int x);

/* Button released; true when a drag ended. */
bool ctui_hexview_release(ctui_hexview *v);

/* Draw rows of data in a width x height area. The cursor byte is shown
 * reversed when focused. */
void ctui_hexview_draw(ctui_hexview *v, int y, int x, int width, int height,
		       bool focused);

#endif /* CTUI_HEXVIEW_H */
