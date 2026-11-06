/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */
#include "check.h"
#include "screen.h"

#include <curses.h>
#include <wchar.h>

#include <ctui/knob.h>

static void fixture(ctui_knob *k)
{
	ctui_knob_init(k, "Cutoff", 0, 100, 1, 50);
}

static void test_init(void)
{
	ctui_knob k;
	fixture(&k);
	CHECK(k.value == 50 && k.def == 50 && k.decimals == 0 && !k.bipolar &&
	      !k.dragging);
	ctui_knob_init(&k, "x", 0, 1, 0.25,
		       7); /* value clamped, two decimals */
	CHECK(k.value == 1 && k.def == 1 && k.decimals == 2);
	ctui_knob_init(&k, "x", 5, 5, 0,
		       5); /* empty range and no step repaired */
	CHECK(k.max == 6 && k.step > 0);
	ctui_knob_init(&k, NULL, 0, 10, 0.1, 3);
	CHECK(k.decimals == 1 && k.label == NULL);
}

static void test_mapping(void)
{
	ctui_knob k;
	fixture(&k);
	CHECK_NEAR(ctui_knob_value_at(&k, 0), 0, 1e-9);
	CHECK_NEAR(ctui_knob_value_at(&k, 1), 100, 1e-9);
	CHECK_NEAR(ctui_knob_value_at(&k, 0.254), 25,
		   1e-9); /* snapped to the step */
	CHECK_NEAR(ctui_knob_value_at(&k, -3), 0, 1e-9);
	CHECK_NEAR(ctui_knob_value_at(&k, 9), 100, 1e-9);
	CHECK_NEAR(ctui_knob_ratio(&k), 0.5, 1e-9);
	k.value = 0;
	CHECK(ctui_knob_position(&k) == 0);
	k.value = 100;
	CHECK(ctui_knob_position(&k) == CTUI_KNOB_POSITIONS - 1);
	k.value = 50;
	CHECK(ctui_knob_position(&k) == 4);
	k.value = 30; /* 0.3 * 8 = 2.4 */
	CHECK(ctui_knob_position(&k) == 2);
	k.value = 32; /* 2.56 */
	CHECK(ctui_knob_position(&k) == 3);
	CHECK_NEAR(ctui_knob_position_value(&k, 0), 0, 1e-9);
	CHECK_NEAR(ctui_knob_position_value(&k, 4), 50, 1e-9);
	CHECK_NEAR(ctui_knob_position_value(&k, 8), 100, 1e-9);
	CHECK_NEAR(ctui_knob_position_value(&k, 99), 100, 1e-9);
	CHECK_NEAR(ctui_knob_position_value(&k, -4), 0, 1e-9);
	/* Every position maps back to itself. */
	for (int p = 0; p < CTUI_KNOB_POSITIONS; p++) {
		k.value = ctui_knob_position_value(&k, p);
		CHECK(ctui_knob_position(&k) == p);
	}
	/* Value to position to value is monotonic. */
	int last = 0;
	for (int v = 0; v <= 100; v++) {
		k.value = v;
		int p = ctui_knob_position(&k);
		CHECK(p >= last);
		last = p;
	}
}

static void test_cells(void)
{
	int row = -1, col = -1;
	CHECK(ctui_knob_cell(0, &row, &col) && row == 2 && col == 0);
	CHECK(ctui_knob_cell(4, &row, &col) && row == 0 &&
	      col == 2); /* top middle */
	CHECK(ctui_knob_cell(8, &row, &col) && row == 2 && col == 4);
	CHECK(!ctui_knob_cell(-1, &row, &col) &&
	      !ctui_knob_cell(CTUI_KNOB_POSITIONS, &row, &col));
	/* The ring has no repeated cell and keeps to a 3 x 5 box. */
	bool seen[3][5] = {{false}};
	for (int p = 0; p < CTUI_KNOB_POSITIONS; p++) {
		bool in_box = ctui_knob_cell(p, &row, &col) && row >= 0 &&
			      row < 3 && col >= 0 && col < CTUI_KNOB_WIDTH;
		CHECK(in_box);
		if (!in_box)
			continue;
		CHECK(!seen[row][col]);
		seen[row][col] = true;
	}
}

