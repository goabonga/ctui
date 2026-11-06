/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */
#include "check.h"

#include <ctui/text.h>

static void test_decode(void)
{
	unsigned long cp = 0;
	CHECK(ctui_utf8_decode("a", &cp) == 1 && cp == 'a');
	CHECK(ctui_utf8_decode("\xc3\xa9", &cp) == 2 && cp == 0xE9);
	CHECK(ctui_utf8_decode("\xee\x82\xb0", &cp) == 3 && cp == 0xE0B0);
	CHECK(ctui_utf8_decode("\xf0\x9f\x98\x80", &cp) == 4 && cp == 0x1F600);
	CHECK(ctui_utf8_decode("\xff", &cp) == 1 && cp == 0xFFFD);
	CHECK(ctui_utf8_decode("\xc3", &cp) == 1 &&
	      cp == 0xFFFD); /* truncated */
	CHECK(ctui_utf8_decode("\xc0\x80", &cp) == 1 &&
	      cp == 0xFFFD); /* overlong */
	CHECK(ctui_codepoint_width('a') == 1 &&
	      ctui_codepoint_width('\t') == 1);
	CHECK(ctui_codepoint_width(0x0301) == 0);
	CHECK(ctui_codepoint_width(0x65E5) == 2);
	CHECK(ctui_codepoint_width(0x1F600) == 2);
}

static void test_columns(void)
{
	CHECK(ctui_utf8_columns(NULL) == 0);
	CHECK(ctui_utf8_columns("") == 0);
	CHECK(ctui_utf8_columns("abc") == 3);
	CHECK(ctui_utf8_columns("h\xc3\xa9llo") == 5);		   /* é */
	CHECK(ctui_utf8_columns("\xe6\x97\xa5\xe6\x9c\xac") == 4); /* wide */
	CHECK(ctui_utf8_columns("e\xcc\x81") == 1);    /* combining */
	CHECK(ctui_utf8_columns("\xee\x82\xb0") == 1); /* powerline */
	CHECK(ctui_utf8_columns("\xff\xfe") == 2);     /* invalid */
}

static void test_prefix(void)
{
	CHECK(ctui_utf8_prefix_bytes("abcdef", 3) == 3);
	CHECK(ctui_utf8_prefix_bytes("abc", 10) == 3);
	CHECK(ctui_utf8_prefix_bytes("h\xc3\xa9llo", 2) == 3);
	CHECK(ctui_utf8_prefix_bytes("\xe6\x97\xa5\xe6\x9c\xac", 3) ==
	      3); /* one wide fits */
	CHECK(ctui_utf8_prefix_bytes("\xe6\x97\xa5", 1) == 0);
	CHECK(ctui_utf8_prefix_bytes("abc", 0) == 0);
}

static void test_sanitize(void)
{
	char out[32];
	ctui_sanitize(out, sizeof out, "a\x1b[31mb", true);
	CHECK_STR(out, "a?[31mb");
	ctui_sanitize(out, sizeof out,
		      "x\xff"
		      "y",
		      true);
	CHECK_STR(out, "x?y");
	ctui_sanitize(out, sizeof out, "caf\xc3\xa9", true);
	CHECK_STR(out, "caf\xc3\xa9");
	ctui_sanitize(out, sizeof out, "caf\xc3\xa9", false);
	CHECK_STR(out, "caf?");
	ctui_sanitize(out, sizeof out,
		      "a\xe2\x94\x80"
		      "b",
		      false);
	CHECK_STR(out, "a-b");
	ctui_sanitize(out, 4, "abcdef", true);
	CHECK_STR(out, "abc");
	ctui_sanitize(out, sizeof out, NULL, true);
	CHECK_STR(out, "");
}

static void test_wrap(void)
{
	const char *t = "the quick brown fox";
	CHECK(ctui_wrap_chunk(t, 9) == 9);  /* "the quick" */
	CHECK(ctui_wrap_chunk(t, 12) == 9); /* last space that fits */
	CHECK(ctui_wrap_chunk(t, 80) == 19);
	CHECK(ctui_wrap_skip(t, 9) == 10);
	CHECK(ctui_wrap_chunk("abcdefghij", 4) == 4); /* long word, hard cut */
	CHECK(ctui_wrap_chunk("ab\ncd", 10) == 2);
	CHECK(ctui_wrap_skip("ab\ncd", 2) == 3);
	CHECK(ctui_wrap_chunk("", 10) == 0);
	CHECK(ctui_wrap_chunk("abc", 0) == 0);
}

int main(void)
{
	RUN(test_decode);
	RUN(test_columns);
	RUN(test_prefix);
	RUN(test_sanitize);
	RUN(test_wrap);
	return CHECK_DONE();
}
