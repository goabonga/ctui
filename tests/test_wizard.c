/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * test_wizard.c - step movement, button actions and button layout.
 */
#include "check.h"
#include <ctui/wizard.h>

static const char *const TITLES[] = {"Identity", "Resources", "Review"};
#define COUNT (sizeof TITLES / sizeof TITLES[0])

static void test_steps(void)
{
	ctui_wizard w;
	ctui_wizard_init(&w, TITLES, COUNT);
	CHECK(w.step == 0 && ctui_wizard_first(&w) && !ctui_wizard_last(&w));
	CHECK(!ctui_wizard_apply(&w, CTUI_WIZARD_GO_BACK));
	CHECK(ctui_wizard_apply(&w, CTUI_WIZARD_GO_NEXT) && w.step == 1);
	CHECK(ctui_wizard_apply(&w, CTUI_WIZARD_GO_NEXT) && w.step == 2);
	CHECK(ctui_wizard_last(&w));
	CHECK(!ctui_wizard_apply(&w, CTUI_WIZARD_GO_NEXT));
	CHECK(ctui_wizard_apply(&w, CTUI_WIZARD_GO_BACK) && w.step == 1);
	CHECK(!ctui_wizard_apply(&w, CTUI_WIZARD_DONE));
	CHECK(!ctui_wizard_apply(&w, CTUI_WIZARD_ABORT));
	CHECK(w.step == 1);
}

static void test_single_step(void)
{
	ctui_wizard w;
	ctui_wizard_init(&w, TITLES, 1);
	CHECK(ctui_wizard_first(&w) && ctui_wizard_last(&w));
	CHECK(ctui_wizard_press(&w, CTUI_WIZARD_NEXT) == CTUI_WIZARD_DONE);
}

static void test_press(void)
{
	ctui_wizard w;
	ctui_wizard_init(&w, TITLES, COUNT);
	CHECK(ctui_wizard_press(&w, CTUI_WIZARD_CANCEL) == CTUI_WIZARD_ABORT);
	CHECK(ctui_wizard_press(&w, CTUI_WIZARD_BACK) ==
	      CTUI_WIZARD_NONE); /* nowhere to go */
	CHECK(ctui_wizard_press(&w, CTUI_WIZARD_NEXT) == CTUI_WIZARD_GO_NEXT);
	w.step = 1;
	CHECK(ctui_wizard_press(&w, CTUI_WIZARD_BACK) == CTUI_WIZARD_GO_BACK);
	w.step = 2;
	CHECK(ctui_wizard_press(&w, CTUI_WIZARD_NEXT) ==
	      CTUI_WIZARD_DONE); /* Finish */
	CHECK(ctui_wizard_press(&w, CTUI_WIZARD_BUTTONS) == CTUI_WIZARD_NONE);
}

static void test_keys(void)
{
	ctui_wizard w;
	ctui_wizard_init(&w, TITLES, COUNT);
	CHECK(ctui_wizard_key(&w, CTUI_WIZARD_KEY_BACK) == CTUI_WIZARD_NONE);
	CHECK(ctui_wizard_key(&w, CTUI_WIZARD_KEY_NEXT) == CTUI_WIZARD_GO_NEXT);
	CHECK(ctui_wizard_key(&w, 'x') == CTUI_WIZARD_NONE);
	w.step = 1;
	CHECK(ctui_wizard_key(&w, CTUI_WIZARD_KEY_BACK) == CTUI_WIZARD_GO_BACK);
}

static void test_button_layout(void)
{
	ctui_wizard w;
	ctui_wizard_init(&w, TITLES, COUNT);
	int cancel_at, cancel_w, back_at, back_w, next_at, next_w;
	CHECK(ctui_wizard_button_span(&w, CTUI_WIZARD_CANCEL, 60, &cancel_at,
				      &cancel_w));
	CHECK(ctui_wizard_button_span(&w, CTUI_WIZARD_BACK, 60, &back_at,
				      &back_w));
	CHECK(ctui_wizard_button_span(&w, CTUI_WIZARD_NEXT, 60, &next_at,
				      &next_w));
	CHECK(cancel_w == 10 && back_w == 8 && next_w == 8);
	CHECK(next_at + next_w == 60); /* right-aligned */
	CHECK(back_at + back_w + 2 == next_at);
	CHECK(cancel_at + cancel_w + 2 == back_at);

	/* the last step says Finish, which is wider */
	w.step = 2;
	CHECK(ctui_wizard_button_span(&w, CTUI_WIZARD_NEXT, 60, &next_at,
				      &next_w));
	CHECK(next_w == 10 && next_at + next_w == 60);

	CHECK(!ctui_wizard_button_span(&w, CTUI_WIZARD_NEXT, 20, &next_at,
				       &next_w)); /* too narrow */
	CHECK(!ctui_wizard_button_span(&w, CTUI_WIZARD_BUTTONS, 60, &next_at,
				       &next_w));
}

static void test_hit(void)
{
	ctui_wizard w;
	ctui_wizard_init(&w, TITLES, COUNT);
	int at, cols;
	ctui_wizard_button_span(&w, CTUI_WIZARD_BACK, 60, &at, &cols);
	CHECK(ctui_wizard_hit(&w, 60, at) == CTUI_WIZARD_BACK);
	CHECK(ctui_wizard_hit(&w, 60, at + cols - 1) == CTUI_WIZARD_BACK);
	CHECK(ctui_wizard_hit(&w, 60, at + cols) == -1); /* the gap */
	CHECK(ctui_wizard_hit(&w, 60, 59) == CTUI_WIZARD_NEXT);
	CHECK(ctui_wizard_hit(&w, 60, 0) == -1);
}

static void test_labels(void)
{
	static const char *const titles[] = {"A", "B"};
	ctui_wizard w;
	ctui_wizard_init(&w, titles, 2);
	int offset, columns;
	CHECK(ctui_wizard_button_span(&w, CTUI_WIZARD_NEXT, 60, &offset,
				      &columns));
	CHECK(columns == 8); /* "Next" and its margins */
	ctui_labels over = {.next = "Suivant", .cancel = "Annuler"};
	w.labels = &over;
	CHECK(ctui_wizard_button_span(&w, CTUI_WIZARD_NEXT, 60, &offset,
				      &columns));
	CHECK(columns == 11);
	CHECK(ctui_wizard_button_span(&w, CTUI_WIZARD_CANCEL, 60, &offset,
				      &columns));
	CHECK(columns == 11);
	CHECK(ctui_wizard_button_span(&w, CTUI_WIZARD_BACK, 60, &offset,
				      &columns));
	CHECK(columns == 8); /* "Back" is not overridden */
	ctui_wizard_button_span(&w, CTUI_WIZARD_NEXT, 60, &offset, &columns);
	CHECK(offset + columns ==
	      60); /* the run still ends on the last column */
	CHECK(ctui_wizard_hit(&w, 60, offset) == CTUI_WIZARD_NEXT);
}

int main(void)
{
	RUN(test_steps);
	RUN(test_single_step);
	RUN(test_press);
	RUN(test_keys);
	RUN(test_button_layout);
	RUN(test_hit);
	RUN(test_labels);
	return CHECK_DONE();
}
