/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */
#include "check.h"
#include "screen.h"

#include <curses.h>

#include <ctui/textarea.h>

static void type(ctui_textarea *t, const char *s)
{
	for (; *s; s++)
		ctui_textarea_key(t, (unsigned char)*s, 5);
}

static void test_init_and_lines(void)
{
	ctui_textarea t;
	CHECK(ctui_textarea_init(&t, "one\ntwo\n\nfour"));
	CHECK(t.line_count == 4 && ctui_textarea_length(&t) == 13);
	size_t len;
	CHECK(strncmp(ctui_textarea_line(&t, 1, &len), "two", 3) == 0 &&
	      len == 3);
	ctui_textarea_line(&t, 2, &len);
	CHECK(len == 0);
	ctui_textarea_line(&t, 3, &len);
	CHECK(len == 4);
	CHECK(!t.modified);
	ctui_textarea_free(&t);
	CHECK(ctui_textarea_init(&t, "a\n"));
	CHECK(t.line_count == 2); /* a trailing newline starts an empty line */
	ctui_textarea_line(&t, 1, &len);
	CHECK(len == 0);
	ctui_textarea_free(&t);
	CHECK(ctui_textarea_init(&t, NULL));
	CHECK(t.line_count == 1);
	CHECK_STR(ctui_textarea_text(&t), "");
	ctui_textarea_free(&t);
}

static void test_locate(void)
{
	ctui_textarea t;
	ctui_textarea_init(&t, "ab\ncde\n\nf");
	size_t line, byte;
	ctui_textarea_locate(&t, 0, &line, &byte);
	CHECK(line == 0 && byte == 0);
	ctui_textarea_locate(&t, 2, &line, &byte);
	CHECK(line == 0 && byte == 2);
	ctui_textarea_locate(&t, 3, &line, &byte);
	CHECK(line == 1 && byte == 0);
	ctui_textarea_locate(&t, 7, &line, &byte);
	CHECK(line == 2 && byte == 0);
	ctui_textarea_locate(&t, 99, &line, &byte);
	CHECK(line == 3 && byte == 1);
	CHECK(ctui_textarea_offset(&t, 1, 2) == 5);
	CHECK(ctui_textarea_offset(&t, 1, 99) == 6);
	CHECK(ctui_textarea_offset(&t, 99, 0) == 8);
	ctui_textarea_free(&t);
}

static void test_edit_operations(void)
{
	ctui_textarea t;
	ctui_textarea_init(&t, "hello\nworld");
	CHECK(ctui_textarea_insert(&t, 5, ",", 1));
	CHECK_STR(ctui_textarea_text(&t), "hello,\nworld");
	CHECK(t.modified);
	CHECK(ctui_textarea_insert(&t, 7, "big\nnew ", 8));
	CHECK_STR(ctui_textarea_text(&t), "hello,\nbig\nnew world");
	CHECK(t.line_count == 3);
	CHECK(ctui_textarea_delete(&t, 6, 5)); /* removes "\nbig\n" */
	CHECK_STR(ctui_textarea_text(&t), "hello,new world");
	CHECK(t.line_count == 1);
	CHECK(ctui_textarea_replace(&t, 0, 5, "HELLO", 5));
	CHECK_STR(ctui_textarea_text(&t), "HELLO,new world");
	CHECK(!ctui_textarea_delete(&t, 10, 99)); /* out of range */
	CHECK(!ctui_textarea_insert(&t, 99, "x", 1));
	CHECK(!ctui_textarea_insert(&t, 0, "", 0));
	CHECK_STR(ctui_textarea_text(&t), "HELLO,new world");
	ctui_textarea_free(&t);
}

static void test_cursor_follows_edits(void)
{
	ctui_textarea t;
	ctui_textarea_init(&t, "abcdef");
	t.cursor = 4;
	ctui_textarea_insert(&t, 1, "XX", 2);
	CHECK(t.cursor == 6);
	ctui_textarea_insert(&t, 6, "Y",
			     1); /* at the cursor: it stays before */
	CHECK(t.cursor == 7);
	ctui_textarea_delete(&t, 0, 2);
	CHECK(t.cursor == 5);
	t.cursor = 3;
	ctui_textarea_delete(&t, 2, 3); /* the cursor was inside */
	CHECK(t.cursor == 2);
	ctui_textarea_free(&t);
}

