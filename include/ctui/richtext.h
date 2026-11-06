/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * richtext.h - styled text made of spans, wrapped, aligned and hit tested.
 *
 * A span is a UTF-8 slice with a style: a theme colour pair, attributes
 * (bold, dim, italic, underline, reverse) and a link id. A line is a run of
 * spans and a document an array of lines. Nothing is allocated: the spans
 * point at text the caller owns, and the document built by ctui_rt_parse()
 * lives in caller-owned arrays.
 *
 * Markup grammar read by ctui_rt_parse():
 *
 *   [b] [d] [i] [u] [r]     bold, dim, italic, underline, reverse
 *   [fg=NAME]               colour: body muted heading ok warn err info
 *                           accent sel
 *   [link=N]                link id N (1 to 999999999), see ctui_rt_hit()
 *   [/]                     close the innermost open tag
 *   [/b] [/d] [/i] [/u] [/r] [/fg] [/link]
 *                           close the innermost tag when it is of that kind
 *   \[  \\                  a literal '[' and a literal backslash
 *   newline                 ends the line ('\r' is ignored)
 *
 * Anything else is literal text: an unknown tag, an unknown colour, a close
 * tag that matches nothing, a tag nested deeper than CTUI_RT_DEPTH, an
 * unterminated '['. A tag still open at the end of the markup simply runs
 * to the end. Parsing never fails on its input; it only stops early when
 * the caller's arrays are full. strlen(markup) + 1 entries of each array
 * are always enough.
 *
 * A span text must not contain a newline: a line break is a new line.
 * Control bytes and invalid UTF-8 are drawn as '?' (one column each).
 */
#ifndef CTUI_RICHTEXT_H
#define CTUI_RICHTEXT_H

#include <stdbool.h>
#include <stddef.h>

#include <ctui/theme.h>

#define CTUI_RT_DEPTH 16 /* nesting depth of the markup tags */

/* Bits of ctui_span.attrs. */
#define CTUI_RT_BOLD 1U
#define CTUI_RT_DIM 2U
#define CTUI_RT_ITALIC 4U
#define CTUI_RT_UNDERLINE 8U
#define CTUI_RT_REVERSE 16U

typedef struct ctui_span {
	const char *text; /* not NUL-terminated, len bytes */
	size_t len;
	int pair;	/* a ctui_pair; 0 = CTUI_PAIR_BODY */
	unsigned attrs; /* CTUI_RT_* bits */
	int link;	/* 0 = not a link */
} ctui_span;

typedef struct ctui_rt_line {
	size_t first, count; /* range in the span array; count 0 = empty line */
} ctui_rt_line;

typedef struct ctui_rt_doc {
	ctui_span *spans;
	size_t span_count, span_cap;
	ctui_rt_line *lines;
	size_t line_count, line_cap;
	char *text; /* storage of the text of parsed spans */
	size_t text_len, text_cap;
	bool overflow; /* the last parse ran out of room */
} ctui_rt_doc;

typedef enum ctui_rt_align {
	CTUI_RT_LEFT,
	CTUI_RT_CENTER,
	CTUI_RT_RIGHT,
	CTUI_RT_JUSTIFY /* wrapped text only; the last row of a paragraph stays
			   left */
} ctui_rt_align;

typedef struct ctui_rt_view {
	ctui_rt_align align;
	bool wrap; /* wrap at spaces (hard cut for longer words); else one row
		      per line */
	bool ellipsis; /* cut rows get the theme ellipsis: long rows when not
			  wrapping, the last visible row when text goes on below
			  it */
	size_t top;    /* first row shown */
} ctui_rt_view;

/* Use the given arrays (capacities in elements / bytes) and empty the
 * document. */
void ctui_rt_doc_init(ctui_rt_doc *doc, char *text, size_t text_cap,
		      ctui_span *spans, size_t span_cap, ctui_rt_line *lines,
		      size_t line_cap);

/* Replace the content of doc by the parsed markup. Returns false when the
 * arrays were too small (doc.overflow is set and holds what fitted). */
bool ctui_rt_parse(ctui_rt_doc *doc, const char *markup);

/* curses attribute of a span (pair, bold, dim, italic, underline, reverse). */
int ctui_rt_attr(const ctui_span *span);

/* Rows of the document wrapped (or not) to width columns; 0 when width <= 0. */
size_t ctui_rt_height(const ctui_rt_doc *doc, int width, bool wrap);

/* Plain text of one row, unaligned. False when row does not exist. */
bool ctui_rt_row_text(const ctui_rt_doc *doc, int width, bool wrap, size_t row,
		      char *out, size_t cap);

/* Largest useful view->top for a viewport of height rows. */
size_t ctui_rt_max_top(const ctui_rt_doc *doc, int width, bool wrap,
		       int height);

/* Move view->top by delta rows, kept in [0, max]. True when it changed. */
bool ctui_rt_scroll(ctui_rt_view *view, const ctui_rt_doc *doc, int width,
		    int height, int delta);

/* Draw the rows from view->top clipped to the width x height rectangle at
 * (y, x). Cells outside the text are left alone. Returns the rows drawn. */
int ctui_rt_draw(const ctui_rt_doc *doc, const ctui_rt_view *view, int y, int x,
		 int width, int height);

/* The span under the cell (ry, rx) of the rectangle drawn by ctui_rt_draw()
 * with the same arguments; false over padding or past the text. */
bool ctui_rt_hit(const ctui_rt_doc *doc, const ctui_rt_view *view, int width,
		 int height, int ry, int rx, size_t *span);

/* Link id of the span under the cell, 0 when none. */
int ctui_rt_link_at(const ctui_rt_doc *doc, const ctui_rt_view *view, int width,
		    int height, int ry, int rx);

#endif /* CTUI_RICHTEXT_H */
