/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * test_input.c - single-line input: editing vectors, scrolling, mouse, drawing.
 */
#include "check.h"
#include "screen.h"

#include <curses.h>

#include <ctui/input.h>

#define E_ACUTE "\xc3\xa9"	 /* e acute, 2 bytes, 1 column */
#define EURO "\xe2\x82\xac"	 /* 3 bytes, 1 column */
#define WIDE_O "\xe3\x81\x8a"	 /* hiragana o, 3 bytes, 2 columns */
#define EMOJI "\xf0\x9f\x98\x80" /* 4 bytes, 2 columns */

static void test_init(void)
{
	char buf[16] = "h" E_ACUTE "llo";
	ctui_input in;
	ctui_input_init(&in, buf, sizeof buf);
	CHECK(in.cursor == 6 && in.scroll == 0 && !in.mask &&
	      in.max_chars == 0);
	CHECK(ctui_input_length(&in) == 5);
	ctui_input_init(&in, buf, 0);
	CHECK(!ctui_input_insert(&in, "a") &&
	      ctui_input_key(&in, 'a') == CTUI_INPUT_NONE);
	CHECK(!ctui_input_backspace(&in) && ctui_input_length(&in) == 0);
}

static void test_insert_utf8(void)
{
	char buf[32] = "";
	ctui_input in;
	ctui_input_init(&in, buf, sizeof buf);
	CHECK(ctui_input_insert(&in, "a" E_ACUTE WIDE_O EMOJI));
	CHECK_STR(buf, "a" E_ACUTE WIDE_O EMOJI);
	CHECK(in.cursor == 10 && ctui_input_length(&in) == 4);
	/* insert in the middle */
	ctui_input_left(&in);
	ctui_input_left(&in);
	CHECK(ctui_input_insert(&in, EURO));
	CHECK_STR(buf, "a" E_ACUTE EURO WIDE_O EMOJI);
	CHECK(in.cursor == 6);
}

static void test_insert_rejects(void)
{
	char buf[32] = "";
	ctui_input in;
	ctui_input_init(&in, buf, sizeof buf);
	CHECK(!ctui_input_insert(&in, "\x01"));		/* control */
	CHECK(!ctui_input_insert(&in, "\xc2\x85"));	/* C1 control */
	CHECK(!ctui_input_insert(&in, "\xff"));		/* invalid byte */
	CHECK(!ctui_input_insert(&in, "\xc3"));		/* truncated */
	CHECK(!ctui_input_insert(&in, "\xed\xa0\x80")); /* surrogate */
	CHECK(!ctui_input_insert(&in, NULL) && !ctui_input_insert(&in, ""));
	CHECK_STR(buf, "");
	CHECK(ctui_input_insert(&in, "ab\tc")); /* stops at the tab */
	CHECK_STR(buf, "ab");
	CHECK(
	    ctui_input_insert(&in, "\xef\xbf\xbd")); /* a real U+FFFD is fine */
}

static void test_capacity_and_max(void)
{
	char buf[5] = "";
	ctui_input in;
	ctui_input_init(&in, buf, sizeof buf);
	CHECK(ctui_input_insert(&in, "abcdef")); /* fits 4 bytes */
	CHECK_STR(buf, "abcd");
	CHECK(!ctui_input_insert(&in, "z"));
	ctui_input_set(&in,
		       "xy" E_ACUTE E_ACUTE); /* 6 bytes: cut on a boundary */
	CHECK_STR(buf, "xy" E_ACUTE);
	CHECK(in.cursor == 4);

	char big[32] = "";
	ctui_input_init(&in, big, sizeof big);
	in.max_chars = 3;
	CHECK(ctui_input_insert(&in, E_ACUTE WIDE_O "abc"));
	CHECK_STR(big, E_ACUTE WIDE_O "a");
	CHECK(ctui_input_key(&in, 'x') == CTUI_INPUT_NONE);
	CHECK(ctui_input_key(&in, KEY_BACKSPACE) == CTUI_INPUT_CHANGED);
	CHECK(ctui_input_key(&in, 'x') == CTUI_INPUT_CHANGED);
}

