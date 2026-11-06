/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * bigtext.h - large text made of block glyphs.
 *
 * Every character is a cell of CTUI_BIGTEXT_CELL_W x CTUI_BIGTEXT_HEIGHT dots,
 * drawn with a full block (or '#' without UTF-8), with one blank column
 * between characters. The font covers the digits, the letters (lowercase
 * is drawn as uppercase), space and ": . - /". Any other character is
 * drawn blank, one cell wide per code point.
 */
#ifndef CTUI_BIGTEXT_H
#define CTUI_BIGTEXT_H

#include <ctui/theme.h>

#define CTUI_BIGTEXT_CELL_W 3 /* columns of a character */
#define CTUI_BIGTEXT_HEIGHT 5 /* rows of a character */

typedef struct ctui_bigtext {
	ctui_pair pair;		 /* colour of the dots: 0 = CTUI_PAIR_HEADING */
	const ctui_style *style; /* colours and glyphs of this instance, may be
				    NULL; set after init */
} ctui_bigtext;

/* Dots of row (0 = top) of the cell of code point cp, as 3 bits with the
 * leftmost dot in bit 2. Unknown code points and rows out of range give 0. */
unsigned ctui_bigtext_row(unsigned long cp, int row);

/* Columns s takes: 3 per character plus one between them; 0 when empty. */
int ctui_bigtext_width(const char *s);

/* Draw s with its top left at (y, x), keeping to at most maxcols columns:
 * a character that does not fit is cut at the edge. Returns the columns
 * of s that were drawn. */
int ctui_bigtext_draw(const ctui_bigtext *b, int y, int x, int maxcols,
		      const char *s);

#endif /* CTUI_BIGTEXT_H */