static void test_set_reset(void)
{
	ctui_knob k;
	fixture(&k);
	CHECK(ctui_knob_set(&k, 70) && k.value == 70);
	CHECK(!ctui_knob_set(&k, 70));
	CHECK(ctui_knob_set(&k, 500) && k.value == 100);
	CHECK(ctui_knob_set(&k, -5) && k.value == 0);
	CHECK(ctui_knob_reset(&k) && k.value == 50);
	CHECK(!ctui_knob_reset(&k));
}

static void test_format(void)
{
	ctui_knob k;
	char buf[64];
	fixture(&k);
	CHECK_STR(ctui_knob_format(&k, buf, sizeof buf), "50");
	k.unit = "Hz";
	CHECK_STR(ctui_knob_format(&k, buf, sizeof buf), "50Hz");
	ctui_knob_init(&k, "Tune", -12, 12, 0.5, 3.5);
	CHECK_STR(ctui_knob_format(&k, buf, sizeof buf), "3.5");
	k.bipolar = true;
	CHECK_STR(ctui_knob_format(&k, buf, sizeof buf), "+3.5");
	k.value = 0;
	CHECK_STR(ctui_knob_format(&k, buf, sizeof buf), "+0.0");
	k.value = -1e-12; /* never "-0.0" */
	CHECK_STR(ctui_knob_format(&k, buf, sizeof buf), "+0.0");
	k.value = -12;
	CHECK_STR(ctui_knob_format(&k, buf, sizeof buf), "-12.0");
	CHECK_STR(ctui_knob_format(&k, buf, 4),
		  "-12"); /* cut, still terminated */
}

static void test_keys(void)
{
	ctui_knob k;
	fixture(&k);
	CHECK(ctui_knob_key(&k, KEY_RIGHT) && k.value == 51);
	CHECK(ctui_knob_key(&k, KEY_UP) && k.value == 52);
	CHECK(ctui_knob_key(&k, KEY_LEFT) && k.value == 51);
	CHECK(ctui_knob_key(&k, KEY_DOWN) && k.value == 50);
	CHECK(ctui_knob_key(&k, KEY_PPAGE) && k.value == 60);
	CHECK(ctui_knob_key(&k, KEY_NPAGE) && k.value == 50);
	CHECK(ctui_knob_key(&k, KEY_END) && k.value == 100);
	CHECK(ctui_knob_key(&k, KEY_RIGHT) &&
	      k.value == 100); /* handled, stays */
	CHECK(ctui_knob_key(&k, KEY_PPAGE) && k.value == 100);
	CHECK(ctui_knob_key(&k, KEY_HOME) && k.value == 0);
	CHECK(ctui_knob_key(&k, KEY_NPAGE) && k.value == 0);
	CHECK(!ctui_knob_key(&k, 'x') && !ctui_knob_key(&k, '\n') &&
	      !ctui_knob_key(&k, KEY_F(1)));
	CHECK(ctui_knob_key(&k, 'r') && k.value == 50);
	ctui_knob_set(&k, 10);
	CHECK(ctui_knob_key(&k, KEY_DC) && k.value == 50);
}

static void test_fine_keys(void)
{
	ctui_knob k;
	fixture(&k);
	CHECK(ctui_knob_key(&k, KEY_SRIGHT));
	CHECK_NEAR(k.value, 50.1, 1e-9);
	CHECK(ctui_knob_key(&k, KEY_SR));
	CHECK_NEAR(k.value, 50.2, 1e-9);
	CHECK(ctui_knob_key(&k, KEY_SLEFT));
	CHECK(ctui_knob_key(&k, KEY_SF));
	CHECK_NEAR(k.value, 50.0, 1e-9);
	/* Off the grid, a coarse key lands back on it. */
	ctui_knob_set(&k, 50.3);
	ctui_knob_key(&k, KEY_RIGHT);
	CHECK_NEAR(k.value, 51, 1e-9);
	ctui_knob_set(&k, 50.3);
	ctui_knob_key(&k, KEY_LEFT);
	CHECK_NEAR(k.value, 50, 1e-9);
	/* Fine steps stop at the ends. */
	ctui_knob_set(&k, 99.95);
	ctui_knob_key(&k, KEY_SRIGHT);
	ctui_knob_key(&k, KEY_SRIGHT);
	CHECK_NEAR(k.value, 100, 1e-9);
	/* The key that does not change anything at the end is still handled. */
	CHECK(ctui_knob_key(&k, KEY_SRIGHT));
}

