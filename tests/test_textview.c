/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */
#include "check.h"

#include <curses.h>

#include "screen.h"
#include <ctui/textview.h>

static void test_lines(void)
{
	ctui_textview v;
	CHECK(ctui_textview_init(&v, "one\ntwo\n\nfour"));
	CHECK(v.line_count == 4);
	size_t len;
	CHECK(strncmp(ctui_textview_line(&v, 1, &len), "two", 3) == 0 &&
	      len == 3);
	ctui_textview_line(&v, 2, &len);
	CHECK(len == 0);
	ctui_textview_line(&v, 3, &len);
	CHECK(len == 4);
	ctui_textview_free(&v);
	CHECK(ctui_textview_init(&v, "a\n"));
	CHECK(v.line_count == 1);
	ctui_textview_free(&v);
	CHECK(ctui_textview_init(&v, NULL));
	CHECK(v.line_count == 1);
	ctui_textview_line(&v, 0, &len);
	CHECK(len == 0);
	ctui_textview_free(&v);
}

static void test_columns(void)
{
	const char *line = "a\tb\xc3\xa9\xe6\x97\xa5z";
	size_t len = strlen(line); /* a \t b é 日 z */
	CHECK(ctui_line_column(line, len, 0) == 0);
	CHECK(ctui_line_column(line, len, 1) == 1);
	CHECK(ctui_line_column(line, len, 2) == 4); /* tab to the next stop */
	CHECK(ctui_line_column(line, len, 3) == 5);
	CHECK(ctui_line_column(line, len, 5) == 6);
	CHECK(ctui_line_column(line, len, 8) == 8); /* past the wide char */
	CHECK(ctui_line_byte(line, len, 0) == 0);
	CHECK(ctui_line_byte(line, len, 3) == 1); /* inside the tab */
	CHECK(ctui_line_byte(line, len, 4) == 2);
	CHECK(ctui_line_byte(line, len, 7) ==
	      5); /* second cell of the wide char */
	CHECK(ctui_line_byte(line, len, 99) == len);
}

static void test_gutter(void)
{
	ctui_textview v;
	ctui_textview_init(&v, "a");
	CHECK(ctui_textview_gutter(&v) == 3);
	ctui_textview_free(&v);
	char big[2048];
	size_t n = 0;
	for (int i = 0; i < 120; i++)
		n += (size_t)snprintf(big + n, sizeof big - n, "x\n");
	ctui_textview_init(&v, big);
	CHECK(v.line_count == 120 && ctui_textview_gutter(&v) == 5);
	ctui_textview_free(&v);
}

static void test_cursor_moves(void)
{
	ctui_textview v;
	ctui_textview_init(&v, "abc\n\xc3\xa9t\nz");
	CHECK(ctui_textview_key(&v, KEY_RIGHT, 5) && v.byte == 1);
	ctui_textview_key(&v, KEY_RIGHT, 5);
	ctui_textview_key(&v, KEY_RIGHT, 5);
	CHECK(v.line == 0 && v.byte == 3);
	ctui_textview_key(&v, KEY_RIGHT, 5); /* wraps to the next line */
	CHECK(v.line == 1 && v.byte == 0);
	ctui_textview_key(&v, KEY_RIGHT, 5); /* over the 2-byte é */
	CHECK(v.byte == 2);
	ctui_textview_key(&v, KEY_LEFT, 5);
	CHECK(v.byte == 0);
	ctui_textview_key(&v, KEY_LEFT, 5); /* back to the end of line 0 */
	CHECK(v.line == 0 && v.byte == 3);
	CHECK(!ctui_textview_key(&v, KEY_UP, 5));
	CHECK(ctui_textview_key(&v, KEY_DOWN, 5) && v.line == 1 &&
	      v.byte == 3); /* column 3 -> end */
	ctui_textview_key(&v, KEY_DOWN, 5);
	CHECK(v.line == 2 && v.byte == 1);
	CHECK(!ctui_textview_key(&v, KEY_DOWN, 5));
	ctui_textview_key(&v, KEY_HOME, 5);
	CHECK(v.byte == 0);
	ctui_textview_key(&v, KEY_PPAGE, 5);
	CHECK(v.line == 0);
	ctui_textview_key(&v, KEY_END, 5);
	CHECK(v.byte == 3);
	ctui_textview_key(&v, KEY_NPAGE, 5);
	CHECK(v.line == 2);
	CHECK(!ctui_textview_key(&v, 'x', 5));
	ctui_textview_free(&v);
}

