/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */
#include "check.h"

#include <ctui/statusbar.h>

static void test_width(void)
{
	ctui_segment s[] = {{"OK", CTUI_TONE_OK, true},
			    {"port", CTUI_TONE_BAR, false}};
	CHECK(ctui_statusbar_width(s, 0) == 0);
	CHECK(ctui_statusbar_width(s, 1) == 2 + 2 + 1);
	CHECK(ctui_statusbar_width(s, 2) == (2 + 3) + (4 + 3));
}

static void test_width_wide(void)
{
	ctui_segment s[] = {{"\xe6\x97\xa5", CTUI_TONE_INFO, false},
			    {NULL, CTUI_TONE_BAR, false}};
	CHECK(ctui_statusbar_width(s, 2) == (2 + 3) + 3);
}

int main(void)
{
	RUN(test_width);
	RUN(test_width_wide);
	return CHECK_DONE();
}
