/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * test_helpview.c - the key-binding help overlay: layout, scrolling, drawing.
 */
#include "check.h"
#include "screen.h"

#include <curses.h>
#include <stdio.h>

#include <ctui/helpview.h>

#define A_N 10
#define B_N 60

static ctui_help_entry g_a[A_N], g_b[B_N];
static char g_text[A_N + B_N][2][24];
static ctui_help_group g_groups[2];

static void setup(void)
{
	for (int i = 0; i < A_N + B_N; i++) {
		snprintf(g_text[i][0], sizeof g_text[i][0], "Key-%c%02d",
			 i < A_N ? 'A' : 'B', i < A_N ? i : i - A_N);
		snprintf(g_text[i][1], sizeof g_text[i][1], "%c-entry %02d",
			 i < A_N ? 'A' : 'B', i < A_N ? i : i - A_N);
		ctui_help_entry e = {g_text[i][0], g_text[i][1]};
		if (i < A_N)
			g_a[i] = e;
		else
			g_b[i - A_N] = e;
	}
	g_groups[0] = (ctui_help_group){"Group Alpha", g_a, A_N};
	g_groups[1] = (ctui_help_group){"Group Beta", g_b, B_N};
}

static void test_lines(void)
{
	ctui_helpview h;
	ctui_helpview_init(&h, NULL, 0);
	CHECK(ctui_helpview_lines(&h) == 0);
	ctui_helpview_init(&h, g_groups, 2);
	/* two headings, 70 entries and one blank line */
	CHECK(ctui_helpview_lines(&h) == 73);
	ctui_help_group untitled[] = {{NULL, g_a, 3}, {NULL, g_b, 2}};
	ctui_helpview_init(&h, untitled, 2);
	CHECK(ctui_helpview_lines(&h) == 6);
	ctui_help_group empty[] = {{"Empty", NULL, 0}};
	ctui_helpview_init(&h, empty, 1);
	CHECK(ctui_helpview_lines(&h) == 1);
}

static void test_geometry(void)
{
	ctui_helpview h;
	ctui_helpview_init(&h, g_groups, 2);
	ctui_rect box;
	int rows, columns;
	/* wide: two columns */
	CHECK(ctui_helpview_geometry(&h, 24, 80, &box, &rows, &columns));
	CHECK(columns == 2 && box.width == 76 && box.height == 22 &&
	      rows == 19);
	CHECK(ctui_helpview_geometry(&h, 24, 200, &box, &rows, &columns) &&
	      box.width == 100 && columns == 2);
	/* the threshold: inner width 72 */
	CHECK(ctui_helpview_geometry(&h, 24, 80, &box, &rows, &columns) &&
	      columns == 2);
	CHECK(ctui_helpview_geometry(&h, 24, 79, &box, &rows, &columns) &&
	      columns == 1);
	/* narrow: one column */
	CHECK(ctui_helpview_geometry(&h, 24, 50, &box, &rows, &columns) &&
	      columns == 1 && box.width == 46);
	/* too small */
	CHECK(!ctui_helpview_geometry(&h, 24, 22, &box, &rows, &columns));
	CHECK(!ctui_helpview_geometry(&h, 6, 80, &box, &rows, &columns));
	CHECK(ctui_helpview_geometry(&h, 7, 80, &box, &rows, &columns) &&
	      rows == 2);

	/* a short help is only as high as it needs: two columns hold 12 lines
	 */
	ctui_helpview_init(&h, g_groups, 1);
	CHECK(ctui_helpview_geometry(&h, 40, 80, &box, &rows, &columns));
	CHECK(columns == 2 && rows == 6 && box.height == 9);
	CHECK(ctui_helpview_geometry(&h, 40, 60, &box, &rows, &columns));
	CHECK(columns == 1 && rows == 11);
}

static void test_keys(void)
{
	ctui_helpview h;
	ctui_helpview_init(&h, g_groups, 2);
	/* 24x80: 2 columns of 19 rows hold 38 of the 73 lines: top goes up to
	 * 35 */
	CHECK(ctui_helpview_key(&h, KEY_DOWN, 24, 80) == CTUI_DIALOG_OPEN &&
	      h.top == 1);
	ctui_helpview_key(&h, 'j', 24, 80);
	CHECK(h.top == 2);
	ctui_helpview_key(&h, KEY_UP, 24, 80);
	ctui_helpview_key(&h, 'k', 24, 80);
	ctui_helpview_key(&h, 'k', 24, 80); /* stays at the top */
	CHECK(h.top == 0);
	ctui_helpview_key(&h, KEY_NPAGE, 24, 80);
	CHECK(h.top == 37 - 2); /* a page is 37 lines, clamped to 35 */
	ctui_helpview_key(&h, KEY_HOME, 24, 80);
	CHECK(h.top == 0);
	ctui_helpview_key(&h, ' ', 24, 80);
	CHECK(h.top == 35);
	ctui_helpview_key(&h, KEY_DOWN, 24, 80);
	CHECK(h.top == 35); /* clamped */
	ctui_helpview_key(&h, KEY_PPAGE, 24, 80);
	CHECK(h.top == 0);
	ctui_helpview_key(&h, KEY_END, 24, 80);
	CHECK(h.top == 35);
	ctui_helpview_key(&h, 'g', 24, 80);
	CHECK(h.top == 0);
	ctui_helpview_key(&h, 'G', 24, 80);
	ctui_helpview_key(&h, 'b', 24, 80);
	CHECK(h.top == 0);

	/* narrow: one column of 19 rows, top goes up to 54 */
	ctui_helpview_init(&h, g_groups, 2);
	ctui_helpview_key(&h, KEY_END, 24, 50);
	CHECK(h.top == 54);
	ctui_helpview_key(&h, KEY_PPAGE, 24, 50);
	CHECK(h.top == 54 - 18);

	/* closing keys */
	int closing[] = {27, 3, 'q', '?', '\n', KEY_ENTER};
	for (size_t i = 0; i < sizeof closing / sizeof closing[0]; i++)
		CHECK(ctui_helpview_key(&h, closing[i], 24, 80) ==
		      CTUI_DIALOG_CANCELLED);
	CHECK(ctui_helpview_key(&h, 'x', 24, 80) == CTUI_DIALOG_OPEN);

	/* too small a screen: still safe */
	ctui_helpview_init(&h, g_groups, 2);
	CHECK(ctui_helpview_key(&h, KEY_DOWN, 4, 10) == CTUI_DIALOG_OPEN);
	ctui_helpview_key(&h, KEY_END, 4, 10);
	CHECK(h.top == 72);

	/* nothing to show: nothing to scroll */
	ctui_helpview_init(&h, NULL, 0);
	ctui_helpview_key(&h, KEY_DOWN, 24, 80);
	ctui_helpview_key(&h, KEY_END, 24, 80);
	CHECK(h.top == 0);
}

