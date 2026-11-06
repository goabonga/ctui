/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * test_bigtext.c - block font coverage and width measuring.
 */
#include "check.h"
#include <ctui/bigtext.h>

static unsigned rows_of(unsigned long cp)
{
	unsigned sum = 0;
	for (int r = 0; r < CTUI_BIGTEXT_HEIGHT; r++)
		sum += ctui_bigtext_row(cp, r);
	return sum;
}

static void test_known_glyphs(void)
{
	/* 0 is a ring: full top and bottom, open middle */
	CHECK(ctui_bigtext_row('0', 0) == 7 && ctui_bigtext_row('0', 4) == 7);
	CHECK(ctui_bigtext_row('0', 2) == 5);
	/* 1 has its foot */
	CHECK(ctui_bigtext_row('1', 4) == 7);
	/* the colon is two dots in the middle column */
	CHECK(ctui_bigtext_row(':', 1) == 2 && ctui_bigtext_row(':', 3) == 2);
	CHECK(ctui_bigtext_row(':', 0) == 0 && ctui_bigtext_row(':', 2) == 0);
	/* the dash is one bar */
	CHECK(ctui_bigtext_row('-', 2) == 7 && ctui_bigtext_row('-', 1) == 0);
	/* the dot sits on the baseline */
	CHECK(ctui_bigtext_row('.', 4) == 2 && ctui_bigtext_row('.', 3) == 0);
	/* the slash leans right */
	CHECK(ctui_bigtext_row('/', 0) == 1 && ctui_bigtext_row('/', 4) == 4);
}

static void test_every_character_has_a_glyph(void)
{
	for (unsigned long c = '0'; c <= '9'; c++)
		CHECK(rows_of(c) != 0);
	for (unsigned long c = 'A'; c <= 'Z'; c++)
		CHECK(rows_of(c) != 0);
	CHECK(rows_of(':') && rows_of('.') && rows_of('-') && rows_of('/'));
}

static void test_glyphs_are_distinct(void)
{
	const char *set = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ:.-/";
	for (const char *a = set; *a; a++) {
		for (const char *b = a + 1; *b; b++) {
			int same = 1;
			for (int r = 0; r < CTUI_BIGTEXT_HEIGHT; r++)
				if (ctui_bigtext_row((unsigned char)*a, r) !=
				    ctui_bigtext_row((unsigned char)*b, r))
					same = 0;
			CHECK(!same);
		}
	}
}

static void test_case_is_folded(void)
{
	for (unsigned long c = 'a'; c <= 'z'; c++)
		for (int r = 0; r < CTUI_BIGTEXT_HEIGHT; r++)
			CHECK(ctui_bigtext_row(c, r) ==
			      ctui_bigtext_row(c - 'a' + 'A', r));
}

static void test_unknown_is_blank(void)
{
	CHECK(rows_of(' ') == 0);
	CHECK(rows_of('?') == 0);
	CHECK(rows_of(0xe9) == 0);
	CHECK(rows_of(0x1F600) == 0);
	CHECK(ctui_bigtext_row('8', -1) == 0);
	CHECK(ctui_bigtext_row('8', CTUI_BIGTEXT_HEIGHT) == 0);
	for (unsigned long c = '0'; c <= 'Z'; c++)
		for (int r = 0; r < CTUI_BIGTEXT_HEIGHT; r++)
			CHECK(ctui_bigtext_row(c, r) < 8);
}

static void test_width(void)
{
	CHECK(ctui_bigtext_width("") == 0);
	CHECK(ctui_bigtext_width(NULL) == 0);
	CHECK(ctui_bigtext_width("7") == 3);
	CHECK(ctui_bigtext_width("12:30") == 5 * 3 + 4);
	/* a code point counts once, however many bytes it takes */
	CHECK(ctui_bigtext_width("\xc3\xa9") == 3);
	CHECK(ctui_bigtext_width("a\xc3\xa9z") == 3 * 3 + 2);
}

int main(void)
{
	RUN(test_known_glyphs);
	RUN(test_every_character_has_a_glyph);
	RUN(test_glyphs_are_distinct);
	RUN(test_case_is_folded);
	RUN(test_unknown_is_blank);
	RUN(test_width);
	return CHECK_DONE();
}
