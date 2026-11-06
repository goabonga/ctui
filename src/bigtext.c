/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * bigtext.c - large text made of block glyphs (bigtext.h).
 */
#include <ctui/bigtext.h>

#include <ctui/text.h>

/* One row is three bits, leftmost dot highest. */
#define R(a, b, c, d, e) {(a), (b), (c), (d), (e)}

static const unsigned char DIGITS[10][CTUI_BIGTEXT_HEIGHT] = {
    R(7, 5, 5, 5, 7), R(2, 6, 2, 2, 7), R(7, 1, 7, 4, 7), R(7, 1, 7, 1, 7),
    R(5, 5, 7, 1, 1), R(7, 4, 7, 1, 7), R(7, 4, 7, 5, 7), R(7, 1, 1, 2, 2),
    R(7, 5, 7, 5, 7), R(7, 5, 7, 1, 7),
};

static const unsigned char LETTERS[26][CTUI_BIGTEXT_HEIGHT] = {
    R(2, 5, 7, 5, 5), R(6, 5, 6, 5, 6), R(3, 4, 4, 4, 3), R(6, 5, 5, 5, 6),
    R(7, 4, 6, 4, 7), R(7, 4, 6, 4, 4), R(3, 4, 5, 5, 3), R(5, 5, 7, 5, 5),
    R(7, 2, 2, 2, 7), R(1, 1, 1, 5, 2), R(5, 5, 6, 5, 5), R(4, 4, 4, 4, 7),
    R(5, 7, 7, 5, 5), R(6, 5, 5, 5, 5), R(2, 5, 5, 5, 2), R(6, 5, 6, 4, 4),
    R(2, 5, 5, 7, 3), R(6, 5, 6, 5, 5), R(3, 4, 2, 1, 6), R(7, 2, 2, 2, 2),
    R(5, 5, 5, 5, 7), R(5, 5, 5, 5, 2), R(5, 5, 7, 7, 5), R(5, 5, 2, 5, 5),
    R(5, 5, 2, 2, 2), R(7, 1, 2, 4, 7),
};

static const unsigned char COLON[CTUI_BIGTEXT_HEIGHT] = R(0, 2, 0, 2, 0);
static const unsigned char DOT[CTUI_BIGTEXT_HEIGHT] = R(0, 0, 0, 0, 2);
static const unsigned char DASH[CTUI_BIGTEXT_HEIGHT] = R(0, 0, 7, 0, 0);
static const unsigned char SLASH[CTUI_BIGTEXT_HEIGHT] = R(1, 1, 2, 4, 4);

unsigned ctui_bigtext_row(unsigned long cp, int row)
{
	if (row < 0 || row >= CTUI_BIGTEXT_HEIGHT)
		return 0;
	if (cp >= '0' && cp <= '9')
		return DIGITS[cp - '0'][row];
	if (cp >= 'A' && cp <= 'Z')
		return LETTERS[cp - 'A'][row];
	if (cp >= 'a' && cp <= 'z')
		return LETTERS[cp - 'a'][row];
	switch (cp) {
	case ':':
		return COLON[row];
	case '.':
		return DOT[row];
	case '-':
		return DASH[row];
	case '/':
		return SLASH[row];
	default:
		return 0;
	}
}

int ctui_bigtext_width(const char *s)
{
	int n = 0;
	while (s && *s) {
		unsigned long cp;
		s += ctui_utf8_decode(s, &cp);
		n++;
	}
	return n ? n * CTUI_BIGTEXT_CELL_W + (n - 1) : 0;
}

int ctui_bigtext_draw(const ctui_bigtext *b, int y, int x, int maxcols,
		      const char *s)
{
	ctui_style_begin(b->style);
	const char *block = ctui_theme_glyphs()->unicode ? "\xe2\x96\x88" : "#";
	int attr = ctui_attr(b->pair ? b->pair : CTUI_PAIR_HEADING, true);
	int col = 0;
	while (s && *s && col < maxcols) {
		unsigned long cp;
		size_t len = ctui_utf8_decode(s, &cp);
		s += len;
		for (int row = 0; row < CTUI_BIGTEXT_HEIGHT; row++) {
			unsigned bits = ctui_bigtext_row(cp, row);
			for (int dx = 0;
			     dx < CTUI_BIGTEXT_CELL_W && col + dx < maxcols;
			     dx++)
				if (bits &
				    (1U << (unsigned)(CTUI_BIGTEXT_CELL_W - 1 -
						      dx)))
					ctui_put(y + row, x + col + dx, 1,
						 block, attr);
		}
		col += CTUI_BIGTEXT_CELL_W + 1;
	}
	ctui_style_end();
	/* col counts the gap after the last character, which is not drawn */
	col = col > 0 ? col - 1 : 0;
	return col > maxcols ? maxcols : col;
}
