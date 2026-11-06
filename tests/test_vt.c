/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */
#include "check.h"

#include <stdint.h>
#include <stdlib.h>

#include <ctui/vt.h>

#define FEED(vt, s) ctui_vt_feed((vt), (s), strlen(s))

/* The screen as its rows joined by '|', trailing empty rows left out. */
static void screen_str(const ctui_vt *vt, char *out, size_t cap)
{
	int last = -1;
	char row[512];
	for (int r = 0; r < ctui_vt_rows(vt); r++) {
		ctui_vt_row_text(vt, r, row, sizeof row);
		if (row[0])
			last = r;
	}
	size_t n = 0;
	out[0] = '\0';
	for (int r = 0; r <= last; r++) {
		ctui_vt_row_text(vt, r, row, sizeof row);
		n += (size_t)snprintf(out + n, cap - n, "%s%s", r ? "|" : "",
				      row);
	}
}

typedef struct {
	const char *name, *input, *want;
	int rows, cols;
	int cy, cx; /* expected cursor, -1 = not checked */
} vector;

static const vector VECTORS[] = {
    /* cursor movement and control characters */
    {"plain text", "abc", "abc", 4, 10, 0, 3},
    {"crlf", "abc\r\ndef", "abc|def", 4, 10, 1, 3},
    {"CUP", "\033[2;3Hx", "|  x", 4, 10, 1, 3},
    {"HVP", "\033[2;3fx", "|  x", 4, 10, 1, 3},
    {"CUP defaults", "abc\033[Hx", "xbc", 4, 10, 0, 1},
    {"CUU CUD CUF CUB", "\033[3;5H\033[2Aa\033[1Bb\033[2Cc\033[3Dd",
     "    a|     bd c", 4, 10, -1, -1},
    {"CUP clamps", "\033[99;99Hx", "|||         x", 4, 10, 3, 9},
    {"CHA VPA", "\033[5G x\033[3dy", "     x||      y", 4, 10, -1, -1},
    {"CNL CPL", "ab\033[2Ecd\033[1Fe", "ab|e|cd", 4, 10, -1, -1},
    {"DECSC DECRC", "ab\0337\033[3;1Hxx\0338c", "abc||xx", 4, 10, 0, 3},
    {"CSI s u", "ab\033[s\033[2;1H\033[uZ", "abZ", 4, 10, 0, 3},
    {"backspace", "abc\bX", "abX", 4, 10, 0, 3},
    {"tab", "a\tb", "a       b", 4, 10, 0, 9},
    {"tab stops default", "\t\tx", "                x", 4, 20, 0, 17},
    {"HTS", "\033[3g\033[1;4H\033H\r\tx", "   x", 4, 10, 0, 4},
    {"TBC all", "\033[3g\tx", "         x", 4, 10, 0, 9},
    {"IND scrolls", "a\r\nb\r\nc\033D", "b|c", 3, 10, 2, 1},
    {"NEL", "a\033Eb", "a|b", 3, 10, 1, 1},
    {"RI scrolls down", "a\r\nb\033[1;1H\033M", "|a|b", 3, 10, 0, 0},
    {"LF keeps column", "ab\nc", "ab|  c", 3, 10, 1, 3},
    /* erase */
    {"ED 0", "abc\r\ndef\033[1;2H\033[J", "a", 4, 10, -1, -1},
    {"ED 1", "abc\r\ndef\033[2;2H\033[1J", "|  f", 4, 10, -1, -1},
    {"ED 2", "abc\r\ndef\033[2J", "", 4, 10, -1, -1},
    {"EL 0", "abcdef\033[1;4H\033[K", "abc", 4, 10, -1, -1},
    {"EL 1", "abcdef\033[1;3H\033[1K", "   def", 4, 10, -1, -1},
    {"EL 2", "abc\033[2K", "", 4, 10, -1, -1},
    {"ECH", "abcdef\033[1;2H\033[3X", "a   ef", 4, 10, 0, 1},
    /* insert and delete */
    {"ICH", "abcdef\033[1;3H\033[2@", "ab  cdef", 4, 10, -1, -1},
    {"DCH", "abcdef\033[1;2H\033[2P", "adef", 4, 10, -1, -1},
    {"IL", "a\r\nb\r\nc\033[2;1H\033[L", "a||b|c", 4, 10, 1, 0},
    {"DL", "a\r\nb\r\nc\033[2;1H\033[M", "a|c", 4, 10, 1, 0},
    {"SU", "a\r\nb\r\nc\033[1S", "b|c", 3, 10, -1, -1},
    {"SD", "a\r\nb\r\nc\033[1T", "|a|b", 3, 10, -1, -1},
    /* scroll regions */
    {"DECSTBM scrolls inside", "T\033[2;4r\033[2;1Ha\r\nb\r\nc\r\nd\033[5;1He",
     "T|b|c|d|e", 5, 10, -1, -1},
    {"DECSTBM homes", "\033[3;3H\033[2;4rx", "x", 5, 10, 0, 1},
    {"CUU stops at the margin", "\033[2;3r\033[3;1H\033[5Ax", "|x", 4, 10, 1,
     1},
    {"origin mode", "\033[2;3r\033[?6h\033[1;1Hx", "|x", 4, 10, 1, 1},
    {"IL outside region", "\033[2;3r\033[1;1H\033[Lx", "x", 4, 10, 0, 1},
    /* wrapping */
    {"deferred wrap", "abcde", "abcde", 3, 5, 0, 4},
    {"wrap on next char", "abcdef", "abcde|f", 3, 5, 1, 1},
    {"deferred wrap CR", "abcde\rX", "Xbcde", 3, 5, 0, 1},
    {"autowrap off", "\033[?7labcdefg", "abcdg", 3, 5, 0, 4},
    {"autowrap scrolls", "a\r\nb\r\nabcdef", "b|abcde|f", 3, 5, 2, 1},
    /* UTF-8 and wide characters */
    {"utf8 2 bytes", "caf\xc3\xa9", "caf\xc3\xa9", 3, 10, 0, 4},
    {"utf8 3 bytes", "\xe2\x82\xac", "\xe2\x82\xac", 3, 10, 0, 1},
    {"utf8 4 bytes", "\xf0\x9f\x98\x80", "\xf0\x9f\x98\x80", 3, 10, 0, 2},
    {"invalid byte",
     "a\xff"
     "b",
     "a\xef\xbf\xbd"
     "b",
     3, 10, 0, 3},
    {"stray continuation", "\x80\xbf", "\xef\xbf\xbd\xef\xbf\xbd", 3, 10, 0, 2},
    {"truncated then ascii", "\xc3(", "\xef\xbf\xbd(", 3, 10, 0, 2},
    {"overlong", "\xc0\x80", "\xef\xbf\xbd\xef\xbf\xbd", 3, 10, 0, 2},
    {"overlong 3 bytes", "\xe0\x80\x80", "\xef\xbf\xbd", 3, 10, 0, 1},
    {"surrogate", "\xed\xa0\x80", "\xef\xbf\xbd", 3, 10, 0, 1},
    {"above U+10FFFF", "\xf4\x90\x80\x80", "\xef\xbf\xbd", 3, 10, 0, 1},
    {"truncated before ESC", "\xe2\x82\033[mx", "\xef\xbf\xbdx", 3, 10, 0, 2},
    {"wide", "\xe6\x97\xa5\xe6\x9c\xac", "\xe6\x97\xa5\xe6\x9c\xac", 3, 10, 0,
     4},
    {"wide at the edge wraps", "abcd\xe6\x97\xa5", "abcd|\xe6\x97\xa5", 3, 5, 1,
     2},
    {"overwrite half a wide", "\xe6\x97\xa5\033[1;1Hx", "x", 3, 10, 0, 1},
    {"overwrite right half", "\xe6\x97\xa5\033[1;2Hx", " x", 3, 10, 0, 2},
    {"combining mark", "e\xcc\x81x", "e\xcc\x81x", 3, 10, 0, 2},
    {"C1 code point ignored",
     "a\xc2\x85"
     "b",
     "ab", 3, 10, 0, 2},
    /* charsets */
    {"DEC graphics", "\033(0q\033(Bq", "\xe2\x94\x80q", 3, 10, 0, 2},
    {"SO SI", "\033)0\016q\017q", "\xe2\x94\x80q", 3, 10, 0, 2},
    /* alternate screen */
    {"alt screen 1049", "main\033[?1049h\033[Halt", "alt", 3, 10, -1, -1},
    {"alt screen restores", "main\033[?1049h\033[Halt\033[?1049l", "main", 3,
     10, 0, 4},
    {"alt screen 47", "main\033[?47hx\033[?47l", "main", 3, 10, -1, -1},
    {"alt screen 1047", "main\033[?1047hx\033[?1047l", "main", 3, 10, -1, -1},
    /* ignored strings and unknown sequences */
    {"DCS ignored", "a\033Pq#0;2;0;0;0\033\\b", "ab", 3, 10, 0, 2},
    {"APC ignored", "a\033_xyz\033\\b", "ab", 3, 10, 0, 2},
    {"OSC ignored", "a\033]8;;http://x\007b", "ab", 3, 10, 0, 2},
    {"unknown CSI", "a\033[?9999zb\033[>1;2;3xc", "abc", 3, 10, 0, 3},
    {"CAN aborts", "a\033[1\030b", "ab", 3, 10, 0, 2},
    {"controls inside CSI", "\033[1\n;3Hx", "  x", 3, 10, -1, -1},
    {"DECALN", "\033#8", "EEEEE|EEEEE", 2, 5, -1, -1},
    {"RIS", "abc\033[2;1Hdef\033c", "", 3, 10, 0, 0},
};