static void test_cursor_moves(void)
{
	char buf[32] = "a" E_ACUTE WIDE_O EMOJI "z";
	ctui_input in;
	ctui_input_init(&in, buf, sizeof buf);
	CHECK(in.cursor == 11);
	size_t want[] = {10, 6, 3, 1, 0};
	for (int i = 0; i < 5; i++) {
		CHECK(ctui_input_left(&in) == (i < 5));
		CHECK(in.cursor == want[i]);
	}
	CHECK(!ctui_input_left(&in) && in.cursor == 0);
	CHECK(ctui_input_key(&in, KEY_RIGHT) == CTUI_INPUT_MOVED &&
	      in.cursor == 1);
	CHECK(ctui_input_key(&in, KEY_END) == CTUI_INPUT_MOVED &&
	      in.cursor == 11);
	CHECK(ctui_input_key(&in, KEY_END) == CTUI_INPUT_NONE);
	CHECK(ctui_input_key(&in, KEY_HOME) == CTUI_INPUT_MOVED &&
	      in.cursor == 0);
	CHECK(ctui_input_key(&in, 5) == CTUI_INPUT_MOVED && in.cursor == 11);
	CHECK(ctui_input_key(&in, 1) == CTUI_INPUT_MOVED && in.cursor == 0);
	CHECK(ctui_input_key(&in, KEY_LEFT) == CTUI_INPUT_NONE);
}

static void test_delete(void)
{
	char buf[32] = "a" E_ACUTE WIDE_O "z";
	ctui_input in;
	ctui_input_init(&in, buf, sizeof buf);
	CHECK(ctui_input_key(&in, 127) == CTUI_INPUT_CHANGED);
	CHECK_STR(buf, "a" E_ACUTE WIDE_O);
	CHECK(ctui_input_key(&in, KEY_BACKSPACE) == CTUI_INPUT_CHANGED);
	CHECK_STR(buf, "a" E_ACUTE);
	CHECK(ctui_input_key(&in, 8) == CTUI_INPUT_CHANGED);
	CHECK_STR(buf, "a");
	ctui_input_home(&in);
	CHECK(!ctui_input_backspace(&in));
	CHECK(ctui_input_key(&in, KEY_DC) == CTUI_INPUT_CHANGED);
	CHECK_STR(buf, "");
	CHECK(ctui_input_key(&in, KEY_DC) == CTUI_INPUT_NONE);

	ctui_input_set(&in, "ab" EMOJI "cd");
	ctui_input_left(&in);
	ctui_input_left(&in);
	CHECK(ctui_input_delete(&in) &&
	      ctui_input_delete(&in)); /* the two after the cursor */
	CHECK_STR(buf, "ab" EMOJI);
}

static void test_words(void)
{
	char buf[64] = "  foo  b" E_ACUTE "r  " WIDE_O WIDE_O " ";
	ctui_input in;
	ctui_input_init(&in, buf, sizeof buf);
	CHECK(ctui_input_prev_word(&in, in.cursor) == 13);
	CHECK(ctui_input_prev_word(&in, 13) == 7);
	CHECK(ctui_input_prev_word(&in, 7) == 2);
	CHECK(ctui_input_prev_word(&in, 2) == 0);
	CHECK(ctui_input_next_word(&in, 0) == 2);
	CHECK(ctui_input_next_word(&in, 2) == 7);
	CHECK(ctui_input_next_word(&in, 7) == 13);
	CHECK(ctui_input_next_word(&in, 13) == strlen(buf));
	CHECK(ctui_input_next_word(&in, 999) == strlen(buf));

	CHECK(ctui_input_key(&in, KEY_SLEFT) == CTUI_INPUT_MOVED &&
	      in.cursor == 13);
	CHECK(ctui_input_key(&in, KEY_SLEFT) == CTUI_INPUT_MOVED &&
	      in.cursor == 7);
	CHECK(ctui_input_key(&in, KEY_SRIGHT) == CTUI_INPUT_MOVED &&
	      in.cursor == 13);
	ctui_input_end(&in);
	CHECK(ctui_input_key(&in, KEY_SRIGHT) == CTUI_INPUT_NONE);

	CHECK(ctui_input_key(&in, 23) == CTUI_INPUT_CHANGED); /* Ctrl-W */
	CHECK_STR(buf, "  foo  b" E_ACUTE "r  ");
	CHECK(ctui_input_key(&in, 23) == CTUI_INPUT_CHANGED);
	CHECK_STR(buf, "  foo  ");
	ctui_input_set(&in, "   ");
	CHECK(ctui_input_key(&in, 23) == CTUI_INPUT_CHANGED);
	CHECK_STR(buf, "");
	CHECK(ctui_input_key(&in, 23) == CTUI_INPUT_NONE);
}