static void test_max_off_grid(void)
{
	ctui_knob k;
	ctui_knob_init(&k, "x", 0, 10, 3, 0);
	ctui_knob_key(&k, KEY_RIGHT);
	ctui_knob_key(&k, KEY_RIGHT);
	ctui_knob_key(&k, KEY_RIGHT);
	CHECK_NEAR(k.value, 9, 1e-9);
	ctui_knob_key(&k, KEY_RIGHT); /* 12 is clamped to the maximum */
	CHECK_NEAR(k.value, 10, 1e-9);
	ctui_knob_key(&k, KEY_LEFT);
	CHECK_NEAR(k.value, 9, 1e-9);
}

static void test_bipolar(void)
{
	ctui_knob k;
	ctui_knob_init(&k, "Pan", -100, 100, 5, 0);
	k.bipolar = true;
	CHECK_NEAR(ctui_knob_centre(&k), 0, 1e-9);
	CHECK(ctui_knob_position(&k) == 4);
	CHECK_NEAR(ctui_knob_value_at(&k, 0.5), 0, 1e-9);
	CHECK_NEAR(ctui_knob_value_at(&k, 0.51), 0,
		   1e-9); /* the detent catches near values */
	CHECK_NEAR(ctui_knob_value_at(&k, 0.56), 10, 1e-9);
	CHECK(ctui_knob_key(&k, KEY_RIGHT));
	CHECK_NEAR(k.value, 5, 1e-9);
	ctui_knob_key(&k, KEY_LEFT);
	ctui_knob_key(&k, KEY_LEFT);
	CHECK_NEAR(k.value, -5, 1e-9);
	ctui_knob_key(&k, KEY_HOME);
	CHECK_NEAR(k.value, -100, 1e-9);
	ctui_knob_key(&k, 'r');
	CHECK_NEAR(k.value, 0, 1e-9);
	/* An odd range keeps the grid on the centre, not on the minimum. */
	ctui_knob_init(&k, "x", 0, 10, 4, 5);
	k.bipolar = true;
	ctui_knob_key(&k, KEY_RIGHT);
	CHECK_NEAR(k.value, 9, 1e-9);
	ctui_knob_key(&k, KEY_LEFT);
	CHECK_NEAR(k.value, 5, 1e-9);
}

static void test_wheel(void)
{
	ctui_knob k;
	fixture(&k);
	CHECK(ctui_knob_wheel(&k, 1) && k.value == 51);
	CHECK(ctui_knob_wheel(&k, -3) && k.value == 48);
	CHECK(ctui_knob_wheel(&k, 5) && k.value == 53);
	CHECK(!ctui_knob_wheel(&k, 0));
	ctui_knob_set(&k, 100);
	CHECK(!ctui_knob_wheel(&k, 2));
	ctui_knob_set(&k, 1);
	CHECK(ctui_knob_wheel(&k, -4) && k.value == 0);
}