static void test_goal_column(void)
{
	ctui_textview v;
	ctui_textview_init(&v, "abcdef\nab\nabcdef");
	ctui_textview_key(&v, KEY_END, 5);
	ctui_textview_key(&v, KEY_DOWN, 5);
	CHECK(v.line == 1 && v.byte == 2);
	ctui_textview_key(&v, KEY_DOWN, 5);
	CHECK(v.line == 2 &&
	      v.byte == 6); /* the goal column survived the short line */
	ctui_textview_free(&v);
}

static void test_selection(void)
{
	ctui_textview v;
	ctui_textview_init(&v, "abc\ndef");
	CHECK(!ctui_textview_selected(&v, 0, 0));
	ctui_textview_key(&v, KEY_SRIGHT, 5);
	ctui_textview_key(&v, KEY_SRIGHT, 5);
	CHECK(v.selecting && ctui_textview_selected(&v, 0, 0) &&
	      ctui_textview_selected(&v, 0, 1));
	CHECK(!ctui_textview_selected(&v, 0, 2));
	ctui_textview_key(&v, KEY_SF, 5);
	CHECK(ctui_textview_selected(&v, 0, 2) &&
	      ctui_textview_selected(&v, 1, 0));
	CHECK(!ctui_textview_selected(&v, 1, 2));
	CHECK(ctui_textview_key(&v, 27, 5) && !v.selecting);
	CHECK(!ctui_textview_key(&v, 27, 5));
	/* backwards selection is normalised */
	ctui_textview_key(&v, KEY_END, 5);
	ctui_textview_key(&v, KEY_SLEFT, 5);
	CHECK(ctui_textview_selected(&v, 1, 2) &&
	      !ctui_textview_selected(&v, 1, 3));
	/* a plain move drops the selection */
	ctui_textview_key(&v, KEY_LEFT, 5);
	CHECK(!v.selecting);
	CHECK(ctui_textview_key(&v, 1, 5));
	CHECK(ctui_textview_selected(&v, 0, 0) &&
	      ctui_textview_selected(&v, 1, 2));
	ctui_textview_free(&v);
}

static void test_reveal(void)
{
	ctui_textview v;
	ctui_textview_init(&v, "0\n1\n2\n3\n4\n5\n6\n7");
	v.line = 6;
	ctui_textview_reveal(&v, 3, 10);
	CHECK(v.top == 4);
	v.line = 1;
	ctui_textview_reveal(&v, 3, 10);
	CHECK(v.top == 1);
	ctui_textview_free(&v);
	ctui_textview_init(&v, "0123456789abcdef");
	v.byte = 12;
	ctui_textview_reveal(&v, 3, 5);
	CHECK(v.left == 8);
	v.byte = 2;
	ctui_textview_reveal(&v, 3, 5);
	CHECK(v.left == 2);
	ctui_textview_free(&v);
}

static void test_scroll_to(void)
{
	ctui_textview v;
	ctui_textview_init(&v, "0\n1\n2\n3\n4\n5\n6\n7");
	v.line = 6;
	ctui_textview_scroll_to(&v, 3, 0);
	CHECK(v.top == 0 && v.line == 2);
	ctui_textview_scroll_to(&v, 3, 99);
	CHECK(v.top == 5 && v.line == 5);
	ctui_textview_scroll_to(&v, 3, 4);
	CHECK(v.top == 4 && v.line == 5);
	ctui_textview_free(&v);
	/* the byte follows the shorter line */
	ctui_textview_init(&v, "abcdef\nx\ny\nz");
	v.line = 0;
	v.byte = 5;
	ctui_textview_scroll_to(&v, 2, 1);
	CHECK(v.line == 1 && v.byte == 1);
	ctui_textview_free(&v);
}

static void test_width(void)
{
	ctui_textview v;
	ctui_textview_init(&v, "ab\n\t\xe6\x97\xa5x\nabc");
	CHECK(v.width == 7); /* tab (4) + wide char (2) + x */
	ctui_textview_free(&v);
}