static void test_vectors(void)
{
	for (size_t i = 0; i < sizeof VECTORS / sizeof VECTORS[0]; i++) {
		const vector *v = &VECTORS[i];
		ctui_vt *vt = ctui_vt_new(v->rows, v->cols, 10);
		CHECK(vt != NULL);
		if (!vt)
			continue;
		FEED(vt, v->input);
		char got[1024];
		screen_str(vt, got, sizeof got);
		if (strcmp(got, v->want) != 0)
			fprintf(
			    stderr,
			    "vector \"%s\":\n  got:  \"%s\"\n  want: \"%s\"\n",
			    v->name, got, v->want);
		CHECK(strcmp(got, v->want) == 0);
		if (v->cy >= 0) {
			if (ctui_vt_cursor_row(vt) != v->cy ||
			    ctui_vt_cursor_col(vt) != v->cx)
				fprintf(
				    stderr,
				    "vector \"%s\": cursor %d,%d want %d,%d\n",
				    v->name, ctui_vt_cursor_row(vt),
				    ctui_vt_cursor_col(vt), v->cy, v->cx);
			CHECK(ctui_vt_cursor_row(vt) == v->cy &&
			      ctui_vt_cursor_col(vt) == v->cx);
		}
		ctui_vt_free(vt);
	}
}