static void test_mouse(void)
{
	ctui_knob k;
	fixture(&k);
	int w = 11; /* the dial sits at columns 3..7 */
	CHECK(ctui_knob_hit(&k, w, 0, 3) && ctui_knob_hit(&k, w, 4, 7));
	CHECK(!ctui_knob_hit(&k, w, 0, 2) && !ctui_knob_hit(&k, w, 0, 8));
	CHECK(!ctui_knob_hit(&k, w, -1, 5) &&
	      !ctui_knob_hit(&k, w, CTUI_KNOB_HEIGHT, 5));
	CHECK(ctui_knob_hit(&k, 5, 2, 0) && ctui_knob_hit(&k, 3, 2, 0) &&
	      !ctui_knob_hit(&k, 3, 2, 5));
	CHECK(!ctui_knob_drag(&k, 3)); /* no press, no drag */
	CHECK(!ctui_knob_release(&k));
	CHECK(!ctui_knob_press(&k, w, 0, 0) && !k.dragging);
	CHECK(ctui_knob_press(&k, w, 10, 5) == false); /* off the dial */
	CHECK(ctui_knob_press(&k, w, 2, 5) && k.dragging);
	CHECK(ctui_knob_drag(&k, 1)); /* one row up: a twentieth of the range */
	CHECK_NEAR(k.value, 55, 1e-9);
	CHECK(!ctui_knob_drag(&k, 1));
	CHECK(ctui_knob_drag(&k, -3)); /* five rows up */
	CHECK_NEAR(k.value, 75, 1e-9);
	CHECK(ctui_knob_drag(&k, 40)); /* far below: the minimum */
	CHECK_NEAR(k.value, 0, 1e-9);
	CHECK(ctui_knob_drag(
	    &k, 2)); /* back to the pressed row: where it started */
	CHECK_NEAR(k.value, 50, 1e-9);
	CHECK(ctui_knob_drag(&k, -100));
	CHECK_NEAR(k.value, 100, 1e-9);
	CHECK(ctui_knob_release(&k) && !k.dragging);
	CHECK(!ctui_knob_drag(&k, 0));
	/* A drag snaps to the step. */
	ctui_knob_init(&k, "x", 0, 100, 10, 0);
	ctui_knob_press(&k, w, 4, 5);
	ctui_knob_drag(&k, 3); /* 5 units: rounds to the next step */
	CHECK_NEAR(fmod(k.value, 10), 0, 1e-9);
	/* A bipolar drag passes through the centre. */
	ctui_knob_init(&k, "x", -10, 10, 1, 0);
	k.bipolar = true;
	ctui_knob_press(&k, w, 4, 5);
	ctui_knob_drag(&k, 0);
	CHECK_NEAR(k.value, 4, 1e-9);
	ctui_knob_drag(&k, 4);
	CHECK_NEAR(k.value, 0, 1e-9);
}

static wchar_t glyph_at(int y, int x)
{
	cchar_t cell;
	wchar_t wch[CCHARW_MAX];
	attr_t attrs;
	short pair;
	if (mvin_wch(y, x, &cell) != OK ||
	    getcchar(&cell, wch, &attrs, &pair, NULL) != OK)
		return 0;
	return wch[0];
}

static int count_glyph(wchar_t g, int y, int x)
{
	int n = 0;
	for (int row = 0; row < 3; row++)
		for (int col = 0; col < CTUI_KNOB_WIDTH; col++)
			if (glyph_at(y + row, x + col) == g)
				n++;
	return n;
}

static void test_glyph_sets(void)
{
	const ctui_knob_glyphs *u = ctui_knob_glyph_set(true),
			       *a = ctui_knob_glyph_set(false);
	CHECK(u != a && u->lit && u->unlit && u->pointer && u->detent);
	if (!u->lit || !a->lit)
		return;
	CHECK_STR(a->lit, "o");
	CHECK_STR(a->pointer, "O");
	for (const char *s = a->lit; *s; s++)
		CHECK((unsigned char)*s < 0x80);
	CHECK((unsigned char)u->lit[0] >= 0x80);
}