static void test_mouse(void)
{
	ctui_helpview h;
	ctui_helpview_init(&h, g_groups, 2);
	ctui_rect box;
	int rows, columns;
	ctui_helpview_geometry(&h, 24, 80, &box, &rows, &columns);
	CHECK(ctui_helpview_click(&h, 24, 80, box.y, box.x + box.width - 3) ==
	      CTUI_DIALOG_CANCELLED);
	CHECK(ctui_helpview_click(&h, 24, 80, box.y + 3, box.x + 5) ==
	      CTUI_DIALOG_OPEN);
	CHECK(ctui_helpview_click(&h, 24, 80, 0, 0) == CTUI_DIALOG_OPEN);
	CHECK(ctui_helpview_click(&h, 4, 10, 0, 0) == CTUI_DIALOG_OPEN);

	ctui_helpview_wheel(&h, 24, 80, 1);
	CHECK(h.top == 3);
	ctui_helpview_wheel(&h, 24, 80, 1);
	CHECK(h.top == 6);
	ctui_helpview_wheel(&h, 24, 80, -1);
	CHECK(h.top == 3);
	for (int i = 0; i < 50; i++)
		ctui_helpview_wheel(&h, 24, 80, 1);
	CHECK(h.top == 35);
	for (int i = 0; i < 50; i++)
		ctui_helpview_wheel(&h, 24, 80, -1);
	CHECK(h.top == 0);
	ctui_helpview_wheel(&h, 24, 80, 0);
	ctui_helpview_wheel(&h, 4, 10, 1);
	CHECK(h.top == 0);
}

static void test_draw(void)
{
	screen_open(24, 80);
	ctui_helpview h;
	ctui_helpview_init(&h, g_groups, 2);

	/* two columns: the second one starts with line 19, B entry 6 */
	screen_clear();
	ctui_helpview_draw(&h);
	CHECK(screen_has_text("Keyboard shortcuts"));
	CHECK(screen_has_text("Group Alpha") && screen_has_text("Key-A03") &&
	      screen_has_text("A-entry 03"));
	CHECK(screen_has_text("Group Beta") && screen_has_text("B-entry 05") &&
	      screen_has_text("B-entry 06"));
	CHECK(!screen_has_text("B-entry 30"));
	CHECK(screen_has_text("Up/Down: scroll  Esc: close"));
	CHECK(screen_has_text("1-38/73"));

	ctui_helpview_key(&h, KEY_END, 24, 80);
	screen_clear();
	ctui_helpview_draw(&h);
	CHECK(screen_has_text("B-entry 59") && !screen_has_text("Group Alpha"));
	CHECK(screen_has_text("36-73/73"));

	/* narrow: one column, the second column's lines are not shown */
	resizeterm(24, 50);
	ctui_helpview_init(&h, g_groups, 2);
	screen_clear();
	ctui_helpview_draw(&h);
	CHECK(screen_has_text("B-entry 05") && !screen_has_text("B-entry 06"));
	CHECK(screen_has_text("1-19/73"));
	ctui_helpview_key(&h, KEY_END, 24, 50);
	screen_clear();
	ctui_helpview_draw(&h);
	CHECK(screen_has_text("B-entry 59") && screen_has_text("B-entry 41"));

	/* everything fits: no position */
	ctui_help_group one[] = {{"Only", g_a, 3}};
	ctui_helpview_init(&h, one, 1);
	screen_clear();
	ctui_helpview_draw(&h);
	CHECK(screen_has_text("Key-A02") && !screen_has_text("1-"));

	/* per-instance title, labels and style */
	ctui_labels mine = {.help_title = "Aide", .help_hint = "Echap"};
	ctui_style hot = {0};
	hot.attr[CTUI_PAIR_DIALOG] = ctui_attr_color(16, 214);
	h.labels = &mine;
	h.style = &hot;
	screen_clear();
	ctui_helpview_draw(&h);
	CHECK(screen_has_text("Aide") && screen_has_text("Echap") &&
	      !screen_has_text("Keyboard shortcuts"));
	h.title = "Custom";
	screen_clear();
	ctui_helpview_draw(&h);
	CHECK(screen_has_text("Custom") && !screen_has_text("Aide"));

	/* a screen that is too small draws nothing */
	resizeterm(4, 10);
	screen_clear();
	ctui_helpview_draw(&h);
	CHECK(!screen_has_text("Custom"));
	endwin();
}

int main(void)
{
	setup();
	RUN(test_lines);
	RUN(test_geometry);
	RUN(test_keys);
	RUN(test_mouse);
	RUN(test_draw);
	return CHECK_DONE();
}