static ctui_vt *make(const char *input)
{
	ctui_vt *vt = ctui_vt_new(5, 20, 10);
	if (input)
		FEED(vt, input);
	return vt;
}

static void test_sgr(void)
{
	ctui_vt *vt = make(
	    "\033[1;31mA\033[0mB\033[3;4;7;2;8mC\033[22;23;24;27;28mD"
	    "\033[38;5;200;48;2;1;2;3mE\033[38:2::10:20:30mF\033[38:2:4:5:6mG"
	    "\033[39;49mH\033[92;104mI\033[48:5:99mJ\033[5;9mK\033[4:3mL\033[4:"
	    "0mM");
	const ctui_vt_cell *c = ctui_vt_cell_at(vt, 0, 0);
	CHECK(c->cp == 'A' && c->fg == CTUI_VT_INDEXED(1) && c->bg == 0 &&
	      c->attrs == CTUI_VT_BOLD);
	c = ctui_vt_cell_at(vt, 0, 1);
	CHECK(c->cp == 'B' && c->fg == 0 && c->attrs == 0);
	c = ctui_vt_cell_at(vt, 0, 2);
	CHECK(c->attrs == (CTUI_VT_ITALIC | CTUI_VT_UNDERLINE |
			   CTUI_VT_REVERSE | CTUI_VT_DIM | CTUI_VT_HIDDEN));
	c = ctui_vt_cell_at(vt, 0, 3);
	CHECK(c->cp == 'D' && c->attrs == 0);
	c = ctui_vt_cell_at(vt, 0, 4);
	CHECK(c->fg == CTUI_VT_INDEXED(200) && c->bg == CTUI_VT_RGB(1, 2, 3));
	CHECK(CTUI_VT_COLOR_KIND(c->bg) == 2 && CTUI_VT_COLOR_KIND(c->fg) == 1);
	c = ctui_vt_cell_at(vt, 0, 5);
	CHECK(c->fg == CTUI_VT_RGB(10, 20, 30));
	c = ctui_vt_cell_at(vt, 0, 6);
	CHECK(c->fg == CTUI_VT_RGB(4, 5, 6));
	c = ctui_vt_cell_at(vt, 0, 7);
	CHECK(c->fg == 0 && c->bg == 0);
	c = ctui_vt_cell_at(vt, 0, 8);
	CHECK(c->fg == CTUI_VT_INDEXED(10) && c->bg == CTUI_VT_INDEXED(12));
	c = ctui_vt_cell_at(vt, 0, 9);
	CHECK(c->bg == CTUI_VT_INDEXED(99));
	c = ctui_vt_cell_at(vt, 0, 10);
	CHECK(c->cp == 'K'); /* blink and strike are ignored */
	c = ctui_vt_cell_at(vt, 0, 11);
	CHECK(c->attrs & CTUI_VT_UNDERLINE);
	c = ctui_vt_cell_at(vt, 0, 12);
	CHECK(!(c->attrs & CTUI_VT_UNDERLINE));
	FEED(vt, "\033[m\033[44mx\033[K");
	CHECK(ctui_vt_cell_at(vt, 0, 14)->bg ==
	      CTUI_VT_INDEXED(4)); /* erase keeps the background */
	ctui_vt_free(vt);
	/* malformed colours drop the rest, never crash */
	vt = make("\033[38;5mA\033[38;2;1;2mB\033[48mC\033[38;5;999mD");
	CHECK(ctui_vt_cell_at(vt, 0, 3)->cp == 'D');
	ctui_vt_free(vt);
}