static void test_scroll_left(void)
{
	ctui_textview v;
	ctui_textview_init(&v, "0123456789abcdefghij\nxy");
	v.byte = 2;
	ctui_textview_scroll_left_to(&v, 5, 8);
	CHECK(v.left == 8 && v.byte == 8); /* the cursor is pulled into view */
	ctui_textview_scroll_left_to(&v, 5, 99);
	CHECK(v.left == 16 && v.byte == 16); /* 20 columns + the cursor cell */
	CHECK(ctui_textview_hscroll(&v, 5, -4) && v.left == 12 && v.byte == 16);
	CHECK(ctui_textview_hscroll(&v, 5, -99) && v.left == 0 && v.byte == 4);
	CHECK(!ctui_textview_hscroll(&v, 5, -1));
	CHECK(ctui_textview_hscroll(&v, 5, 3) && v.left == 3 && v.byte == 4);
	ctui_textview_free(&v);
	/* a wide character cut by the left edge is skipped */
	ctui_textview_init(&v, "a\xe6\x97\xa5\xe6\x97\xa5z");
	ctui_textview_scroll_left_to(&v, 3, 2);
	CHECK(v.left == 2 && v.byte == 4); /* column 3: the second wide char */
	ctui_textview_free(&v);
}

static void test_detached(void)
{
	screen_open(24, 80);
	ctui_textview v;
	ctui_textview_init(&v, "ab\n0123456789abcdefghij");
	v.hscroll = true;
	ctui_textview_draw(&v, 0, 0, 12, 4, true);
	/* line 0 is too short to follow the window: the view stays where it was
	 * put */
	CHECK(ctui_textview_hscroll(&v, 9, 8) && v.left == 8 && v.detached);
	ctui_textview_draw(&v, 0, 0, 12, 4, true);
	CHECK(v.left == 8 && v.line == 0 && v.byte == 2);
	/* moving the cursor brings the view back to it */
	CHECK(ctui_textview_key(&v, KEY_LEFT, 3) && !v.detached);
	ctui_textview_draw(&v, 0, 0, 12, 4, true);
	CHECK(v.left == 1);
	ctui_textview_free(&v);
}

static void test_hbar(void)
{
	screen_open(24, 80);
	ctui_textview v;
	char text[200];
	memset(text, 'x', sizeof text - 1);
	text[sizeof text - 1] = '\0';
	ctui_textview_init(&v, text);
	int gutter = ctui_textview_gutter(&v);
	screen_clear();
	ctui_textview_draw(&v, 0, 0, 40, 5, true);
	CHECK(!ctui_scrollbar_needed(&v.hbar)); /* off by default */
	v.hscroll = true;
	ctui_textview_draw(&v, 0, 0, 40, 5, true);
	CHECK(ctui_scrollbar_needed(&v.hbar));
	CHECK(v.hbar.y == 4 && v.hbar.x == gutter &&
	      v.hbar.height == 40 - gutter);
	CHECK(v.hbar.total == 200 && v.hbar.visible == (size_t)(40 - gutter));
	/* press on the track right of the thumb moves a page */
	CHECK(ctui_textview_hpress(&v, 4, 39));
	CHECK(v.left == (size_t)(40 - gutter) && v.byte >= v.left);
	CHECK(!ctui_textview_hpress(&v, 0, 10)); /* a text row is not the bar */
	/* drag the thumb to the end */
	ctui_textview_draw(&v, 0, 0, 40, 5, true);
	int start, length;
	ctui_scrollbar_thumb(v.hbar.total, v.hbar.visible, v.hbar.offset,
			     v.hbar.height, &start, &length);
	CHECK(ctui_textview_hpress(&v, 4, gutter + start));
	CHECK(ctui_textview_hdrag(&v, 39));
	CHECK(v.left == 200 - (size_t)(40 - gutter));
	CHECK(ctui_textview_hrelease(&v) && !ctui_textview_hrelease(&v));
	ctui_textview_draw(&v, 0, 0, 40, 5, true);
	CHECK(v.left ==
	      200 -
		  (size_t)(40 - gutter)); /* the cursor does not snap it back */
	CHECK(ctui_textview_hwheel(&v, -10) &&
	      v.left == 200 - (size_t)(40 - gutter) - 10);
	ctui_textview_free(&v);
	/* short lines: no bar, the wheel is not taken */
	ctui_textview_init(&v, "short");
	v.hscroll = true;
	ctui_textview_draw(&v, 0, 0, 40, 5, true);
	CHECK(!ctui_scrollbar_needed(&v.hbar) && !ctui_textview_hwheel(&v, 3));
	ctui_textview_free(&v);
}

int main(void)
{
	RUN(test_lines);
	RUN(test_columns);
	RUN(test_gutter);
	RUN(test_cursor_moves);
	RUN(test_goal_column);
	RUN(test_selection);
	RUN(test_reveal);
	RUN(test_scroll_to);
	RUN(test_width);
	RUN(test_scroll_left);
	RUN(test_detached);
	RUN(test_hbar);
	return CHECK_DONE();
}
