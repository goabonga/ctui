/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * test_tabpanel.c - selection and hit testing of the tab strip.
 */
#include <curses.h>

#include "check.h"
#include <ctui/tabpanel.h>

static const char *const LABELS[] = {"General", "Network", "About"};
#define COUNT (sizeof LABELS / sizeof LABELS[0])

static void test_keys(void)
{
	ctui_tabpanel p;
	ctui_tabpanel_init(&p, LABELS, COUNT);
	CHECK(p.selected == 0);
	CHECK(!ctui_tabpanel_key(&p, KEY_LEFT));
	CHECK(ctui_tabpanel_key(&p, KEY_RIGHT) && p.selected == 1);
	CHECK(ctui_tabpanel_key(&p, 'l') && p.selected == 2);
	CHECK(!ctui_tabpanel_key(&p, KEY_RIGHT));
	CHECK(ctui_tabpanel_key(&p, 'h') && p.selected == 1);
	CHECK(ctui_tabpanel_key(&p, KEY_HOME) && p.selected == 0);
	CHECK(ctui_tabpanel_key(&p, KEY_END) && p.selected == 2);
	CHECK(!ctui_tabpanel_key(&p, 'x'));
	CHECK(!ctui_tabpanel_key(&p, KEY_END));
}

static void test_empty(void)
{
	ctui_tabpanel p;
	ctui_tabpanel_init(&p, NULL, 0);
	CHECK(!ctui_tabpanel_key(&p, KEY_RIGHT));
	CHECK(!ctui_tabpanel_select(&p, 0));
	CHECK(ctui_tabpanel_hit(&p, 40, 3) == -1);
}

static void test_select(void)
{
	ctui_tabpanel p;
	ctui_tabpanel_init(&p, LABELS, COUNT);
	CHECK(ctui_tabpanel_select(&p, 2) && p.selected == 2);
	CHECK(!ctui_tabpanel_select(&p, 3) && p.selected == 2);
}

static void test_widths_and_hit(void)
{
	ctui_tabpanel p;
	ctui_tabpanel_init(&p, LABELS, COUNT);
	CHECK(ctui_tabpanel_tab_width(&p, 0) == 11); /* "General" + 4 */
	CHECK(ctui_tabpanel_tab_width(&p, 2) == 9);
	CHECK(ctui_tabpanel_hit(&p, 60, 0) == 0);
	CHECK(ctui_tabpanel_hit(&p, 60, 10) == 0);
	CHECK(ctui_tabpanel_hit(&p, 60, 11) == 1);
	CHECK(ctui_tabpanel_hit(&p, 60, 21) == 1);
	CHECK(ctui_tabpanel_hit(&p, 60, 22) == 2);
	CHECK(ctui_tabpanel_hit(&p, 60, 30) == 2);
	CHECK(ctui_tabpanel_hit(&p, 60, 31) == -1);
	CHECK(ctui_tabpanel_hit(&p, 60, -1) == -1);
	/* a tab cut by the width is hit only on its visible part */
	CHECK(ctui_tabpanel_hit(&p, 15, 14) == 1);
	CHECK(ctui_tabpanel_hit(&p, 15, 15) == -1);
}

int main(void)
{
	RUN(test_keys);
	RUN(test_empty);
	RUN(test_select);
	RUN(test_widths_and_hit);
	return CHECK_DONE();
}