static void test_modes(void)
{
	ctui_vt *vt = make(NULL);
	const ctui_vt_modes *m = ctui_vt_get_modes(vt);
	CHECK(m->autowrap && m->cursor_visible && !m->app_cursor &&
	      m->mouse == 0 && !m->alt_screen);
	FEED(vt, "\033[?1h\033=\033[?25l\033[?2004h\033[?1000h\033[?1006h\033[?"
		 "1004h\033[4h\033[20h");
	CHECK(m->app_cursor && m->app_keypad && !m->cursor_visible &&
	      m->bracketed_paste);
	CHECK(m->mouse == 1000 && m->mouse_sgr && m->focus_events &&
	      m->insert && m->newline);
	FEED(vt, "\033[?1002h");
	CHECK(m->mouse == 1002);
	FEED(vt, "\033[?1003h");
	CHECK(m->mouse == 1003);
	FEED(vt, "\033[?1002l");
	CHECK(m->mouse == 1003); /* a different mode is not switched off */
	FEED(vt, "\033[?1003l\033[?1l\033>\033[?25h\033[?2004l\033[?1006l\033["
		 "4l\033[20l");
	CHECK(m->mouse == 0 && !m->app_cursor && !m->app_keypad &&
	      m->cursor_visible && !m->bracketed_paste);
	CHECK(!m->mouse_sgr && !m->insert && !m->newline);
	FEED(vt, "\033[?1049h");
	CHECK(m->alt_screen);
	FEED(vt, "\033[?1049l");
	CHECK(!m->alt_screen);
	ctui_vt_free(vt);
	/* insert mode and LNM at work */
	vt = make("abc\033[1;1H\033[4hX\033[4l");
	char row[64];
	ctui_vt_row_text(vt, 0, row, sizeof row);
	CHECK_STR(row, "Xabc");
	ctui_vt_free(vt);
	vt = make("\033[20ha\nb");
	ctui_vt_row_text(vt, 1, row, sizeof row);
	CHECK_STR(row, "b");
	ctui_vt_free(vt);
}

