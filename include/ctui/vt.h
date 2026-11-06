/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * vt.h - a VT100 / xterm terminal emulator: a byte-stream parser feeding a
 * grid of cells.
 *
 * Pure core: no curses, no pty, no I/O. The host writes the bytes a program
 * printed with ctui_vt_feed() and reads the grid back through the accessors;
 * the answers the terminal owes the program (cursor position reports,
 * device attributes) accumulate in a response buffer that the host takes with
 * ctui_vt_take_response() and writes back to the program.
 *
 * Supported: UTF-8 input (invalid sequences become U+FFFD; wide characters
 * take two cells, combining marks attach to the previous cell, one per
 * cell); C0 controls BS HT LF VT FF CR SO SI; ESC 7 8 D E M H c = > and the
 * DEC special graphics set on G0 / G1 (ESC ( 0, ESC ) 0); CSI cursor
 * movement (A B C D E F G H I Z ` a d e f), save / restore (ESC 7 / 8,
 * CSI s / u), erase (J K X), insert / delete (@ L M P), scroll (S T),
 * scroll region (r), tab stops (ESC H, g), SGR (16 colours, bright, 256
 * colours and truecolour with ; or : separators; bold dim italic underline
 * reverse hidden; blink and strike-through are accepted and ignored),
 * modes 4 (insert), 20 (newline) and private 1 6 7 25 47 1000 1002 1003
 * 1004 1006 1047 1049 2004, device status (CSI 5 n, 6 n, ? 6 n), primary
 * and secondary device attributes, OSC 0 / 2 titles and ESC # 8. The
 * primary screen keeps a bounded scrollback.
 *
 * Not supported (parsed and ignored, never answered): DCS, APC, PM and SOS
 * strings, OSC other than 0 / 1 / 2 (colours, hyperlinks, clipboard),
 * 8-bit C1 controls, double-width / height lines, left-right margins, mouse
 * encodings 1005 / 1015, window manipulation (CSI t), cursor style,
 * soft reset, sixel and REP. Resizing truncates and extends, it never
 * reflows.
 *
 * Everything is bounded: sizes are capped (CTUI_VT_MAX_ROWS / _COLS), CSI
 * parameters are capped in count and value, an OSC string keeps its first
 * CTUI_VT_OSC_MAX bytes, the title CTUI_VT_TITLE_MAX bytes and the response
 * buffer CTUI_VT_RESPONSE_MAX bytes (more is dropped); any byte stream is
 * safe.
 *
 * Colours are stored as 32-bit values: 0 is the default colour,
 * CTUI_VT_INDEXED(n) a palette index 0-255 and CTUI_VT_RGB(r, g, b) truecolour;
 * the renderer quantises.
 */
#ifndef CTUI_VT_H
#define CTUI_VT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CTUI_VT_MAX_ROWS 500
#define CTUI_VT_MAX_COLS 1000
#define CTUI_VT_MAX_SCROLLBACK 100000
#define CTUI_VT_OSC_MAX 512
#define CTUI_VT_TITLE_MAX 256
#define CTUI_VT_RESPONSE_MAX 256

#define CTUI_VT_INDEXED(n) ((uint32_t)((1U << 24U) | ((unsigned)(n) & 0xFFU)))
#define CTUI_VT_RGB(r, g, b)                                                   \
	((uint32_t)((2U << 24U) | (((unsigned)(r) & 0xFFU) << 16U) |           \
		    (((unsigned)(g) & 0xFFU) << 8U) |                          \
		    ((unsigned)(b) & 0xFFU)))
#define CTUI_VT_COLOR_KIND(c) ((c) >> 24U) /* 0 default, 1 indexed, 2 rgb */

/* Bits of ctui_vt_cell.attrs. */
#define CTUI_VT_BOLD 1U
#define CTUI_VT_DIM 2U
#define CTUI_VT_ITALIC 4U
#define CTUI_VT_UNDERLINE 8U
#define CTUI_VT_REVERSE 16U
#define CTUI_VT_HIDDEN 32U

typedef struct ctui_vt_cell {
	uint32_t cp;	 /* code point, 0 = empty */
	uint32_t comb;	 /* a combining mark attached to cp, 0 = none */
	uint32_t fg, bg; /* colours, see above */
	uint8_t attrs;	 /* CTUI_VT_* bits */
	uint8_t width; /* 1; 2 for the left half of a wide character; 0 for its
			  right half */
} ctui_vt_cell;

typedef struct ctui_vt_modes {
	bool app_cursor; /* DECCKM: arrows send ESC O x */
	bool app_keypad; /* DECKPAM */
	bool bracketed_paste;
	bool autowrap;	     /* DECAWM, on by default */
	bool origin;	     /* DECOM */
	bool insert;	     /* IRM */
	bool newline;	     /* LNM: LF also does CR */
	bool cursor_visible; /* DECTCEM, on by default */
	bool alt_screen;
	bool mouse_sgr;	   /* 1006 */
	bool focus_events; /* 1004 */
	int mouse;	   /* 0 off, or 1000 / 1002 / 1003 */
} ctui_vt_modes;

typedef struct ctui_vt ctui_vt;

/* A terminal of rows x cols keeping up to scrollback lines of history.
 * NULL when the size is out of range or memory runs out. */
ctui_vt *ctui_vt_new(int rows, int cols, size_t scrollback);
void ctui_vt_free(ctui_vt *vt);

/* Process bytes from the program. */
void ctui_vt_feed(ctui_vt *vt, const void *bytes, size_t len);

/* Change the size, keeping the top-left part of the screen (the primary
 * screen pushes its top lines into the history when the cursor would fall
 * off). False, nothing changed, when out of range or out of memory. */
bool ctui_vt_resize(ctui_vt *vt, int rows, int cols);

int ctui_vt_rows(const ctui_vt *vt);
int ctui_vt_cols(const ctui_vt *vt);

/* A cell of the visible screen; NULL when out of range. */
const ctui_vt_cell *ctui_vt_cell_at(const ctui_vt *vt, int row, int col);

/* Visible row as UTF-8 without trailing blanks. Returns the length (the
 * text is cut to fit and always NUL-terminated when cap > 0). */
size_t ctui_vt_row_text(const ctui_vt *vt, int row, char *buf, size_t cap);

int ctui_vt_cursor_row(const ctui_vt *vt);
int ctui_vt_cursor_col(const ctui_vt *vt);
const ctui_vt_modes *ctui_vt_get_modes(const ctui_vt *vt);

/* Title set by OSC 0 / 2, "" when none. */
const char *ctui_vt_title(const ctui_vt *vt);

/* Lines in the history of the primary screen, and the total ever pushed
 * (it keeps counting when the ring is full). */
size_t ctui_vt_history_count(const ctui_vt *vt);
size_t ctui_vt_history_pushed(const ctui_vt *vt);

/* A cell of history line `age` (0 = the most recent), column col of the
 * current width; NULL when out of range. Cells past the end of a stored
 * line are blank. */
const ctui_vt_cell *ctui_vt_history_cell(const ctui_vt *vt, size_t age,
					 int col);
size_t ctui_vt_history_text(const ctui_vt *vt, size_t age, char *buf,
			    size_t cap);

/* Move up to cap bytes of pending responses into buf; returns the count. */
size_t ctui_vt_take_response(ctui_vt *vt, char *buf, size_t cap);

/* Changes whenever input was processed: compare with the last value to
 * know that a redraw is due. */
unsigned long ctui_vt_generation(const ctui_vt *vt);

#endif /* CTUI_VT_H */