static void test_growth(void)
{
	ctui_textarea t;
	ctui_textarea_init(&t, "");
	for (int i = 0; i < 5000; i++)
		type(&t, i % 50 == 49 ? "\n" : "x");
	CHECK(ctui_textarea_length(&t) == 5000);
	CHECK(t.line_count == 101);
	CHECK(strlen(ctui_textarea_text(&t)) == 5000);
	ctui_textarea_free(&t);
}

static void test_typing(void)
{
	ctui_textarea t;
	ctui_textarea_init(&t, "");
	type(&t, "ab");
	CHECK_STR(ctui_textarea_text(&t), "ab");
	CHECK(t.cursor == 2 && t.modified);
	CHECK(ctui_textarea_key(&t, '\n', 5));
	type(&t, "c");
	ctui_textarea_key(&t, '\t', 5);
	CHECK_STR(ctui_textarea_text(&t), "ab\nc\t");
	CHECK(t.line_count == 2);
	CHECK(ctui_textarea_column(&t) == 4);
	CHECK(!ctui_textarea_key(&t, 5, 5)); /* a control key nobody handles */
	ctui_textarea_free(&t);
}

static void test_utf8_typing(void)
{
	ctui_textarea t;
	ctui_textarea_init(&t, "");
	CHECK(!ctui_textarea_key(&t, 0xC3, 5)); /* half of an e acute */
	CHECK_STR(ctui_textarea_text(&t), "");
	CHECK(ctui_textarea_key(&t, 0xA9, 5));
	CHECK_STR(ctui_textarea_text(&t), "\xc3\xa9");
	ctui_textarea_key(&t, 0xE6, 5); /* a wide character, in three bytes */
	ctui_textarea_key(&t, 0x97, 5);
	CHECK(ctui_textarea_key(&t, 0xA5, 5));
	CHECK_STR(ctui_textarea_text(&t), "\xc3\xa9\xe6\x97\xa5");
	CHECK(ctui_textarea_column(&t) == 3);
	/* an interrupted sequence is dropped */
	ctui_textarea_key(&t, 0xE6, 5);
	ctui_textarea_key(&t, 'z', 5);
	CHECK_STR(ctui_textarea_text(&t), "\xc3\xa9\xe6\x97\xa5z");
	/* stray continuation bytes and invalid leads are ignored */
	CHECK(!ctui_textarea_key(&t, 0xA9, 5));
	CHECK(!ctui_textarea_key(&t, 0xFF, 5));
	CHECK(!ctui_textarea_key(&t, 0xC0, 5));
	CHECK_STR(ctui_textarea_text(&t), "\xc3\xa9\xe6\x97\xa5z");
	/* four bytes */
	ctui_textarea_key(&t, 0xF0, 5);
	ctui_textarea_key(&t, 0x9F, 5);
	ctui_textarea_key(&t, 0x98, 5);
	ctui_textarea_key(&t, 0x80, 5);
	CHECK(ctui_textarea_length(&t) == 2 + 3 + 1 + 4);
	ctui_textarea_free(&t);
}

static void test_backspace_delete(void)
{
	ctui_textarea t;
	ctui_textarea_init(&t, "a\xc3\xa9\xe6\x97\xa5\nb");
	ctui_textarea_key(&t, KEY_END, 5);
	CHECK(t.cursor == 6);
	CHECK(ctui_textarea_key(&t, KEY_BACKSPACE,
				5)); /* removes the 3-byte character */
	CHECK_STR(ctui_textarea_text(&t), "a\xc3\xa9\nb");
	ctui_textarea_key(&t, 127, 5);
	CHECK_STR(ctui_textarea_text(&t), "a\nb");
	CHECK(t.cursor == 1);
	CHECK(ctui_textarea_key(&t, KEY_DC, 5)); /* joins the lines */
	CHECK_STR(ctui_textarea_text(&t), "ab");
	CHECK(t.line_count == 1);
	CHECK(ctui_textarea_key(&t, KEY_DC, 5));
	CHECK_STR(ctui_textarea_text(&t), "a");
	CHECK(!ctui_textarea_key(&t, KEY_DC, 5)); /* at the end */
	ctui_textarea_key(&t, KEY_HOME, 5);
	CHECK(!ctui_textarea_key(&t, KEY_BACKSPACE, 5)); /* at the start */
	ctui_textarea_free(&t);
	/* an invalid byte goes one byte at a time */
	ctui_textarea_init(&t, "\xff\xfe");
	ctui_textarea_key(&t, KEY_END, 5);
	ctui_textarea_key(&t, KEY_BACKSPACE, 5);
	CHECK(ctui_textarea_length(&t) == 1);
	ctui_textarea_free(&t);
}