static void test_draw(void)
{
	screen_open(24, 80);
	ctui_knob k;
	fixture(&k);
	k.unit = "%";
	const ctui_knob_glyphs *g = ctui_knob_glyph_set(true);
	wchar_t lit = L'●', unlit = L'·', pointer = L'◉';
	CHECK(strcmp(g->lit, "\xe2\x97\x8f") == 0);

	screen_clear();
	ctui_knob_draw(&k, 2, 4, 11, true); /* dial at columns 7..11 */
	CHECK(screen_has_text("Cutoff") && screen_has_text("50%"));
	/* Position 4 is the top middle: the pointer; 4 lit before, 4 unlit
	 * after. */
	CHECK(glyph_at(2, 9) == pointer);
	CHECK(glyph_at(4, 7) == lit && glyph_at(3, 7) == lit &&
	      glyph_at(2, 7) == lit && glyph_at(2, 8) == lit);
	CHECK(glyph_at(2, 10) == unlit && glyph_at(2, 11) == unlit &&
	      glyph_at(3, 11) == unlit && glyph_at(4, 11) == unlit);
	CHECK(count_glyph(pointer, 2, 7) == 1 && count_glyph(lit, 2, 7) == 4 &&
	      count_glyph(unlit, 2, 7) == 4);
	CHECK(glyph_at(4, 9) == L' '); /* the dial is open at the bottom */
	CHECK(screen_has_color(ctui_attr(CTUI_PAIR_CURSOR, true)));
	CHECK(screen_has_color(
	    ctui_attr(CTUI_PAIR_HEADING, true))); /* focused label */

	screen_clear();
	ctui_knob_draw(&k, 2, 4, 11, false);
	CHECK(!screen_has_color(ctui_attr(CTUI_PAIR_HEADING, true)));

	/* Minimum and maximum. */
	k.value = 0;
	screen_clear();
	ctui_knob_draw(&k, 0, 0, 5, false);
	CHECK(glyph_at(2, 0) == pointer && count_glyph(lit, 0, 0) == 0 &&
	      count_glyph(unlit, 0, 0) == 8);
	k.value = 100;
	screen_clear();
	ctui_knob_draw(&k, 0, 0, 5, false);
	CHECK(glyph_at(2, 4) == pointer && count_glyph(lit, 0, 0) == 8 &&
	      count_glyph(unlit, 0, 0) == 0);

	/* Bipolar: the detent shows at the centre and the lit run starts there.
	 */
	ctui_knob_init(&k, "Pan", -100, 100, 1, 0);
	k.bipolar = true;
	screen_clear();
	ctui_knob_draw(&k, 0, 0, 5, false);
	CHECK(glyph_at(0, 2) == pointer && count_glyph(lit, 0, 0) == 0 &&
	      count_glyph(unlit, 0, 0) == 8);
	CHECK(screen_has_text("+0"));
	k.value = 100;
	screen_clear();
	ctui_knob_draw(&k, 0, 0, 5, false);
	CHECK(glyph_at(2, 4) == pointer && glyph_at(0, 2) == L'◇');
	CHECK(count_glyph(lit, 0, 0) == 3 && count_glyph(unlit, 0, 0) == 4);
	k.value = -100;
	screen_clear();
	ctui_knob_draw(&k, 0, 0, 5, false);
	CHECK(glyph_at(2, 0) == pointer && glyph_at(0, 2) == L'◇');
	CHECK(count_glyph(lit, 0, 0) == 3 && count_glyph(unlit, 0, 0) == 4);
	CHECK(screen_has_text("-100"));

	/* Texts wider than the area are cut, an area narrower than the dial is
	 * safe. */
	ctui_knob_init(&k, "A very long label indeed", 0, 1, 0.01, 0.5);
	screen_clear();
	ctui_knob_draw(&k, 0, 0, 7, false);
	CHECK(screen_has_text("A very ") && !screen_has_text("A very l"));
	ctui_knob_draw(&k, 0, 0, 3, false);
	ctui_knob_draw(&k, 0, 0, 0, false);
	ctui_knob_draw(&k, 0, 0, -2, false);

	/* A per-instance style replaces the colour of the lit cells. */
	ctui_knob_init(&k, "Lvl", 0, 10, 1, 8);
	static ctui_style style;
	style.attr[CTUI_PAIR_BORDER_SELECTED] = ctui_attr_color(CTUI_C_RED, -1);
	k.style = &style;
	screen_clear();
	ctui_knob_draw(&k, 0, 0, 5, false);
	CHECK(screen_has_color(ctui_attr_color(CTUI_C_RED, -1)));
	k.style = NULL;
	screen_clear();
	ctui_knob_draw(&k, 0, 0, 5, false);
	CHECK(!screen_has_color(ctui_attr_color(CTUI_C_RED, -1)));
}

int main(void)
{
	RUN(test_init);
	RUN(test_mapping);
	RUN(test_cells);
	RUN(test_set_reset);
	RUN(test_format);
	RUN(test_keys);
	RUN(test_fine_keys);
	RUN(test_max_off_grid);
	RUN(test_bipolar);
	RUN(test_wheel);
	RUN(test_mouse);
	RUN(test_glyph_sets);
	RUN(test_draw);
	return CHECK_DONE();
}