static void test_title(void)
{
	ctui_vt *vt = make("\033]0;hello\007");
	CHECK_STR(ctui_vt_title(vt), "hello");
	FEED(vt, "\033]2;ab\033\\");
	CHECK_STR(ctui_vt_title(vt), "ab");
	FEED(vt, "\033]2;x\001y\007");
	CHECK_STR(ctui_vt_title(vt), "xy");
	FEED(vt, "\033]1;icon\007");
	CHECK_STR(ctui_vt_title(vt), "xy");
	FEED(vt, "\033]2;caf\xc3\xa9\007");
	CHECK_STR(ctui_vt_title(vt), "caf\xc3\xa9");
	FEED(vt, "\033]2;\xff\007");
	CHECK_STR(ctui_vt_title(vt), "?");
	/* an OSC cut by another escape sequence is dropped */
	FEED(vt, "\033]2;lost\033[mz");
	CHECK_STR(ctui_vt_title(vt), "?");
	char row[32];
	ctui_vt_row_text(vt, 0, row, sizeof row);
	CHECK_STR(row, "z");
	ctui_vt_free(vt);
}

static void test_responses(void)
{
	ctui_vt *vt = make("ab");
	char buf[64];
	FEED(vt, "\033[5n");
	size_t n = ctui_vt_take_response(vt, buf, sizeof buf);
	buf[n] = '\0';
	CHECK_STR(buf, "\033[0n");
	FEED(vt, "\033[6n");
	n = ctui_vt_take_response(vt, buf, sizeof buf);
	buf[n] = '\0';
	CHECK_STR(buf, "\033[1;3R");
	FEED(vt, "\033[c");
	n = ctui_vt_take_response(vt, buf, sizeof buf);
	buf[n] = '\0';
	CHECK_STR(buf, "\033[?1;2c");
	FEED(vt, "\033[>c\033[?6n");
	n = ctui_vt_take_response(vt, buf, sizeof buf);
	buf[n] = '\0';
	CHECK_STR(buf, "\033[>0;0;0c\033[?1;3R");
	CHECK(ctui_vt_take_response(vt, buf, sizeof buf) == 0);
	/* partial reads keep the rest */
	FEED(vt, "\033[5n");
	CHECK(ctui_vt_take_response(vt, buf, 2) == 2 && buf[0] == '\033' &&
	      buf[1] == '[');
	CHECK(ctui_vt_take_response(vt, buf, 8) == 2);
	/* origin mode reports relative to the region */
	FEED(vt, "\033[3;5r\033[?6h\033[2;2H\033[6n");
	n = ctui_vt_take_response(vt, buf, sizeof buf);
	buf[n] = '\0';
	CHECK_STR(buf, "\033[2;2R");
	/* the buffer is bounded */
	for (int i = 0; i < 1000; i++)
		FEED(vt, "\033[6n");
	size_t total = 0;
	while ((n = ctui_vt_take_response(vt, buf, sizeof buf)) > 0)
		total += n;
	CHECK(total > 0 && total <= CTUI_VT_RESPONSE_MAX);
	ctui_vt_free(vt);
}