static void test_cursor_moves(void)
{
	ctui_textarea t;
	ctui_textarea_init(&t, "abc\n\xc3\xa9t\nz");
	CHECK(ctui_textarea_key(&t, KEY_RIGHT, 5) && t.cursor == 1);
	ctui_textarea_key(&t, KEY_RIGHT, 5);
	ctui_textarea_key(&t, KEY_RIGHT, 5);
	CHECK(t.cursor == 3);
	ctui_textarea_key(&t, KEY_RIGHT, 5); /* wraps to the next line */
	CHECK(t.cursor == 4);
	ctui_textarea_key(&t, KEY_RIGHT, 5); /* over the 2-byte e acute */
	CHECK(t.cursor == 6);
	ctui_textarea_key(&t, KEY_LEFT, 5);
	CHECK(t.cursor == 4);
	ctui_textarea_key(&t, KEY_LEFT, 5);
	CHECK(t.cursor == 3);
	CHECK(!ctui_textarea_key(&t, KEY_UP, 5));
	CHECK(ctui_textarea_key(&t, KEY_DOWN, 5) &&
	      t.cursor == 7); /* column 3 -> end of the line */
	ctui_textarea_key(&t, KEY_DOWN, 5);
	CHECK(t.cursor == 9);
	CHECK(!ctui_textarea_key(&t, KEY_DOWN, 5));
	ctui_textarea_key(&t, KEY_HOME, 5);
	CHECK(t.cursor == 8);
	ctui_textarea_key(&t, KEY_PPAGE, 5);
	CHECK(t.cursor < 4);
	ctui_textarea_key(&t, KEY_END, 5);
	CHECK(t.cursor == 3);
	ctui_textarea_key(&t, KEY_NPAGE, 5);
	CHECK(t.cursor >= 8);
	ctui_textarea_free(&t);
}

static void test_goal_column(void)
{
	ctui_textarea t;
	ctui_textarea_init(&t, "abcdef\nab\nabcdef");
	ctui_textarea_key(&t, KEY_END, 5);
	ctui_textarea_key(&t, KEY_DOWN, 5);
	CHECK(t.cursor == 9);
	ctui_textarea_key(&t, KEY_DOWN, 5);
	CHECK(t.cursor == 16); /* the goal column survived the short line */
	type(&t, "!");	       /* typing resets it */
	ctui_textarea_key(&t, KEY_UP, 5);
	CHECK(t.cursor == 9);
	ctui_textarea_free(&t);
}

static void test_selection(void)
{
	ctui_textarea t;
	ctui_textarea_init(&t, "abc\ndef");
	size_t a, b;
	CHECK(!ctui_textarea_range(&t, &a, &b));
	ctui_textarea_key(&t, KEY_SRIGHT, 5);
	ctui_textarea_key(&t, KEY_SRIGHT, 5);
	CHECK(ctui_textarea_range(&t, &a, &b) && a == 0 && b == 2);
	CHECK(ctui_textarea_selected_length(&t) == 2);
	ctui_textarea_key(&t, KEY_SF, 5);
	CHECK(ctui_textarea_range(&t, &a, &b) && a == 0 && b == 6);
	CHECK(ctui_textarea_key(&t, 27, 5) && !t.selecting);
	CHECK(!ctui_textarea_key(&t, 27, 5));
	/* backwards selection is normalised */
	ctui_textarea_key(&t, KEY_END, 5);
	ctui_textarea_key(&t, KEY_SLEFT, 5);
	CHECK(ctui_textarea_range(&t, &a, &b) && a == 6 && b == 7);
	/* a plain Left collapses to the start of the selection */
	ctui_textarea_key(&t, KEY_LEFT, 5);
	CHECK(!t.selecting && t.cursor == 6);
	ctui_textarea_key(&t, KEY_SRIGHT, 5);
	ctui_textarea_key(&t, KEY_RIGHT, 5);
	CHECK(!t.selecting && t.cursor == 7);
	CHECK(ctui_textarea_key(&t, 1, 5));
	CHECK(ctui_textarea_range(&t, &a, &b) && a == 0 && b == 7);
	ctui_textarea_free(&t);
}

