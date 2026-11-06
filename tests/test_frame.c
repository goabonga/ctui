/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */
#include "check.h"

#include <ctui/frame.h>

static void test_plan_full(void)
{
	ctui_card_plan p = ctui_card_plan_for(6);
	CHECK(p.title == 0 && p.subtitle == 1 && p.gauge_a == 3 &&
	      p.gauge_b == 4 && p.note == 5);
	p = ctui_card_plan_for(9);
	CHECK(p.gauge_a == 3 && p.note == 5);
}

static void test_plan_medium(void)
{
	ctui_card_plan p = ctui_card_plan_for(5);
	CHECK(p.subtitle == 1 && p.gauge_a == 2 && p.gauge_b == 3 &&
	      p.note == 4);
	p = ctui_card_plan_for(4);
	CHECK(p.subtitle == -1 && p.gauge_a == 1 && p.gauge_b == 2 &&
	      p.note == 3);
}

static void test_plan_compact(void)
{
	ctui_card_plan p = ctui_card_plan_for(3);
	CHECK(p.gauge_a == 1 && p.gauge_b == 2 && p.note == -1);
	p = ctui_card_plan_for(2);
	CHECK(p.gauge_a == 1 && p.gauge_b == -1);
	p = ctui_card_plan_for(1);
	CHECK(p.title == 0 && p.gauge_a == -1 && p.gauge_b == -1 &&
	      p.note == -1);
}

static void test_close_hit(void)
{
	/* a frame at (2, 10), 40 wide: the button covers columns 46..48 */
	CHECK(ctui_frame_close_hit(2, 10, 40, 2, 46));
	CHECK(ctui_frame_close_hit(2, 10, 40, 2, 48));
	CHECK(!ctui_frame_close_hit(2, 10, 40, 2, 45));
	CHECK(!ctui_frame_close_hit(2, 10, 40, 2, 49)); /* the corner */
	CHECK(!ctui_frame_close_hit(2, 10, 40, 3, 47));
	CHECK(!ctui_frame_close_hit(2, 10, 7, 2,
				    12)); /* too narrow for a button */
}

int main(void)
{
	RUN(test_plan_full);
	RUN(test_plan_medium);
	RUN(test_plan_compact);
	RUN(test_close_hit);
	return CHECK_DONE();
}