static void test_resize(void)
{
	ctui_vt *vt = ctui_vt_new(4, 10, 5);
	FEED(vt, "abc\r\ndef");
	CHECK(ctui_vt_resize(vt, 2, 5));
	char got[64];
	screen_str(vt, got, sizeof got);
	CHECK_STR(got, "abc|def");
	CHECK(ctui_vt_rows(vt) == 2 && ctui_vt_cols(vt) == 5);
	CHECK(ctui_vt_resize(vt, 6, 12));
	screen_str(vt, got, sizeof got);
	CHECK_STR(got, "abc|def");
	CHECK(ctui_vt_cursor_row(vt) == 1 && ctui_vt_cursor_col(vt) == 3);
	FEED(vt, "\r\n\r\n\r\n\r\nz");
	CHECK(ctui_vt_cursor_row(vt) == 5);
	CHECK(ctui_vt_resize(
	    vt, 3,
	    12)); /* the cursor would fall off: the top goes to history */
	CHECK(ctui_vt_cursor_row(vt) == 2);
	CHECK(ctui_vt_history_count(vt) == 3);
	ctui_vt_resize(vt, 3, 40);
	FEED(vt, "\033[1;40Hq");
	char row[64];
	ctui_vt_row_text(vt, 0, row, sizeof row);
	CHECK(strlen(row) == 40 && row[39] == 'q');
	CHECK(!ctui_vt_resize(vt, 0, 5) &&
	      !ctui_vt_resize(vt, 5, CTUI_VT_MAX_COLS + 1));
	CHECK(ctui_vt_cols(vt) == 40);
	ctui_vt_free(vt);
	/* a wide character cut by a narrower screen leaves no half */
	vt = ctui_vt_new(2, 6, 0);
	FEED(vt, "abcd\xe6\x97\xa5");
	CHECK(ctui_vt_resize(vt, 2, 5));
	CHECK(ctui_vt_cell_at(vt, 0, 4)->width == 1);
	ctui_vt_free(vt);
	/* alternate screen: truncation, no history */
	vt = ctui_vt_new(3, 5, 5);
	FEED(vt, "\033[?1049h\033[3;1Hx");
	ctui_vt_resize(vt, 2, 5);
	CHECK(ctui_vt_history_count(vt) == 0);
	ctui_vt_free(vt);
}

static void test_scrollback(void)
{
	ctui_vt *vt = ctui_vt_new(2, 5, 3);
	FEED(vt, "1\r\n2\r\n3\r\n4\r\n5\r\n6");
	char got[64];
	screen_str(vt, got, sizeof got);
	CHECK_STR(got, "5|6");
	CHECK(ctui_vt_history_count(vt) == 3 &&
	      ctui_vt_history_pushed(vt) == 4);
	ctui_vt_history_text(vt, 0, got, sizeof got);
	CHECK_STR(got, "4");
	ctui_vt_history_text(vt, 2, got, sizeof got);
	CHECK_STR(got, "2");
	CHECK(ctui_vt_history_cell(vt, 0, 0)->cp == '4');
	CHECK(ctui_vt_history_cell(vt, 0, 3)->cp ==
	      0); /* past the stored line: blank */
	CHECK(ctui_vt_history_cell(vt, 3, 0) == NULL &&
	      ctui_vt_history_cell(vt, 0, 5) == NULL);
	/* the alternate screen does not feed the history */
	FEED(vt, "\033[?1049h1\r\n2\r\n3\r\n4\033[?1049l");
	CHECK(ctui_vt_history_count(vt) == 3 &&
	      ctui_vt_history_pushed(vt) == 4);
	/* a partial scroll region does not feed it; ED 3 clears it */
	ctui_vt_free(vt);
	vt = ctui_vt_new(3, 5, 3);
	FEED(vt, "\033[2;3r\033[3;1H\n\n");
	CHECK(ctui_vt_history_pushed(vt) == 0);
	FEED(vt, "\033[r\033[3;1H\n\033[3J");
	CHECK(ctui_vt_history_count(vt) == 0);
	ctui_vt_free(vt);
	/* coloured blanks survive in the history */
	vt = ctui_vt_new(2, 5, 3);
	FEED(vt, "\033[41m  \033[0mx\r\n\r\n");
	CHECK(ctui_vt_history_count(vt) == 1);
	CHECK(ctui_vt_history_cell(vt, 0, 0)->bg == CTUI_VT_INDEXED(1));
	ctui_vt_free(vt);
	/* no scrollback at all */
	vt = ctui_vt_new(2, 5, 0);
	FEED(vt, "1\r\n2\r\n3\r\n4");
	CHECK(ctui_vt_history_count(vt) == 0);
	ctui_vt_free(vt);
}