static void test_typing_over_selection(void)
{
	ctui_textarea t;
	ctui_textarea_init(&t, "hello world");
	ctui_textarea_key(&t, KEY_END, 5);
	for (int i = 0; i < 5; i++)
		ctui_textarea_key(&t, KEY_SLEFT, 5);
	type(&t, "there");
	CHECK_STR(ctui_textarea_text(&t), "hello there");
	CHECK(!t.selecting && t.cursor == 11);
	ctui_textarea_key(&t, 1, 5);
	ctui_textarea_key(&t, KEY_BACKSPACE, 5);
	CHECK_STR(ctui_textarea_text(&t), "");
	ctui_textarea_free(&t);
	ctui_textarea_init(&t, "abcd");
	ctui_textarea_key(&t, KEY_SRIGHT, 5);
	ctui_textarea_key(&t, KEY_SRIGHT, 5);
	ctui_textarea_key(&t, KEY_DC, 5);
	CHECK_STR(ctui_textarea_text(&t), "cd");
	ctui_textarea_free(&t);
}

static void test_limits_and_read_only(void)
{
	ctui_textarea t;
	ctui_textarea_init(&t, "");
	t.max_bytes = 4;
	type(&t, "abcdef");
	CHECK_STR(ctui_textarea_text(&t), "abcd");
	ctui_textarea_key(&t, KEY_BACKSPACE,
			  5); /* "abc": a 2-byte character does not fit */
	ctui_textarea_key(&t, 0xC3, 5);
	CHECK(!ctui_textarea_key(&t, 0xA9, 5));
	CHECK_STR(ctui_textarea_text(&t), "abc");
	ctui_textarea_key(&t, KEY_BACKSPACE, 5);
	ctui_textarea_key(&t, 0xC3, 5);
	CHECK(ctui_textarea_key(&t, 0xA9, 5));
	CHECK_STR(ctui_textarea_text(&t), "ab\xc3\xa9");
	CHECK(ctui_textarea_replace(&t, 0, 4, "xy",
				    2)); /* shrinking is always allowed */
	CHECK(!ctui_textarea_set_text(&t, "12345"));
	CHECK(ctui_textarea_set_text(&t, "1234"));
	ctui_textarea_free(&t);

	ctui_textarea_init(&t, "keep");
	t.read_only = true;
	type(&t, "x");
	ctui_textarea_key(&t, KEY_BACKSPACE, 5);
	ctui_textarea_key(&t, '\n', 5);
	CHECK(!ctui_textarea_insert(&t, 0, "x", 1));
	CHECK_STR(ctui_textarea_text(&t), "keep");
	CHECK(!t.modified);
	CHECK(ctui_textarea_key(&t, KEY_RIGHT, 5) && t.cursor == 1);
	CHECK(ctui_textarea_key(&t, 1, 5) && t.selecting);
	ctui_textarea_free(&t);
}

static void test_set_text(void)
{
	ctui_textarea t;
	ctui_textarea_init(&t, "abc");
	type(&t, "x");
	CHECK(t.modified);
	CHECK(ctui_textarea_set_text(&t, "one\ntwo"));
	CHECK_STR(ctui_textarea_text(&t), "one\ntwo");
	CHECK(t.line_count == 2 && t.cursor == 0 && !t.modified &&
	      !t.selecting);
	CHECK(ctui_textarea_set_text(&t, ctui_textarea_text(&t) +
					     4)); /* from its own buffer */
	CHECK_STR(ctui_textarea_text(&t), "two");
	CHECK(ctui_textarea_set_text(&t, NULL));
	CHECK(t.line_count == 1 && ctui_textarea_length(&t) == 0);
	ctui_textarea_free(&t);
}

