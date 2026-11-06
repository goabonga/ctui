/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */
#include "check.h"

#include <math.h>

#include <ctui/panel.h>
#include <ctui/text.h>

static void test_row_format(void)
{
	ctui_panel p;
	ctui_panel_init(&p);
	ctui_panel_row(&p, "RPM", "812", NAN, NULL);
	CHECK(p.count == 1);
	CHECK_STR(p.lines[0].text, "  RPM                     812");
	CHECK(p.lines[0].gauge_col == -1);
	ctui_panel_free(&p);
}

static void test_row_gauge_and_note(void)
{
	ctui_panel p;
	ctui_panel_init(&p);
	ctui_panel_row(&p, "Load", "42.5", 0.425, "ok");
	CHECK(p.lines[0].gauge_col == 2 + 18 + 1 + 8 + 2);
	CHECK_NEAR(p.lines[0].gauge_fraction, 0.425, 1e-9);
	size_t len = strlen(p.lines[0].text);
	CHECK(len == (size_t)(p.lines[0].gauge_col + 28 + 2 + 2));
	CHECK(strcmp(p.lines[0].text + len - 2, "ok") == 0);
	ctui_panel_row(&p, "Over", "1", 7.0, NULL);
	CHECK_NEAR(p.lines[1].gauge_fraction, 1.0, 1e-9);
	ctui_panel_free(&p);
}

static void test_wide_label_padding(void)
{
	ctui_panel p;
	ctui_panel_init(&p);
	ctui_panel_row(&p, "\xe6\x97\xa5\xe6\x9c\xac", "1", NAN,
		       NULL); /* 4 columns */
	CHECK(ctui_utf8_columns(p.lines[0].text) == 2 + 18 + 1 + 8);
	ctui_panel_free(&p);
}

static void test_sections(void)
{
	ctui_panel p;
	ctui_panel_init(&p);
	ctui_panel_section(&p, "Empty");
	ctui_panel_row(&p, "Gone", NULL, NAN, NULL);
	CHECK(ctui_panel_is_empty(&p));
	ctui_panel_section(&p, "One");
	ctui_panel_row(&p, "A", "1", NAN, NULL);
	ctui_panel_section(&p, "Two");
	ctui_panel_row(&p, "B", "2", NAN, NULL);
	/* One, rule, A, blank, Two, rule, B */
	CHECK(p.count == 7);
	CHECK(p.lines[0].kind == CTUI_LINE_HEADING);
	CHECK(p.lines[1].kind == CTUI_LINE_RULE);
	CHECK(p.lines[3].kind == CTUI_LINE_BLANK);
	CHECK_STR(p.lines[4].text, "Two");
	ctui_panel_free(&p);
}

static void test_finish(void)
{
	ctui_panel p;
	ctui_panel_init(&p);
	ctui_panel_section(&p, "None");
	ctui_panel_finish(&p, "  No data");
	CHECK(p.count == 1);
	CHECK_STR(p.lines[0].text, "  No data");
	ctui_panel_free(&p);
	ctui_panel_init(&p);
	ctui_panel_add_line(&p, "x");
	ctui_panel_finish(&p, "  No data");
	CHECK(p.count == 1);
	CHECK_STR(p.lines[0].text, "x");
	ctui_panel_free(&p);
}

static void fill(ctui_panel *p)
{
	for (int s = 0; s < 4; s++) {
		char title[16];
		snprintf(title, sizeof title, "S%d", s);
		ctui_panel_section(p, title);
		ctui_panel_row(p, "a", "1", NAN, NULL);
		ctui_panel_row(p, "b", "2", NAN, NULL);
	}
}

static void test_arrange_single(void)
{
	ctui_panel p;
	ctui_panel_layout l;
	ctui_panel_init(&p);
	fill(&p);
	ctui_panel_arrange(&p, 70, true, &l); /* too narrow */
	CHECK(l.columns == 1 && l.width == 70 && l.rows == p.count);
	ctui_panel_layout_free(&l);
	ctui_panel_arrange(&p, 120, false, &l);
	CHECK(l.columns == 1);
	ctui_panel_layout_free(&l);
	ctui_panel_free(&p);
}

static void test_arrange_two_columns(void)
{
	ctui_panel p;
	ctui_panel_layout l;
	ctui_panel_init(&p);
	fill(&p);
	ctui_panel_arrange(&p, 101, true, &l);
	CHECK(l.columns == 2 && l.width == 49);
	/* 4 sections of 4 lines + 3 blanks = 19 lines; split on a blank. */
	CHECK(l.row[0] == 0 && l.column[0] == 0);
	size_t first_right = 0;
	while (first_right < p.count && l.column[first_right] == 0)
		first_right++;
	CHECK(first_right < p.count);
	CHECK(l.row[first_right] == -1); /* leading blank not drawn */
	CHECK(l.row[first_right + 1] == 0 && l.column[first_right + 1] == 1);
	CHECK(p.lines[first_right + 1].kind == CTUI_LINE_HEADING);
	CHECK(l.rows < p.count);
	ctui_panel_layout_free(&l);
	ctui_panel_free(&p);
}

int main(void)
{
	RUN(test_row_format);
	RUN(test_row_gauge_and_note);
	RUN(test_wide_label_padding);
	RUN(test_sections);
	RUN(test_finish);
	RUN(test_arrange_single);
	RUN(test_arrange_two_columns);
	return CHECK_DONE();
}
