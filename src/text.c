/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * text.c - UTF-8 column arithmetic, sanitising and clipped drawing (text.h).
 */
#include <ctui/text.h>

#include <curses.h>
#include <string.h>

#include <ctui/theme.h>

size_t ctui_utf8_decode(const char *s, unsigned long *cp)
{
	const unsigned char *p = (const unsigned char *)s;
	unsigned c = *p;
	size_t n = 0;
	unsigned long v = 0;
	if (c < 0x80) {
		*cp = c;
		return 1;
	}
	if (c >= 0xC2 && c <= 0xDF) {
		n = 2;
		v = c & 0x1FU;
	} else if (c >= 0xE0 && c <= 0xEF) {
		n = 3;
		v = c & 0x0FU;
	} else if (c >= 0xF0 && c <= 0xF4) {
		n = 4;
		v = c & 0x07U;
	}
	bool ok = n > 0;
	for (size_t i = 1; ok && i < n; i++) {
		/* The loop stops at the NUL terminator; the analyzer loses the
		 * contents of string literals passed down from callers. */
		// NOLINTNEXTLINE(clang-analyzer-security.ArrayBound)
		unsigned b = p[i];
		if (b == 0 || (b & 0xC0U) != 0x80U)
			ok = false;
		else
			v = (v << 6U) | (b & 0x3FU);
	}
	/* No overlongs, surrogates, C1 controls or out-of-range points. */
	if (ok && ((n == 3 && v < 0x800) ||
		   (n == 4 && (v < 0x10000 || v > 0x10FFFF)) ||
		   (v >= 0xD800 && v <= 0xDFFF) || v < 0xA0))
		ok = false;
	if (!ok) {
		*cp = 0xFFFD;
		return 1;
	}
	*cp = v;
	return n;
}

int ctui_codepoint_width(unsigned long cp)
{
	if (cp < 0x20 || cp == 0x7F)
		return 1;
	if ((cp >= 0x0300 && cp <= 0x036F) || (cp >= 0x200B && cp <= 0x200F) ||
	    (cp >= 0xFE00 && cp <= 0xFE0F))
		return 0;
	if ((cp >= 0x1100 && cp <= 0x115F) || (cp >= 0x2E80 && cp <= 0xA4CF) ||
	    (cp >= 0xAC00 && cp <= 0xD7A3) || (cp >= 0xF900 && cp <= 0xFAFF) ||
	    (cp >= 0xFE30 && cp <= 0xFE6F) || (cp >= 0xFF00 && cp <= 0xFF60) ||
	    (cp >= 0xFFE0 && cp <= 0xFFE6) ||
	    (cp >= 0x1F300 && cp <= 0x1F64F) ||
	    (cp >= 0x1F900 && cp <= 0x1F9FF) ||
	    (cp >= 0x20000 && cp <= 0x3FFFD))
		return 2;
	return 1;
}

size_t ctui_utf8_columns(const char *s)
{
	size_t cols = 0;
	const unsigned char *p = (const unsigned char *)(s ? s : "");
	while (*p) {
		unsigned long cp;
		p += ctui_utf8_decode((const char *)p, &cp);
		cols += (size_t)ctui_codepoint_width(cp);
	}
	return cols;
}

size_t ctui_utf8_prefix_bytes(const char *s, size_t cols)
{
	const unsigned char *start = (const unsigned char *)(s ? s : ""),
			    *p = start;
	size_t used = 0;
	while (*p) {
		unsigned long cp;
		size_t n = ctui_utf8_decode((const char *)p, &cp);
		size_t w = (size_t)ctui_codepoint_width(cp);
		if (used + w > cols)
			break;
		used += w;
		p += n;
	}
	return (size_t)(p - start);
}

static char ascii_for(unsigned long cp)
{
	switch (cp) {
	case 0x00B0:
		return 'o';
	case 0x2013:
	case 0x2014:
	case 0x2500:
		return '-';
	case 0x2502:
		return '|';
	case 0x2588:
		return '#';
	default:
		return '?';
	}
}

void ctui_sanitize(char *out, size_t cap, const char *s, bool unicode)
{
	if (cap == 0)
		return;
	size_t o = 0;
	const unsigned char *p = (const unsigned char *)(s ? s : "");
	// NOLINTNEXTLINE(clang-analyzer-security.ArrayBound)
	while (*p && o + 1 < cap) {
		unsigned long cp;
		size_t n = ctui_utf8_decode((const char *)p, &cp);
		if (n == 1) {
			bool printable =
			    cp != 0xFFFD && *p >= 0x20 && *p != 0x7F;
			out[o++] = printable ? (char)*p : '?';
			p += n;
		} else if (!unicode) {
			out[o++] = ascii_for(cp);
			p += n;
		} else {
			if (o + n >= cap)
				break;
			memcpy(out + o, p, n);
			o += n;
			p += n;
		}
	}
	out[o] = '\0';
}

size_t ctui_wrap_chunk(const char *text, int width)
{
	if (!text || width <= 0)
		return 0;
	size_t nl = strcspn(text, "\n");
	size_t fit = ctui_utf8_prefix_bytes(text, (size_t)width);
	if (nl <= fit)
		return nl;
	if (text[fit] == ' ')
		return fit;
	size_t n = fit;
	while (n > 0 && text[n - 1] != ' ')
		n--;
	if (n == 0)
		return fit ? fit : 1;
	return n - 1;
}

size_t ctui_wrap_skip(const char *text, size_t len)
{
	size_t n = len;
	if (text[n] == '\n')
		return n + 1;
	while (text[n] == ' ')
		n++;
	return n;
}

int ctui_put(int y, int x, int maxcols, const char *s, int attr)
{
	if (maxcols <= 0 || y < 0 || y >= LINES || x < 0 || x >= COLS)
		return 0;
	if (x + maxcols > COLS)
		maxcols = COLS - x;
	char buf[CTUI_TEXT_MAX];
	ctui_sanitize(buf, sizeof buf, s, ctui_theme_unicode());
	size_t bytes = ctui_utf8_prefix_bytes(buf, (size_t)maxcols);
	buf[bytes] = '\0';
	int cols = (int)ctui_utf8_columns(buf);
	attrset((attr_t)attr);
	mvaddnstr(y, x, buf, (int)bytes);
	attrset(A_NORMAL);
	return cols;
}

void ctui_fill(int y, int x, int width, int attr)
{
	if (y < 0 || y >= LINES || x < 0 || x >= COLS || width <= 0)
		return;
	if (width > COLS - x)
		width = COLS - x;
	attrset((attr_t)attr);
	mvhline(y, x, ' ', width);
	attrset(A_NORMAL);
}

int ctui_wrapped(int y, int x, int width, int rows, const char *text, int attr)
{
	if (!text || width <= 0)
		return 0;
	int row = 0;
	while (*text && row < rows) {
		size_t len = ctui_wrap_chunk(text, width);
		char line[CTUI_TEXT_MAX];
		size_t take = len < sizeof line - 1 ? len : sizeof line - 1;
		memcpy(line, text, take);
		line[take] = '\0';
		ctui_put(y + row, x, width, line, attr);
		text += ctui_wrap_skip(text, len);
		row++;
		if (*text && row == rows && width >= 3)
			ctui_put(y + row - 1, x + width - 3, 3, "...", attr);
	}
	return row;
}