static void test_reveal_and_scroll(void)
{
	ctui_textarea t;
	ctui_textarea_init(&t, "0\n1\n2\n3\n4\n5\n6\n7");
	t.cursor = ctui_textarea_offset(&t, 6, 0);
	ctui_textarea_reveal(&t, 3, 10);
	CHECK(t.top == 4);
	t.cursor = ctui_textarea_offset(&t, 1, 0);
	ctui_textarea_reveal(&t, 3, 10);
	CHECK(t.top == 1);
	ctui_textarea_scroll_to(&t, 3, 99);
	CHECK(t.top == 5 && t.cursor == ctui_textarea_offset(&t, 5, 0));
	ctui_textarea_scroll_to(&t, 3, 0);
	CHECK(t.top == 0 && t.cursor == ctui_textarea_offset(&t, 2, 0));
	ctui_textarea_free(&t);
	ctui_textarea_init(&t, "0123456789abcdef");
	t.cursor = 12;
	ctui_textarea_reveal(&t, 3, 5);
	CHECK(t.left == 8);
	t.cursor = 2;
	ctui_textarea_reveal(&t, 3, 5);
	CHECK(t.left == 2);
	ctui_textarea_free(&t);
	/* deleting lines pulls the window back */
	ctui_textarea_init(&t, "0\n1\n2\n3\n4\n5");
	t.top = 3;
	ctui_textarea_delete(&t, 2, 8);
	ctui_textarea_reveal(&t, 3, 5);
	CHECK(t.top == 0);
	ctui_textarea_free(&t);
}

static void test_gutter(void)
{
	ctui_textarea t;
	ctui_textarea_init(&t, "a");
	CHECK(ctui_textarea_gutter(&t) == 0);
	t.line_numbers = true;
	CHECK(ctui_textarea_gutter(&t) == 3);
	for (int i = 0; i < 120; i++)
		ctui_textarea_key(&t, '\n', 5);
	CHECK(ctui_textarea_gutter(&t) == 5);
	ctui_textarea_free(&t);
}

static void test_undo_redo(void)
{
	ctui_textarea t;
	ctui_textarea_init(&t, "base");
	CHECK(!ctui_textarea_undo(&t) && !ctui_textarea_redo(&t));
	ctui_textarea_key(&t, KEY_END, 5);
	type(&t, " one"); /* typing coalesces into one record */
	CHECK(t.undo_len == 1);
	ctui_textarea_key(&t, '\n', 5);
	type(&t, "two");
	CHECK(t.undo_len == 3);
	CHECK_STR(ctui_textarea_text(&t), "base one\ntwo");
	CHECK(ctui_textarea_key(&t, 26, 5));
	CHECK_STR(ctui_textarea_text(&t), "base one\n");
	CHECK(ctui_textarea_key(&t, 26, 5));
	CHECK_STR(ctui_textarea_text(&t), "base one");
	CHECK(t.cursor == 8 && t.line_count == 1);
	CHECK(ctui_textarea_key(&t, 26, 5));
	CHECK_STR(ctui_textarea_text(&t), "base");
	CHECK(t.cursor == 4);
	CHECK(!ctui_textarea_key(&t, 26, 5));
	CHECK(ctui_textarea_key(&t, 25, 5));
	CHECK_STR(ctui_textarea_text(&t), "base one");
	CHECK(t.cursor == 8);
	CHECK(ctui_textarea_redo(&t) && ctui_textarea_redo(&t));
	CHECK_STR(ctui_textarea_text(&t), "base one\ntwo");
	CHECK(!ctui_textarea_redo(&t));
	/* a new edit drops what could be redone */
	ctui_textarea_undo(&t);
	type(&t, "X");
	CHECK(t.redo_len == 0 && !ctui_textarea_redo(&t));
	ctui_textarea_free(&t);
}