static void test_kill(void)
{
	char buf[32] = "one " E_ACUTE "two";
	ctui_input in;
	ctui_input_init(&in, buf, sizeof buf);
	for (int i = 0; i < 3; i++)
		ctui_input_left(&in);
	CHECK(ctui_input_key(&in, 11) == CTUI_INPUT_CHANGED); /* Ctrl-K */
	CHECK_STR(buf, "one " E_ACUTE);
	CHECK(ctui_input_key(&in, 11) == CTUI_INPUT_NONE);
	ctui_input_left(&in);
	CHECK(ctui_input_key(&in, 21) == CTUI_INPUT_CHANGED); /* Ctrl-U */
	CHECK_STR(buf, E_ACUTE);
	CHECK(in.cursor == 0);
	CHECK(ctui_input_key(&in, 21) == CTUI_INPUT_NONE);
}

static void test_typed_bytes(void)
{
	char buf[32] = "";
	ctui_input in;
	ctui_input_init(&in, buf, sizeof buf);
	/* curses delivers a UTF-8 character one byte at a time */
	CHECK(ctui_input_key(&in, 0xE3) == CTUI_INPUT_NONE);
	CHECK(ctui_input_key(&in, 0x81) == CTUI_INPUT_NONE);
	CHECK(ctui_input_key(&in, 0x8A) == CTUI_INPUT_CHANGED);
	CHECK_STR(buf, WIDE_O);
	CHECK(ctui_input_key(&in, 0xC3) == CTUI_INPUT_NONE);
	CHECK(ctui_input_key(&in, 0xA9) == CTUI_INPUT_CHANGED);
	CHECK(ctui_input_key(&in, 0xF0) == CTUI_INPUT_NONE);
	CHECK(ctui_input_key(&in, 'x') ==
	      CTUI_INPUT_CHANGED); /* cut sequence: dropped */
	CHECK_STR(buf, WIDE_O E_ACUTE "x");
	CHECK(ctui_input_key(&in, 0x80) ==
	      CTUI_INPUT_NONE); /* stray continuation */
	CHECK(ctui_input_key(&in, 0xC0) == CTUI_INPUT_NONE); /* overlong lead */
	CHECK(ctui_input_key(&in, 0xFF) == CTUI_INPUT_NONE);
	CHECK(ctui_input_key(&in, 0xC3) == CTUI_INPUT_NONE);
	CHECK(ctui_input_key(&in, 0x28) ==
	      CTUI_INPUT_CHANGED); /* "(" after a bad tail */
	CHECK_STR(buf, WIDE_O E_ACUTE "x(");
	CHECK(ctui_input_key(&in, 0xC0 + 1) == CTUI_INPUT_NONE);
	CHECK(ctui_input_key(&in, 0x80) == CTUI_INPUT_NONE);
	CHECK(ctui_input_key(&in, 300) == CTUI_INPUT_NONE);
	CHECK(ctui_input_key(&in, KEY_F(1)) == CTUI_INPUT_NONE);
	CHECK(ctui_input_key(&in, 3) == CTUI_INPUT_NONE);
}

static void test_submit_cancel(void)
{
	char buf[8] = "x";
	ctui_input in;
	ctui_input_init(&in, buf, sizeof buf);
	CHECK(ctui_input_key(&in, '\n') == CTUI_INPUT_SUBMITTED);
	CHECK(ctui_input_key(&in, KEY_ENTER) == CTUI_INPUT_SUBMITTED);
	CHECK(ctui_input_key(&in, 27) == CTUI_INPUT_CANCELLED);
	CHECK_STR(buf, "x");
}

static void test_columns_and_scroll(void)
{
	char buf[64] = "ab" WIDE_O "cd" EMOJI "ef";
	ctui_input in;
	ctui_input_init(&in, buf, sizeof buf);
	CHECK(ctui_input_columns(&in, 0, 999) == 2 + 2 + 2 + 2 + 2);
	CHECK(ctui_input_columns(&in, 2, 5) == 2);
	in.mask = true;
	CHECK(ctui_input_columns(&in, 0, 999) == 8); /* one per character */
	in.mask = false;

	ctui_input_fit(&in,
		       6); /* cursor at the end: it needs the last 5 columns */
	CHECK(in.scroll > 0);
	CHECK((int)ctui_input_columns(&in, in.scroll, in.cursor) + 1 <= 6);
	size_t at = in.scroll;
	ctui_input_home(&in);
	ctui_input_fit(&in, 6);
	CHECK(in.scroll == 0 && at > 0);
	ctui_input_end(&in);
	ctui_input_fit(&in, 1000);
	CHECK(in.scroll == 0 ||
	      in.scroll ==
		  at); /* wide enough: scroll only shrinks on cursor moves */
	ctui_input_home(&in);
	ctui_input_fit(&in, 1000);
	CHECK(in.scroll == 0);

	/* a wide character at the cursor needs two columns */
	ctui_input_set(&in, "abc" WIDE_O);
	ctui_input_left(&in);
	ctui_input_fit(&in, 4);
	CHECK((int)ctui_input_columns(&in, in.scroll, in.cursor) + 2 <= 4);
	/* corrupted offsets are repaired */
	in.cursor = 999;
	in.scroll = 999;
	ctui_input_fit(&in, 5);
	CHECK(in.cursor == strlen(buf) && in.scroll <= in.cursor);
}

