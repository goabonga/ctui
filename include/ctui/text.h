/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * text.h - UTF-8 column arithmetic, sanitising and clipped drawing.
 *
 * Column counting is locale independent: control bytes and invalid UTF-8
 * count one column each, combining marks none, East Asian wide characters
 * and emoji two, everything else (private-use powerline glyphs included)
 * one. The drawing helpers clip to the screen and to a column budget and
 * never emit escape sequences: whatever the caller passes is sanitised first.
 */
#ifndef CTUI_TEXT_H
#define CTUI_TEXT_H

#include <stdbool.h>
#include <stddef.h>

#define CTUI_TEXT_MAX 1024 /* bytes drawn by one ctui_put call, incl. NUL */

/* Decode the character at s (non-empty): stores its code point in *cp and
 * returns its length in bytes. An invalid or truncated sequence yields
 * U+FFFD and length 1. */
size_t ctui_utf8_decode(const char *s, unsigned long *cp);

/* Display columns of one code point: 0 for combining marks, 2 for wide
 * characters, 1 otherwise (controls included). */
int ctui_codepoint_width(unsigned long cp);

/* Display columns of s. NULL counts as the empty string. */
size_t ctui_utf8_columns(const char *s);

/* Length in bytes of the longest prefix of s that fits in cols columns,
 * cut on a character boundary. */
size_t ctui_utf8_prefix_bytes(const char *s, size_t cols);

/* Copy s into out (capacity cap, NUL-terminated) keeping printable ASCII
 * and well-formed UTF-8 of printable code points; everything else becomes
 * '?'. When unicode is false every non-ASCII code point becomes one ASCII
 * cell so columns still add up. */
void ctui_sanitize(char *out, size_t cap, const char *s, bool unicode);

/* Bytes of the next line of text wrapped to width columns: it stops at a
 * newline, or at the last space that fits, or hard at width when a word is
 * longer than the line. Leading spaces of the following line are not
 * included; use ctui_wrap_skip() to step over them. */
size_t ctui_wrap_chunk(const char *text, int width);

/* Offset of the start of the next line after a chunk of len bytes. */
size_t ctui_wrap_skip(const char *text, size_t len);

/* Draw s at (y, x) in at most maxcols columns with attr. Returns the
 * columns used (0 when off screen). */
int ctui_put(int y, int x, int maxcols, const char *s, int attr);

/* Fill width cells of row y, starting at column x, with spaces in attr. */
void ctui_fill(int y, int x, int width, int attr);

/* Draw text wrapped to width columns on at most rows rows; an overflowing
 * last row ends with "...". Returns the rows used. */
int ctui_wrapped(int y, int x, int width, int rows, const char *text, int attr);

#endif /* CTUI_TEXT_H */
