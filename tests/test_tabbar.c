/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */
#include "check.h"

#include <ctui/tabbar.h>

static const ctui_tab TABS[] = {
    {"I", "[1]", "One", true, false},
    {NULL, NULL, "Two", true, true},
    {NULL, NULL, "Three", false, false},
};

static ctui_tabbar bar(size_t current)
{
	return (ctui_tabbar){"app", TABS, 3, current, NULL};
}

static void test_layout_first_current(void)
{
	ctui_tab_span s[3];
	ctui_tabbar b = bar(0);
	CHECK(ctui_tabbar_layout(&b, 80, true, s) == 3);
	/* brand " app " = 5; tab 0: hard in (1) + " I One " (7) + hard out (1)
	 */
	CHECK(s[0].x == 5 && s[0].width == 9 && s[0].close_x == -1);
	/* tab 1 follows the current tab: no prefix; " Two x " = 7 */
	CHECK(s[1].x == 14 && s[1].width == 7);
	CHECK(s[1].close_x == 14 + 7 - 2);
	/* tab 2 is after a non-current tab: soft divider (1) + " Three " (7) */
	CHECK(s[2].x == 21 && s[2].width == 8);
}

static void test_layout_ascii_icons(void)
{
	ctui_tab_span s[3];
	ctui_tabbar b = bar(1);
	CHECK(ctui_tabbar_layout(&b, 80, false, s) == 3);
	/* tab 0 not current, first: no prefix; " [1] One " = 9 */
	CHECK(s[0].x == 5 && s[0].width == 9);
	/* tab 1 current: hard in + " Two x " + hard out */
	CHECK(s[1].x == 14 && s[1].width == 9);
	/* tab 2 follows current: no prefix */
	CHECK(s[2].x == 23 && s[2].width == 7);
}

static void test_layout_truncates(void)
{
	ctui_tab_span s[3];
	ctui_tabbar b = bar(0);
	CHECK(ctui_tabbar_layout(&b, 14, true, s) == 1);
	CHECK(ctui_tabbar_layout(&b, 21, true, s) == 2);
	CHECK(ctui_tabbar_layout(&b, 4, true, s) == 0);
}

static void test_hit(void)
{
	ctui_tabbar b = bar(0);
	bool close = true;
	CHECK(ctui_tabbar_hit(&b, 80, true, 2, &close) == -1 && !close);
	CHECK(ctui_tabbar_hit(&b, 80, true, 6, &close) == 0 && !close);
	CHECK(ctui_tabbar_hit(&b, 80, true, 15, &close) == 1 && !close);
	CHECK(ctui_tabbar_hit(&b, 80, true, 19, &close) == 1 && close);
	CHECK(ctui_tabbar_hit(&b, 80, true, 22, NULL) == 2);
	CHECK(ctui_tabbar_hit(&b, 80, true, 70, &close) == -1);
}

int main(void)
{
	RUN(test_layout_first_current);
	RUN(test_layout_ascii_icons);
	RUN(test_layout_truncates);
	RUN(test_hit);
	return CHECK_DONE();
}