static void test_undo_coalescing(void)
{
	ctui_textarea t;
	ctui_textarea_init(&t, "");
	type(&t, "abc");
	ctui_textarea_key(&t, KEY_LEFT, 5); /* moving starts a new record */
	type(&t, "X");
	type(&t, "Y");
	CHECK_STR(ctui_textarea_text(&t), "abXYc");
	CHECK(t.undo_len == 2);
	ctui_textarea_undo(&t);
	CHECK_STR(ctui_textarea_text(&t), "abc");
	CHECK(t.cursor == 2);
	ctui_textarea_undo(&t);
	CHECK_STR(ctui_textarea_text(&t), "");
	ctui_textarea_free(&t);
	/* a public edit between two typed runs breaks the run */
	ctui_textarea_init(&t, "");
	type(&t, "a");
	ctui_textarea_insert(&t, 0, "-", 1);
	type(&t, "b");
	CHECK(t.undo_len == 3);
	ctui_textarea_free(&t);
}

static void test_undo_replacements(void)
{
	ctui_textarea t;
	ctui_textarea_init(&t, "hello world");
	ctui_textarea_key(&t, KEY_END, 5);
	for (int i = 0; i < 5; i++)
		ctui_textarea_key(&t, KEY_SLEFT, 5);
	type(&t, "there"); /* replaces the selection in one record */
	CHECK_STR(ctui_textarea_text(&t), "hello there");
	CHECK(t.undo_len == 1);
	ctui_textarea_undo(&t);
	CHECK_STR(ctui_textarea_text(&t), "hello world");
	ctui_textarea_redo(&t);
	CHECK_STR(ctui_textarea_text(&t), "hello there");
	CHECK(t.cursor == 11);
	/* backspace and delete */
	ctui_textarea_key(&t, KEY_BACKSPACE, 5);
	ctui_textarea_key(&t, KEY_HOME, 5);
	ctui_textarea_key(&t, KEY_DC, 5);
	CHECK_STR(ctui_textarea_text(&t), "ello ther");
	ctui_textarea_undo(&t);
	CHECK_STR(ctui_textarea_text(&t), "hello ther");
	CHECK(t.cursor == 0);
	ctui_textarea_undo(&t);
	CHECK_STR(ctui_textarea_text(&t), "hello there");
	CHECK(t.cursor == 11);
	ctui_textarea_free(&t);
}

static void test_history_bound(void)
{
	ctui_textarea t;
	ctui_textarea_init(&t, "");
	for (int i = 0; i < CTUI_TEXTAREA_HISTORY + 50; i++)
		ctui_textarea_key(&t, '\n', 5);
	CHECK(t.undo_len == CTUI_TEXTAREA_HISTORY);
	int undone = 0;
	while (ctui_textarea_undo(&t))
		undone++;
	CHECK(undone == CTUI_TEXTAREA_HISTORY);
	CHECK(ctui_textarea_length(&t) ==
	      50); /* the oldest changes are gone for good */
	ctui_textarea_free(&t);
}

static void test_undo_misc(void)
{
	ctui_textarea t;
	ctui_textarea_init(&t, "abc");
	type(&t, "x");
	ctui_textarea_set_text(&t, "new");
	CHECK(t.undo_len == 0 && !ctui_textarea_undo(&t));
	type(&t, "y");
	t.read_only = true;
	CHECK(!ctui_textarea_undo(&t));
	CHECK_STR(ctui_textarea_text(&t), "ynew");
	t.read_only = false;
	CHECK(ctui_textarea_undo(&t) && t.modified);
	ctui_textarea_free(&t);
}

