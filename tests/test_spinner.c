/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * test_spinner.c - spinner frame sets and the frame chosen from time.
 */
#include "check.h"
#include <ctui/spinner.h>
#include <ctui/text.h>

static void test_frame_counts(void)
{
	CHECK(ctui_spinner_frame_count(CTUI_SPINNER_BRAILLE, true) == 10);
	CHECK(ctui_spinner_frame_count(CTUI_SPINNER_LINE, true) == 4);
	CHECK(ctui_spinner_frame_count(CTUI_SPINNER_DOTS, true) == 8);
	CHECK(ctui_spinner_frame_count(CTUI_SPINNER_BRAILLE, false) == 4);
	// NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
	CHECK(ctui_spinner_frame_count((ctui_spinner_kind)99, true) == 0);
}

static void test_frames_are_distinct_and_one_cell(void)
{
	for (int kind = 0; kind <= CTUI_SPINNER_DOTS; kind++) {
		for (int u = 0; u < 2; u++) {
			size_t n = ctui_spinner_frame_count(
			    (ctui_spinner_kind)kind, u);
			for (size_t i = 0; i < n; i++) {
				const char *a = ctui_spinner_frame(
				    (ctui_spinner_kind)kind, u, i);
				CHECK(a[0] != '\0');
				for (size_t j = i + 1; j < n; j++)
					CHECK(
					    strcmp(a,
						   ctui_spinner_frame(
						       (ctui_spinner_kind)kind,
						       u, j)) != 0);
			}
		}
	}
	/* braille, line and dots frames are one column wide; ASCII dots are
	 * three */
	CHECK(ctui_utf8_columns(
		  ctui_spinner_frame(CTUI_SPINNER_BRAILLE, true, 3)) == 1);
	CHECK(ctui_utf8_columns(
		  ctui_spinner_frame(CTUI_SPINNER_DOTS, true, 5)) == 1);
	CHECK(ctui_utf8_columns(
		  ctui_spinner_frame(CTUI_SPINNER_DOTS, false, 1)) == 3);
}

static void test_ascii_sets_are_ascii(void)
{
	for (int kind = 0; kind <= CTUI_SPINNER_DOTS; kind++) {
		size_t n =
		    ctui_spinner_frame_count((ctui_spinner_kind)kind, false);
		for (size_t i = 0; i < n; i++)
			for (const char *p = ctui_spinner_frame(
				 (ctui_spinner_kind)kind, false, i);
			     *p; p++)
				CHECK((unsigned char)*p < 0x80);
	}
}

static void test_frame_wraps(void)
{
	CHECK_STR(ctui_spinner_frame(CTUI_SPINNER_LINE, false, 0), "-");
	CHECK_STR(ctui_spinner_frame(CTUI_SPINNER_LINE, false, 4), "-");
	CHECK_STR(ctui_spinner_frame(CTUI_SPINNER_LINE, false, 7), "/");
	// NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
	CHECK_STR(ctui_spinner_frame((ctui_spinner_kind)99, true, 3), "");
}

static void test_index_from_time(void)
{
	/* default rate: ten frames a second */
	CHECK(ctui_spinner_index(CTUI_SPINNER_BRAILLE, true, 0, 0) == 0);
	CHECK(ctui_spinner_index(CTUI_SPINNER_BRAILLE, true, 0.05, 0) == 0);
	CHECK(ctui_spinner_index(CTUI_SPINNER_BRAILLE, true, 0.1, 0) == 1);
	CHECK(ctui_spinner_index(CTUI_SPINNER_BRAILLE, true, 0.95, 0) == 9);
	CHECK(ctui_spinner_index(CTUI_SPINNER_BRAILLE, true, 1.0, 0) ==
	      0); /* wraps after ten */
	CHECK(ctui_spinner_index(CTUI_SPINNER_LINE, true, 0.5, 0) == 1);
	/* another rate */
	CHECK(ctui_spinner_index(CTUI_SPINNER_LINE, true, 1.0, 2) == 2);
	CHECK(ctui_spinner_index(CTUI_SPINNER_LINE, true, 2.0, 2) == 0);
	/* a late time keeps working */
	CHECK(ctui_spinner_index(CTUI_SPINNER_BRAILLE, true, 12345.65, 0) == 6);
}

static void test_index_edge_cases(void)
{
	CHECK(ctui_spinner_index(CTUI_SPINNER_BRAILLE, true, -5, 0) == 0);
	CHECK(ctui_spinner_index(CTUI_SPINNER_LINE, true, 0.5, -3) ==
	      1); /* bad rate = default */
	CHECK(ctui_spinner_index(CTUI_SPINNER_LINE, true, 1e300, 0) < 4);
	CHECK(ctui_spinner_index(CTUI_SPINNER_LINE, true, 0.0 / 0.0, 0) == 0);
	// NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
	CHECK(ctui_spinner_index((ctui_spinner_kind)99, true, 3, 0) == 0);
}

int main(void)
{
	RUN(test_frame_counts);
	RUN(test_frames_are_distinct_and_one_cell);
	RUN(test_ascii_sets_are_ascii);
	RUN(test_frame_wraps);
	RUN(test_index_from_time);
	RUN(test_index_edge_cases);
	return CHECK_DONE();
}