static void test_limits(void)
{
	CHECK(ctui_vt_new(0, 10, 0) == NULL && ctui_vt_new(10, 0, 0) == NULL);
	CHECK(ctui_vt_new(CTUI_VT_MAX_ROWS + 1, 10, 0) == NULL &&
	      ctui_vt_new(10, CTUI_VT_MAX_COLS + 1, 0) == NULL);
	ctui_vt *vt = ctui_vt_new(CTUI_VT_MAX_ROWS, CTUI_VT_MAX_COLS, 1000000);
	CHECK(vt != NULL);
	ctui_vt_free(vt);
	vt = make(NULL);
	FEED(vt, "\033[99999999999999999999999A\033[99999999999999999999B\033["
		 "999999999@\033[999999999P"
		 "\033[999999999L\033[999999999M\033[999999999S\033["
		 "999999999T\033[999999999X");
	CHECK(ctui_vt_cursor_row(vt) >= 0 && ctui_vt_cursor_row(vt) < 5);
	/* 100 parameters, and 40 colon sub-parameters */
	char big[1024] = "\033[";
	size_t used = strlen(big);
	for (int i = 0; i < 100; i++)
		used += (size_t)snprintf(big + used, sizeof big - used, "1;");
	snprintf(big + used, sizeof big - used, "mx");
	FEED(vt, big);
	used = (size_t)snprintf(big, sizeof big, "\033[38");
	for (int i = 0; i < 40; i++)
		used += (size_t)snprintf(big + used, sizeof big - used, ":5");
	snprintf(big + used, sizeof big - used, "my");
	FEED(vt, big);
	/* a long OSC and a long DCS */
	char *osc = malloc(70000);
	memset(osc, 'a', 70000);
	// NOLINTNEXTLINE(bugprone-not-null-terminated-result)
	memcpy(osc, "\033]2;", 4);
	osc[69999] = '\007';
	ctui_vt_feed(vt, osc, 70000);
	CHECK(strlen(ctui_vt_title(vt)) <= CTUI_VT_TITLE_MAX);
	// NOLINTNEXTLINE(bugprone-not-null-terminated-result)
	memcpy(osc, "\033Pxxx", 5);
	osc[69999] = 'b';
	ctui_vt_feed(vt, osc, 70000);
	free(osc);
	FEED(vt, "\033\\z");
	char row[64];
	ctui_vt_row_text(vt, ctui_vt_cursor_row(vt), row, sizeof row);
	CHECK(strchr(row, 'z') != NULL);
	ctui_vt_free(vt);
}

static uint32_t g_rng = 12345;
static uint32_t rnd(void)
{
	g_rng ^= g_rng << 13U;
	g_rng ^= g_rng >> 17U;
	g_rng ^= g_rng << 5U;
	return g_rng;
}

static bool consistent(const ctui_vt *vt)
{
	int rows = ctui_vt_rows(vt), cols = ctui_vt_cols(vt);
	if (ctui_vt_cursor_row(vt) < 0 || ctui_vt_cursor_row(vt) >= rows ||
	    ctui_vt_cursor_col(vt) < 0 || ctui_vt_cursor_col(vt) >= cols)
		return false;
	for (int r = 0; r < rows; r++)
		for (int c = 0; c < cols; c++) {
			const ctui_vt_cell *p = ctui_vt_cell_at(vt, r, c);
			if (p->width > 2)
				return false;
			if (p->width == 2 &&
			    (c + 1 >= cols ||
			     ctui_vt_cell_at(vt, r, c + 1)->width != 0))
				return false;
			if (p->width == 0 &&
			    (c == 0 ||
			     ctui_vt_cell_at(vt, r, c - 1)->width != 2))
				return false;
		}
	return true;
}

static const char *const PIECES[] = {
    "\033[",	   "\033]",	  "\033P",
    "\033",	   "\033\\",	  "\007",
    ";",	   ":",		  "?",
    "0",	   "1",		  "5",
    "99999",	   "m",		  "H",
    "J",	   "K",		  "r",
    "h",	   "l",		  "@",
    "P",	   "L",		  "M",
    "S",	   "T",		  "X",
    "n",	   "c",		  "\r\n",
    "\t",	   "\b",	  "\xe6\x97\xa5",
    "e\xcc\x81",   "\xff",	  "\xc3",
    "\033[?1049h", "\033[?1049l", "\033[38;2;1;2;3m",
    "\033(0",	   "\016",	  "\033[4:3m"};