static void test_click(void)
{
	char buf[64] = "ab" WIDE_O "cd";
	ctui_input in;
	ctui_input_init(&in, buf, sizeof buf);
	CHECK(ctui_input_click(&in, 20, 0) && in.cursor == 0);
	CHECK(ctui_input_click(&in, 20, 1) && in.cursor == 1);
	CHECK(ctui_input_click(&in, 20, 2) &&
	      in.cursor == 2); /* left half of the wide one */
	CHECK(!ctui_input_click(&in, 20, 3) &&
	      in.cursor == 2); /* right half: same character */
	CHECK(ctui_input_click(&in, 20, 4) && in.cursor == 5);
	CHECK(ctui_input_click(&in, 20, 19) &&
	      in.cursor == 7); /* past the text: the end */
	CHECK(ctui_input_click(&in, 20, -3) &&
	      in.cursor == 0); /* left of the field */

	/* with a scrolled field the column counts from the scroll position */
	ctui_input_set(&in, "0123456789");
	ctui_input_fit(&in, 5);
	CHECK(in.scroll == 6);
	CHECK(ctui_input_click(&in, 5, 0) && in.cursor == 6);
	CHECK(ctui_input_click(&in, 5, 2) && in.cursor == 8);
	in.mask = true;
	ctui_input_set(&in, WIDE_O WIDE_O);
	CHECK(ctui_input_click(&in, 5, 1) &&
	      in.cursor == 3); /* masked: one column each */
}

static void test_draw(void)
{
	screen_open(24, 80);
	char buf[64] = "";
	ctui_input in;
	ctui_input_init(&in, buf, sizeof buf);
	in.placeholder = "your name";
	screen_clear();
	ctui_input_draw(&in, 2, 3, 20, true);
	CHECK(screen_has_text("your name"));

	ctui_input_set(&in, "h" E_ACUTE "llo");
	screen_clear();
	ctui_input_draw(&in, 2, 3, 20, true);
	CHECK(screen_has_text("h" E_ACUTE "llo") &&
	      !screen_has_text("your name"));

	in.mask = true;
	screen_clear();
	ctui_input_draw(&in, 2, 3, 20, true);
	CHECK(screen_has_text("*****") && !screen_has_text("llo"));
	in.mask = false;

	ctui_input_set(&in, "0123456789abcdefghij");
	screen_clear();
	ctui_input_draw(&in, 2, 3, 8, false);
	CHECK(screen_has_text("defghij") && !screen_has_text("0123"));
	ctui_input_draw(&in, 2, 3, 0, true); /* zero width: nothing, no crash */

	ctui_input_set(&in, "x");
	ctui_style hot = {0};
	hot.attr[CTUI_PAIR_SELECTION] = ctui_attr_color(CTUI_C_RED, -1);
	in.style = &hot;
	screen_clear();
	ctui_input_draw(&in, 2, 3, 8, true);
	CHECK(screen_has_color(ctui_attr_color(CTUI_C_RED, -1)));
	in.style = NULL;
	screen_clear();
	ctui_input_draw(&in, 2, 3, 8, true);
	CHECK(!screen_has_color(ctui_attr_color(CTUI_C_RED, -1)));
}

int main(void)
{
	RUN(test_init);
	RUN(test_insert_utf8);
	RUN(test_insert_rejects);
	RUN(test_capacity_and_max);
	RUN(test_cursor_moves);
	RUN(test_delete);
	RUN(test_words);
	RUN(test_kill);
	RUN(test_typed_bytes);
	RUN(test_submit_cancel);
	RUN(test_columns_and_scroll);
	RUN(test_click);
	RUN(test_draw);
	return CHECK_DONE();
}