static void test_clipboard(void)
{
	ctui_textarea t;
	ctui_textarea_init(&t, "hello world");
	CHECK_STR(ctui_textarea_clipboard(&t), "");
	CHECK(!ctui_textarea_copy(&t) && !ctui_textarea_cut(&t) &&
	      !ctui_textarea_paste(&t));
	for (int i = 0; i < 5; i++)
		ctui_textarea_key(&t, KEY_SRIGHT, 5);
	CHECK(ctui_textarea_key(&t, 3, 5)); /* Ctrl-C */
	CHECK_STR(ctui_textarea_clipboard(&t), "hello");
	CHECK_STR(ctui_textarea_text(&t), "hello world");
	CHECK(ctui_textarea_key(&t, 24, 5)); /* Ctrl-X */
	CHECK_STR(ctui_textarea_text(&t), " world");
	CHECK(t.cursor == 0 && !t.selecting);
	ctui_textarea_key(&t, KEY_END, 5);
	CHECK(ctui_textarea_key(&t, 22, 5)); /* Ctrl-V */
	CHECK_STR(ctui_textarea_text(&t), " worldhello");
	CHECK(t.cursor == 11);
	/* pasting over a selection, multi-line, then one undo step each */
	CHECK(ctui_textarea_set_clipboard(&t, "1\n2\n"));
	ctui_textarea_key(&t, 1, 5);
	CHECK(ctui_textarea_paste(&t));
	CHECK_STR(ctui_textarea_text(&t), "1\n2\n");
	CHECK(t.line_count == 3 && t.cursor == 4);
	ctui_textarea_undo(&t);
	CHECK_STR(ctui_textarea_text(&t), " worldhello");
	ctui_textarea_undo(&t);
	CHECK_STR(ctui_textarea_text(&t), " world");
	ctui_textarea_undo(&t);
	CHECK_STR(ctui_textarea_text(&t), "hello world");
	ctui_textarea_free(&t);
}

static void test_clipboard_limits(void)
{
	ctui_textarea t;
	ctui_textarea_init(&t, "ab");
	ctui_textarea_set_clipboard(&t, "0123456789");
	t.max_bytes = 5;
	CHECK(!ctui_textarea_paste(&t));
	CHECK_STR(ctui_textarea_text(&t), "ab");
	t.max_bytes = 0;
	t.read_only = true;
	ctui_textarea_key(&t, 1, 5);
	CHECK(ctui_textarea_copy(&t)); /* copying is fine */
	CHECK(!ctui_textarea_cut(&t) && !ctui_textarea_paste(&t));
	CHECK_STR(ctui_textarea_clipboard(&t), "ab");
	CHECK_STR(ctui_textarea_text(&t), "ab");
	CHECK(ctui_textarea_set_clipboard(&t, NULL));
	CHECK_STR(ctui_textarea_clipboard(&t), "");
	ctui_textarea_free(&t);
}

static void test_hit(void)
{
	ctui_textarea t;
	ctui_textarea_init(&t, "abc\n\xe6\x97\xa5x\n\nlast line");
	size_t off;
	CHECK(ctui_textarea_hit(&t, 20, 5, 0, 0, &off) && off == 0);
	CHECK(ctui_textarea_hit(&t, 20, 5, 0, 2, &off) && off == 2);
	CHECK(ctui_textarea_hit(&t, 20, 5, 0, 15, &off) &&
	      off == 3); /* past the end of the line */
	CHECK(ctui_textarea_hit(&t, 20, 5, 1, 0, &off) && off == 4);
	CHECK(ctui_textarea_hit(&t, 20, 5, 1, 1, &off) &&
	      off == 4); /* second cell of the wide character */
	CHECK(ctui_textarea_hit(&t, 20, 5, 1, 2, &off) && off == 7);
	CHECK(ctui_textarea_hit(&t, 20, 5, 2, 4, &off) &&
	      off == 9); /* the empty line */
	CHECK(ctui_textarea_hit(&t, 20, 5, 4, 3, &off) &&
	      off == 13); /* below the text: the last line */
	CHECK(!ctui_textarea_hit(&t, 20, 5, 5, 0, &off));
	CHECK(!ctui_textarea_hit(&t, 20, 5, -1, 0, &off));
	CHECK(!ctui_textarea_hit(&t, 20, 5, 0, 20, &off));
	/* with a gutter, a scrolled window and a scrolled column */
	t.line_numbers = true;
	t.top = 1;
	CHECK(ctui_textarea_gutter(&t) == 3);
	CHECK(ctui_textarea_hit(&t, 20, 3, 0, 1, &off) &&
	      off == 4); /* the gutter: start of the line */
	CHECK(ctui_textarea_hit(&t, 20, 3, 0, 5, &off) && off == 7);
	CHECK(ctui_textarea_hit(&t, 20, 3, 2, 5, &off) && off == 10 + 2);
	t.top = 3;
	t.left = 2;
	CHECK(ctui_textarea_hit(&t, 20, 3, 0, 4, &off) &&
	      off == 13); /* scrolled by 2 columns */
	ctui_textarea_free(&t);
}