static void test_fuzz(void)
{
	char buf[512], resp[64];
	bool ok = true;
	for (int round = 0; round < 3000 && ok; round++) {
		int rows = 1 + (int)(rnd() % 12), cols = 1 + (int)(rnd() % 30);
		ctui_vt *vt = ctui_vt_new(rows, cols, rnd() % 8);
		for (int chunk = 0; chunk < 6 && ok; chunk++) {
			size_t n = 0;
			if (round % 2) { /* pure random bytes */
				n = rnd() % sizeof buf;
				for (size_t i = 0; i < n; i++)
					buf[i] = (char)rnd();
			} else { /* random sequence pieces */
				int pieces = (int)(rnd() % 40);
				for (int i = 0;
				     i < pieces && n + 16 < sizeof buf; i++) {
					const char *p =
					    PIECES[rnd() % (sizeof PIECES /
							    sizeof PIECES[0])];
					size_t len = strlen(p);
					// NOLINTNEXTLINE(bugprone-not-null-terminated-result)
					memcpy(buf + n, p, len);
					n += len;
				}
			}
			ctui_vt_feed(vt, buf, n);
			ok = consistent(vt);
			CHECK(ok);
			while (ctui_vt_take_response(vt, resp, sizeof resp) > 0)
				;
			if (rnd() % 4 == 0)
				ctui_vt_resize(vt, 1 + (int)(rnd() % 12),
					       1 + (int)(rnd() % 30));
			char row[256];
			ctui_vt_row_text(
			    vt, (int)(rnd() % (unsigned)ctui_vt_rows(vt)), row,
			    sizeof row);
			if (ctui_vt_history_count(vt))
				ctui_vt_history_text(vt, 0, row, 3);
		}
		ctui_vt_free(vt);
	}
}

static void test_truncated(void)
{
	static const char *const SEQS[] = {
	    "\033[1;31;48;2;1;2;3m\033[38:2::1:2:3m",
	    "\033]2;title\033\\\033]0;t\007", "\033P1$r0m\033\\",
	    "\033[?1049h\033[2J\033[3;5r\033[6n",
	    "\033(0\xe6\x97\xa5\xf0\x9f\x98\200e\xcc\x81"};
	for (size_t s = 0; s < sizeof SEQS / sizeof SEQS[0]; s++) {
		size_t len = strlen(SEQS[s]);
		for (size_t cut = 0; cut <= len; cut++) {
			ctui_vt *vt = ctui_vt_new(3, 8, 4);
			ctui_vt_feed(vt, SEQS[s], cut);
			CHECK(consistent(vt));
			FEED(vt, "\033\\ok"); /* the next bytes recover */
			ctui_vt_free(vt);
		}
	}
	/* fed one byte at a time gives the same screen as in one go */
	const char *text = "\033[31mred\033[0m \xe6\x97\xa5\xe6\x9c\xac "
			   "\033[2;3Hx\033]2;t\007";
	ctui_vt *a = ctui_vt_new(4, 12, 2), *b = ctui_vt_new(4, 12, 2);
	FEED(a, text);
	for (size_t i = 0; text[i]; i++)
		ctui_vt_feed(b, text + i, 1);
	char sa[256], sb[256];
	screen_str(a, sa, sizeof sa);
	screen_str(b, sb, sizeof sb);
	CHECK_STR(sa, sb);
	unsigned long g = ctui_vt_generation(b);
	ctui_vt_feed(b, "", 0);
	CHECK(ctui_vt_generation(b) == g);
	ctui_vt_feed(b, "x", 1);
	CHECK(ctui_vt_generation(b) != g);
	ctui_vt_free(a);
	ctui_vt_free(b);
}

int main(void)
{
	RUN(test_vectors);
	RUN(test_sgr);
	RUN(test_modes);
	RUN(test_title);
	RUN(test_responses);
	RUN(test_resize);
	RUN(test_scrollback);
	RUN(test_limits);
	RUN(test_fuzz);
	RUN(test_truncated);
	return CHECK_DONE();
}