static void test_mouse(void)
{
	ctui_textarea t;
	ctui_textarea_init(&t, "hello\nworld\nthird");
	CHECK(!ctui_textarea_release(&t) &&
	      !ctui_textarea_drag(&t, 20, 5, 0, 3));
	CHECK(ctui_textarea_press(&t, 20, 5, 1, 2) && t.cursor == 8 &&
	      !t.selecting && t.dragging);
	CHECK(ctui_textarea_drag(&t, 20, 5, 0, 1));
	size_t a, b;
	CHECK(ctui_textarea_range(&t, &a, &b) && a == 1 && b == 8);
	CHECK(!ctui_textarea_drag(&t, 20, 5, 0, 1)); /* same place */
	CHECK(ctui_textarea_drag(&t, 20, 5, 2, 50));
	CHECK(ctui_textarea_range(&t, &a, &b) && a == 8 && b == t.len);
	CHECK(ctui_textarea_release(&t) && !t.dragging && t.selecting);
	CHECK(!ctui_textarea_release(&t));
	/* a click without a drag selects nothing */
	CHECK(ctui_textarea_press(&t, 20, 5, 0, 4));
	CHECK(ctui_textarea_release(&t) && !t.selecting && t.cursor == 4);
	CHECK(!ctui_textarea_press(&t, 20, 5, 9, 0) && !t.dragging);
	/* dragging below the window reaches the line under it */
	ctui_textarea_press(&t, 20, 2, 0, 0);
	ctui_textarea_drag(&t, 20, 2, 5, 1);
	ctui_textarea_release(&t);
	CHECK(ctui_textarea_range(&t, &a, &b) && a == 0 && b == 13);
	/* editing at the clicked place */
	ctui_textarea_press(&t, 20, 5, 0, 5);
	ctui_textarea_release(&t);
	type(&t, "!");
	CHECK_STR(ctui_textarea_text(&t), "hello!\nworld\nthird");
	ctui_textarea_free(&t);
}

static void test_draw(void)
{
	screen_open(24, 80);
	ctui_textarea t;
	ctui_textarea_init(&t, "first\n\tsecond\nbell [\x07] \xe6\x97\xa5");
	t.line_numbers = true;
	screen_clear();
	ctui_textarea_draw(&t, 0, 0, 40, 5, true);
	CHECK(screen_has_text("1 first") && screen_has_text("2     second"));
	CHECK(screen_has_text("bell [?] \xe6\x97\xa5"));
	CHECK(!screen_has_text("\x07"));
	/* the cursor line follows a long line and a small window */
	ctui_textarea_free(&t);
	ctui_textarea_init(&t, "0\n1\n2\n3\n4\n5\n6\n7\n8\n9");
	ctui_textarea_key(&t, KEY_NPAGE, 20);
	screen_clear();
	ctui_textarea_draw(&t, 0, 0, 20, 3, true);
	CHECK(t.top == 7 && screen_has_text("9") && !screen_has_text("6"));
	ctui_textarea_free(&t);
	ctui_textarea_init(&t, "");
	screen_clear();
	ctui_textarea_draw(&t, 0, 0, 20, 3,
			   true); /* an empty text draws its cursor */
	ctui_textarea_draw(&t, 0, 0, 0, 0, true);
	ctui_textarea_free(&t);
}

int main(void)
{
	RUN(test_init_and_lines);
	RUN(test_locate);
	RUN(test_edit_operations);
	RUN(test_cursor_follows_edits);
	RUN(test_growth);
	RUN(test_typing);
	RUN(test_utf8_typing);
	RUN(test_backspace_delete);
	RUN(test_cursor_moves);
	RUN(test_goal_column);
	RUN(test_selection);
	RUN(test_typing_over_selection);
	RUN(test_limits_and_read_only);
	RUN(test_set_text);
	RUN(test_reveal_and_scroll);
	RUN(test_gutter);
	RUN(test_undo_redo);
	RUN(test_undo_coalescing);
	RUN(test_undo_replacements);
	RUN(test_history_bound);
	RUN(test_undo_misc);
	RUN(test_clipboard);
	RUN(test_clipboard_limits);
	RUN(test_hit);
	RUN(test_mouse);
	RUN(test_draw);
	return CHECK_DONE();
}
