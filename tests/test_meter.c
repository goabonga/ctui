/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */
#include "check.h"
#include "screen.h"

#include <curses.h>
#include <math.h>

#include <ctui/meter.h>

/* The text of the glyph at (y, x), or "" when it cannot be read. */
static const char *cell_text(int y, int x)
{
	static char buf[16];
	cchar_t cell;
	wchar_t wch[CCHARW_MAX];
	attr_t attrs;
	short pair;
	buf[0] = '\0';
	if (mvin_wch(y, x, &cell) == OK &&
	    getcchar(&cell, wch, &attrs, &pair, NULL) == OK) {
		wchar_t one[2] = {wch[0], L'\0'};
		if (wcstombs(buf, one, sizeof buf) == (size_t)-1)
			buf[0] = '\0';
	}
	return buf;
}

static short cell_pair(int y, int x)
{
	cchar_t cell;
	wchar_t wch[CCHARW_MAX];
	attr_t attrs;
	short pair = -1;
	if (mvin_wch(y, x, &cell) == OK)
		getcchar(&cell, wch, &attrs, &pair, NULL);
	return pair;
}

static attr_t cell_attrs(int y, int x)
{
	cchar_t cell;
	wchar_t wch[CCHARW_MAX];
	attr_t attrs = 0;
	short pair;
	if (mvin_wch(y, x, &cell) == OK)
		getcchar(&cell, wch, &attrs, &pair, NULL);
	return attrs;
}

static short pair_of(ctui_pair p, bool bold)
{
	return (short)PAIR_NUMBER((chtype)ctui_attr(p, bold));
}

static void test_db_conversion(void)
{
	CHECK_NEAR(ctui_meter_db(1.0), 0.0, 1e-12);
	CHECK_NEAR(ctui_meter_db(0.5), -6.0206, 1e-4);
	CHECK_NEAR(ctui_meter_db(0.1), -20.0, 1e-9);
	CHECK_NEAR(ctui_meter_db(-0.1), -20.0,
		   1e-9); /* the sign does not matter */
	CHECK_NEAR(ctui_meter_db(2.0), 6.0206, 1e-4);
	CHECK(isinf(ctui_meter_db(0.0)) && ctui_meter_db(0.0) < 0);
	CHECK(isinf(ctui_meter_db(NAN)) && ctui_meter_db(NAN) < 0);
	CHECK_NEAR(ctui_meter_linear(0.0), 1.0, 1e-12);
	CHECK_NEAR(ctui_meter_linear(-20.0), 0.1, 1e-12);
	CHECK_NEAR(ctui_meter_linear(-6.0), 0.5012, 1e-4);
	CHECK_NEAR(ctui_meter_linear(-INFINITY), 0.0, 1e-12);
	for (int i = 0; i <= 9; i++) {
		double db = -60 + 7.5 * i;

		CHECK_NEAR(ctui_meter_db(ctui_meter_linear(db)), db, 1e-9);
	}
}

static void test_fraction_and_cells(void)
{
	CHECK_NEAR(ctui_meter_fraction(-60.0), 0.0, 1e-12);
	CHECK_NEAR(ctui_meter_fraction(-30.0), 0.5, 1e-12);
	CHECK_NEAR(ctui_meter_fraction(0.0), 1.0, 1e-12);
	CHECK_NEAR(ctui_meter_fraction(-90.0), 0.0, 1e-12);
	CHECK_NEAR(ctui_meter_fraction(6.0), 1.0, 1e-12);
	CHECK_NEAR(ctui_meter_fraction(-INFINITY), 0.0, 1e-12);
	CHECK_NEAR(ctui_meter_fraction(NAN), 0.0, 1e-12);

	CHECK(ctui_meter_cells(-60.0, 20) == 0);
	CHECK(ctui_meter_cells(-30.0, 20) == 10);
	CHECK(ctui_meter_cells(0.0, 20) == 20);
	CHECK(ctui_meter_cells(10.0, 20) == 20);
	CHECK(ctui_meter_cells(-59.0, 20) == 0); /* under half a cell */
	CHECK(ctui_meter_cells(-58.0, 20) == 1); /* over half a cell */
	CHECK(ctui_meter_cells(-30.0, 0) == 0);
	CHECK(ctui_meter_cells(-30.0, -3) == 0);

	CHECK(ctui_meter_cell(-60.0, 20) == 0);
	CHECK(ctui_meter_cell(-30.0, 20) == 10);
	CHECK(ctui_meter_cell(0.0, 20) == 19);
	CHECK(ctui_meter_cell(12.0, 20) == 19);
	CHECK(ctui_meter_cell(-30.0, 0) == -1);
}

static void test_cell_db(void)
{
	CHECK_NEAR(ctui_meter_cell_db(0, 60), -59.5, 1e-9);
	CHECK_NEAR(ctui_meter_cell_db(59, 60), -0.5, 1e-9);
	CHECK_NEAR(ctui_meter_cell_db(-4, 60), -59.5, 1e-9);
	CHECK_NEAR(ctui_meter_cell_db(99, 60), -0.5, 1e-9);
	CHECK_NEAR(ctui_meter_cell_db(0, 0), CTUI_METER_FLOOR_DB, 1e-12);
	for (int length = 1; length <= 40; length++)
		for (int c = 0; c < length; c++)
			CHECK(ctui_meter_cell(ctui_meter_cell_db(c, length),
					      length) == c);
}

static void test_zones(void)
{
	CHECK(ctui_meter_zone_of(-60.0) == CTUI_METER_GREEN);
	CHECK(ctui_meter_zone_of(-18.5) == CTUI_METER_GREEN);
	CHECK(ctui_meter_zone_of(-18.0) == CTUI_METER_YELLOW);
	CHECK(ctui_meter_zone_of(-6.5) == CTUI_METER_YELLOW);
	CHECK(ctui_meter_zone_of(-6.0) == CTUI_METER_RED);
	CHECK(ctui_meter_zone_of(0.0) == CTUI_METER_RED);
	CHECK(ctui_meter_zone_pair(CTUI_METER_GREEN, false) ==
	      CTUI_PAIR_GAUGE_GREEN);
	CHECK(ctui_meter_zone_pair(CTUI_METER_GREEN, true) ==
	      CTUI_PAIR_GAUGE_GREEN_BRIGHT);
	CHECK(ctui_meter_zone_pair(CTUI_METER_YELLOW, false) ==
	      CTUI_PAIR_GAUGE_YELLOW);
	CHECK(ctui_meter_zone_pair(CTUI_METER_YELLOW, true) ==
	      CTUI_PAIR_GAUGE_YELLOW_BRIGHT);
	CHECK(ctui_meter_zone_pair(CTUI_METER_RED, false) ==
	      CTUI_PAIR_GAUGE_RED);
	CHECK(ctui_meter_zone_pair(CTUI_METER_RED, true) ==
	      CTUI_PAIR_GAUGE_RED_BRIGHT);
}

static void test_scale_length(void)
{
	CHECK(ctui_meter_scale_length(0) == 0);
	CHECK(ctui_meter_scale_length(1) == 0);
	CHECK(ctui_meter_scale_length(2) == 1);
	CHECK(ctui_meter_scale_length(30) == 29);
}

static void test_peak_update(void)
{
	ctui_meter_peak p = {CTUI_METER_FLOOR_DB, 0};
	ctui_meter_peak_update(&p, -20.0, 0.1, 1.0, 10.0);
	CHECK_NEAR(p.db, -20.0, 1e-12); /* follows the level up at once */
	ctui_meter_peak_update(&p, -40.0, 0.5, 1.0, 10.0);
	CHECK_NEAR(p.db, -20.0, 1e-12); /* held */
	CHECK_NEAR(p.held, 0.5, 1e-12);
	ctui_meter_peak_update(&p, -40.0, 0.5, 1.0, 10.0);
	CHECK_NEAR(p.db, -20.0, 1e-12); /* the hold is exactly over */
	ctui_meter_peak_update(&p, -40.0, 0.5, 1.0, 10.0);
	CHECK_NEAR(p.db, -25.0, 1e-12); /* 0.5 s of fall at 10 dB/s */
	ctui_meter_peak_update(&p, -40.0, 0.25, 1.0, 10.0);
	CHECK_NEAR(p.db, -27.5, 1e-12);
	ctui_meter_peak_update(&p, -40.0, 100.0, 1.0, 10.0);
	CHECK_NEAR(p.db, -40.0, 1e-12); /* never below the level */
	ctui_meter_peak_update(&p, -10.0, 0.0, 1.0, 10.0);
	CHECK_NEAR(p.db, -10.0, 1e-12);
	CHECK_NEAR(p.held, 0.0, 1e-12); /* a new peak restarts the hold */
}

static void test_peak_update_partial_step(void)
{
	/* the hold ends in the middle of a step: only the rest of it falls */
	ctui_meter_peak p = {-10.0, 0.8};
	ctui_meter_peak_update(&p, -60.0, 0.5, 1.0, 10.0);
	CHECK_NEAR(p.held, 1.3, 1e-12);
	CHECK_NEAR(p.db, -10.0 - 3.0, 1e-12);
	/* a negative or NaN step counts as zero */
	ctui_meter_peak q = {-10.0, 5.0};
	ctui_meter_peak_update(&q, -60.0, -3.0, 1.0, 10.0);
	CHECK_NEAR(q.db, -10.0, 1e-12);
	ctui_meter_peak_update(&q, -60.0, NAN, 1.0, 10.0);
	CHECK_NEAR(q.db, -10.0, 1e-12);
	/* a level equal to the peak keeps it held */
	ctui_meter_peak r = {-10.0, 3.0};
	ctui_meter_peak_update(&r, -10.0, 1.0, 1.0, 10.0);
	CHECK_NEAR(r.held, 0.0, 1e-12);
}

static void test_init_and_set(void)
{
	ctui_meter m;
	ctui_meter_init(&m, true);
	CHECK(m.vertical && m.show_peak && !m.clip && !m.dragging);
	CHECK_NEAR(m.level, CTUI_METER_FLOOR_DB, 1e-12);
	CHECK_NEAR(m.peak.db, CTUI_METER_FLOOR_DB, 1e-12);
	CHECK_NEAR(m.hold, CTUI_METER_HOLD, 1e-12);
	CHECK_NEAR(m.decay, CTUI_METER_DECAY, 1e-12);
	ctui_meter_init(&m, false);
	CHECK(!m.vertical);

	ctui_meter_set_db(&m, -12.0, 0.1);
	CHECK_NEAR(m.level, -12.0, 1e-12);
	CHECK_NEAR(m.peak.db, -12.0, 1e-12);
	CHECK(!m.clip);
	ctui_meter_set_db(&m, -30.0, 0.1);
	CHECK_NEAR(m.level, -30.0, 1e-12);
	CHECK_NEAR(m.peak.db, -12.0, 1e-12);
	ctui_meter_set_db(&m, -30.0, 3.0); /* 1.5 s hold, then 1.6 s of fall */
	CHECK_NEAR(m.peak.db, -30.0, 1e-12);

	ctui_meter_set_db(&m, NAN, 0.0);
	CHECK_NEAR(m.level, CTUI_METER_FLOOR_DB, 1e-12);
	ctui_meter_set_db(&m, -500.0, 0.0);
	CHECK_NEAR(m.level, CTUI_METER_FLOOR_DB, 1e-12);
	ctui_meter_set_db(&m, INFINITY, 0.0);
	CHECK_NEAR(m.level, CTUI_METER_MAX_DB, 1e-12);
	CHECK(m.clip);
	ctui_meter_set_db(&m, -40.0, 0.0);
	CHECK(m.clip); /* latched */
	ctui_meter_clear(&m);
	CHECK(!m.clip);
	CHECK_NEAR(m.peak.db, -40.0, 1e-12);

	ctui_meter_set_linear(&m, 0.5, 0.0);
	CHECK_NEAR(m.level, -6.0206, 1e-4);
	ctui_meter_set_linear(&m, 0.0, 0.0);
	CHECK_NEAR(m.level, CTUI_METER_FLOOR_DB, 1e-12);
	ctui_meter_set_linear(&m, 1.0, 0.0);
	CHECK(m.clip);
}

static void test_place_and_pair(void)
{
	ctui_meter l, r;
	ctui_meter_init(&l, false);
	ctui_meter_init(&r, true);
	ctui_meter_place(&l, 2, 3, 40, 1);
	CHECK(l.y == 2 && l.x == 3 && l.width == 40 && l.height == 1);
	ctui_meter_place(&l, 0, 0, -5, -1);
	CHECK(l.width == 0 && l.height == 0);

	ctui_meter_place_pair(&l, &r, 1, 2, 30, 3); /* horizontal: stacked */
	CHECK(!r.vertical);
	CHECK(l.y == 1 && l.height == 1 && r.y == 3 && r.height == 1);
	CHECK(l.x == 2 && r.x == 2 && l.width == 30 && r.width == 30);
	ctui_meter_place_pair(&l, &r, 1, 2, 30, 2); /* no room for a gap */
	CHECK(l.height == 1 && r.y == 2 && r.height == 1);
	ctui_meter_place_pair(&l, &r, 0, 0, 30, 5);
	CHECK(l.height == 2 && r.y == 3 && r.height == 2);

	ctui_meter_init(&l, true);
	ctui_meter_place_pair(&l, &r, 4, 10, 5,
			      12); /* vertical: side by side */
	CHECK(r.vertical);
	CHECK(l.x == 10 && l.width == 2 && r.x == 13 && r.width == 2);
	CHECK(l.y == 4 && r.y == 4 && l.height == 12 && r.height == 12);
	ctui_meter_place_pair(&l, &r, 4, 10, 1, 12);
	CHECK(l.width == 1 && r.width == 0);
}

static void test_keys(void)
{
	ctui_meter m;
	ctui_meter_init(&m, false);
	CHECK(ctui_meter_key(&m, KEY_UP));
	CHECK_NEAR(m.level, -59.0, 1e-12);
	CHECK(ctui_meter_key(&m, KEY_RIGHT));
	CHECK_NEAR(m.level, -58.0, 1e-12);
	CHECK(ctui_meter_key(&m, KEY_PPAGE));
	CHECK_NEAR(m.level, -52.0, 1e-12);
	CHECK(ctui_meter_key(&m, KEY_DOWN));
	CHECK(ctui_meter_key(&m, KEY_LEFT));
	CHECK_NEAR(m.level, -54.0, 1e-12);
	CHECK(ctui_meter_key(&m, KEY_NPAGE));
	CHECK_NEAR(m.level, -60.0, 1e-12); /* stops at the floor */
	CHECK(ctui_meter_key(&m, KEY_END));
	CHECK_NEAR(m.level, 0.0, 1e-12);
	CHECK(m.clip);
	CHECK(ctui_meter_key(&m, KEY_HOME));
	CHECK_NEAR(m.level, CTUI_METER_FLOOR_DB, 1e-12);
	CHECK_NEAR(m.peak.db, 0.0, 1e-12); /* the peak stays */
	CHECK(ctui_meter_key(&m, 'c'));
	CHECK(!m.clip);
	CHECK_NEAR(m.peak.db, CTUI_METER_FLOOR_DB, 1e-12);
	CHECK(!ctui_meter_key(&m, 'x'));
	ctui_meter_set_db(&m, -5.0, 0.0);
	ctui_meter_key(&m, KEY_PPAGE);
	ctui_meter_key(&m, KEY_PPAGE);
	CHECK_NEAR(m.level, 7.0, 1e-12);
	for (int i = 0; i < 10; i++)
		ctui_meter_key(&m, KEY_UP);
	CHECK_NEAR(m.level, CTUI_METER_MAX_DB,
		   1e-12); /* and stops at the top */
}

static void test_mouse_horizontal(void)
{
	ctui_meter m;
	ctui_meter_init(&m, false);
	ctui_meter_place(&m, 5, 10, 61,
			 2); /* 60 cells of scale, clip at column 70 */
	CHECK(ctui_meter_over(&m, 5, 10) && ctui_meter_over(&m, 6, 70));
	CHECK(!ctui_meter_over(&m, 4, 10) && !ctui_meter_over(&m, 7, 10));
	CHECK(!ctui_meter_over(&m, 5, 9) && !ctui_meter_over(&m, 5, 71));
	CHECK(!ctui_meter_press(&m, 9, 9));
	CHECK(!m.dragging);

	CHECK(ctui_meter_press(&m, 5, 40)); /* cell 30 */
	CHECK(m.dragging);
	CHECK_NEAR(m.level, -29.5, 1e-9);
	CHECK(ctui_meter_drag(&m, 5, 50));
	CHECK_NEAR(m.level, -19.5, 1e-9);
	CHECK(ctui_meter_drag(&m, 5, 300)); /* past the end: the last cell */
	CHECK_NEAR(m.level, -0.5, 1e-9);
	CHECK(!m.clip);
	CHECK(ctui_meter_drag(&m, 5, -20)); /* before the start: the floor */
	CHECK_NEAR(m.level, CTUI_METER_FLOOR_DB, 1e-12);
	CHECK(ctui_meter_release(&m));
	CHECK(!m.dragging);
	CHECK(!ctui_meter_release(&m));
	CHECK(!ctui_meter_drag(&m, 5, 40)); /* no press, no drag */
	CHECK_NEAR(m.level, CTUI_METER_FLOOR_DB, 1e-12);

	m.clip = true;
	CHECK(ctui_meter_press(&m, 5, 70)); /* on the clip indicator */
	CHECK(!m.clip && !m.dragging);
	CHECK_NEAR(m.level, CTUI_METER_FLOOR_DB, 1e-12);

	CHECK(ctui_meter_wheel(&m, 5, 30, 5));
	CHECK_NEAR(m.level, -55.0, 1e-12);
	CHECK(ctui_meter_wheel(&m, 6, 30, -2));
	CHECK_NEAR(m.level, -57.0, 1e-12);
	CHECK(!ctui_meter_wheel(&m, 8, 30, 5));
	CHECK_NEAR(m.level, -57.0, 1e-12);
}

static void test_mouse_vertical(void)
{
	ctui_meter m;
	ctui_meter_init(&m, true);
	ctui_meter_place(&m, 2, 8, 3,
			 21); /* rows 2..22, 20 cells of scale; clip on row 2 */
	CHECK(
	    ctui_meter_press(&m, 22, 8)); /* the bottom row is the floor end */
	CHECK_NEAR(m.level, -58.5, 1e-9);
	CHECK(ctui_meter_drag(&m, 12, 8)); /* cell 10 */
	CHECK_NEAR(m.level, -28.5, 1e-9);
	CHECK(ctui_meter_drag(&m, -4,
			      8)); /* above the meter: the top of the scale */
	CHECK_NEAR(m.level, -1.5, 1e-9);
	CHECK(ctui_meter_drag(&m, 99, 8)); /* below: the floor */
	CHECK_NEAR(m.level, CTUI_METER_FLOOR_DB, 1e-12);
	ctui_meter_release(&m);
	m.clip = true;
	CHECK(ctui_meter_press(&m, 2, 9));
	CHECK(!m.clip);
	CHECK(ctui_meter_wheel(&m, 10, 9, 3));
	CHECK_NEAR(m.level, -57.0, 1e-12);

	ctui_meter_place(&m, 0, 0, 3, 1); /* nothing but the clip cell */
	CHECK(ctui_meter_press(&m, 0, 0));
	CHECK(!m.dragging);
}

#define FULL "\xe2\x96\x88"
#define SHADE "\xe2\x96\x91"

static void test_draw_horizontal(void)
{
	screen_open(24, 80);
	ctui_meter m;
	ctui_meter_init(&m, false);
	ctui_meter_place(
	    &m, 2, 4, 31,
	    2); /* 30 cells of 2 dB; the clip indicator at column 34 */
	m.show_peak = false;
	ctui_meter_set_db(&m, -30.0, 0.0); /* 15 cells lit */
	screen_clear();
	ctui_meter_draw(&m);
	for (int row = 2; row < 4; row++) {
		CHECK_STR(cell_text(row, 4), FULL);
		CHECK_STR(cell_text(row, 18), FULL);
		CHECK_STR(cell_text(row, 19), SHADE);
		CHECK_STR(cell_text(row, 33), SHADE);
		CHECK_STR(cell_text(row, 34), SHADE); /* clip indicator, off */
	}
	CHECK_STR(cell_text(1, 4), " ");
	CHECK_STR(cell_text(4, 4), " ");
	CHECK_STR(cell_text(2, 35), " ");
	/* lit cells are bright, unlit ones dim, in the colour of their zone */
	CHECK(cell_pair(2, 4) == pair_of(CTUI_PAIR_GAUGE_GREEN_BRIGHT, false));
	CHECK(!(cell_attrs(2, 4) & A_DIM));
	CHECK(cell_pair(2, 19) == pair_of(CTUI_PAIR_GAUGE_GREEN, false));
	CHECK(cell_attrs(2, 19) & A_DIM);
	CHECK(cell_pair(2, 4 + 21) == pair_of(CTUI_PAIR_GAUGE_YELLOW, false));
	CHECK(cell_pair(2, 4 + 26) == pair_of(CTUI_PAIR_GAUGE_YELLOW, false));
	CHECK(cell_pair(2, 4 + 27) == pair_of(CTUI_PAIR_GAUGE_RED, false));
	CHECK(cell_pair(2, 4 + 29) == pair_of(CTUI_PAIR_GAUGE_RED, false));
	CHECK(cell_pair(2, 34) == pair_of(CTUI_PAIR_GAUGE_RED, false));
	CHECK(cell_attrs(2, 34) & A_DIM);

	/* a loud level lights the yellow and red zones and the clip indicator
	 */
	ctui_meter_set_db(&m, 0.0, 0.0);
	screen_clear();
	ctui_meter_draw(&m);
	CHECK_STR(cell_text(2, 4 + 29), FULL);
	CHECK(cell_pair(2, 4 + 29) ==
	      pair_of(CTUI_PAIR_GAUGE_RED_BRIGHT, false));
	CHECK(cell_pair(2, 4 + 23) ==
	      pair_of(CTUI_PAIR_GAUGE_YELLOW_BRIGHT, false));
	CHECK_STR(cell_text(3, 34), FULL);
	CHECK(cell_pair(3, 34) == pair_of(CTUI_PAIR_GAUGE_RED_BRIGHT, true));
	CHECK(!(cell_attrs(3, 34) & A_DIM));
	ctui_meter_clear(&m);
	ctui_meter_set_db(&m, -60.0, 0.0);
	screen_clear();
	ctui_meter_draw(&m);
	CHECK_STR(cell_text(2, 4), SHADE);
	CHECK_STR(cell_text(2, 34), SHADE); /* cleared */
}

static void test_draw_peak(void)
{
	screen_open(24, 80);
	ctui_meter m;
	ctui_meter_init(&m, false);
	ctui_meter_place(&m, 0, 0, 31, 1);
	ctui_meter_set_db(&m, -12.0, 0.0); /* peak on cell 24 (yellow) */
	ctui_meter_set_db(&m, -40.0, 0.1); /* the level falls, the peak stays */
	screen_clear();
	ctui_meter_draw(&m);
	CHECK_STR(cell_text(0, 24), "\xe2\x94\x82"); /* box_v */
	CHECK(cell_pair(0, 24) == pair_of(CTUI_PAIR_GAUGE_YELLOW_BRIGHT, true));
	CHECK_STR(cell_text(0, 9), FULL); /* the level: 10 cells */
	CHECK_STR(cell_text(0, 10), SHADE);
	CHECK_STR(cell_text(0, 23), SHADE);
	CHECK_STR(cell_text(0, 25), SHADE);

	ctui_meter_set_db(&m, -40.0,
			  1.0); /* hold 1.5 s: 1.1 s waited so far... */
	ctui_meter_set_db(&m, -40.0,
			  1.0); /* ...then 0.6 s of fall at 20 dB/s: -24 dB */
	screen_clear();
	ctui_meter_draw(&m);
	CHECK_STR(cell_text(0, 24), SHADE);
	CHECK_STR(cell_text(0, 18), "\xe2\x94\x82");
	m.show_peak = false;
	screen_clear();
	ctui_meter_draw(&m);
	CHECK_STR(cell_text(0, 18), SHADE);

	/* a silent meter draws no marker */
	ctui_meter_init(&m, false);
	ctui_meter_place(&m, 0, 0, 31, 1);
	screen_clear();
	ctui_meter_draw(&m);
	CHECK_STR(cell_text(0, 0), SHADE);
}

static void test_draw_vertical(void)
{
	screen_open(24, 80);
	ctui_meter m;
	ctui_meter_init(&m, true);
	ctui_meter_place(
	    &m, 2, 8, 2,
	    11); /* 10 cells of 6 dB; the clip indicator on row 2 */
	ctui_meter_set_db(&m, -30.0, 0.0); /* 5 cells lit, the peak on cell 5 */
	ctui_meter_set_db(
	    &m, -42.0, 0.1); /* 3 cells lit (rounded), peak stays on cell 5 */
	screen_clear();
	ctui_meter_draw(&m);
	for (int col = 8; col < 10; col++) {
		CHECK_STR(cell_text(12, col), FULL); /* cell 0 at the bottom */
		CHECK_STR(cell_text(10, col), FULL); /* cell 2 */
		CHECK_STR(cell_text(9, col), SHADE); /* cell 3 */
		CHECK_STR(cell_text(7, col),
			  "\xe2\x94\x80"); /* cell 5: the peak, box_h */
		CHECK_STR(cell_text(6, col), SHADE);
		CHECK_STR(cell_text(3, col), SHADE); /* cell 8 */
		CHECK_STR(cell_text(2, col), SHADE); /* clip indicator */
	}
	CHECK_STR(cell_text(13, 8), " ");
	CHECK_STR(cell_text(1, 8), " ");
	CHECK_STR(cell_text(12, 10), " ");
	CHECK(cell_pair(12, 8) == pair_of(CTUI_PAIR_GAUGE_GREEN_BRIGHT, false));
	CHECK(cell_pair(4, 8) ==
	      pair_of(CTUI_PAIR_GAUGE_YELLOW, false)); /* cell 7: -18..-12 */
	CHECK(cell_pair(3, 8) ==
	      pair_of(CTUI_PAIR_GAUGE_RED, false)); /* cell 8: -9 dB */

	ctui_meter_set_db(&m, 3.0, 0.0);
	screen_clear();
	ctui_meter_draw(&m);
	CHECK_STR(cell_text(2, 9), FULL);
	CHECK(cell_pair(2, 9) == pair_of(CTUI_PAIR_GAUGE_RED_BRIGHT, true));
	CHECK_STR(cell_text(3, 9), "\xe2\x94\x80"); /* the peak is there too */
	CHECK_STR(cell_text(4, 9), FULL);
}

static void test_draw_style(void)
{
	screen_open(24, 80);
	ctui_meter m;
	ctui_meter_init(&m, false);
	ctui_meter_place(&m, 0, 0, 11, 1);
	m.show_peak = false;
	ctui_meter_set_db(&m, -30.0, 0.0); /* 5 cells lit */
	ctui_style ascii = {0};
	ascii.glyphs = *ctui_theme_glyphs_for(false);
	ascii.attr[CTUI_PAIR_GAUGE_GREEN_BRIGHT] = ctui_attr_color(16, 214);
	m.style = &ascii;
	screen_clear();
	ctui_meter_draw(&m);
	CHECK_STR(cell_text(0, 0), "#");
	CHECK_STR(cell_text(0, 5), "-");
	CHECK(cell_pair(0, 0) ==
	      (short)PAIR_NUMBER((chtype)ctui_attr_color(16, 214)));
	CHECK(cell_pair(0, 5) ==
	      pair_of(CTUI_PAIR_GAUGE_GREEN,
		      false)); /* the others keep the theme */
	m.show_peak = true;
	ctui_meter_set_db(&m, -60.0, 0.1);
	screen_clear();
	ctui_meter_draw(&m);
	CHECK_STR(cell_text(0, 5), "|"); /* the ASCII marker */

	m.style = NULL; /* and the style does not leak into the next drawing */
	screen_clear();
	ctui_meter_draw(&m);
	CHECK_STR(cell_text(0, 5), "\xe2\x94\x82");
}

static void test_draw_pair(void)
{
	screen_open(24, 80);
	ctui_meter l, r;
	ctui_meter_init(&l, false);
	ctui_meter_init(&r, false);
	l.show_peak = r.show_peak = false;
	ctui_meter_place_pair(&l, &r, 1, 0, 21,
			      3); /* rows 1 and 3, row 2 left empty */
	ctui_meter_set_db(&l, -6.0, 0.0);
	ctui_meter_set_db(&r, -54.0, 0.0);
	screen_clear();
	ctui_meter_draw(&l);
	ctui_meter_draw(&r);
	CHECK_STR(cell_text(1, 17), FULL);  /* left: 18 of 20 cells */
	CHECK_STR(cell_text(3, 17), SHADE); /* right: 2 cells */
	CHECK_STR(cell_text(3, 1), FULL);
	CHECK_STR(cell_text(3, 2), SHADE);
	CHECK_STR(cell_text(2, 0), " ");
}

static void test_draw_degenerate(void)
{
	screen_open(24, 80);
	ctui_meter m;
	ctui_meter_init(&m, false);
	m.show_peak = false;
	ctui_meter_set_db(&m, -10.0, 0.0);
	for (int w = -1; w <= 2; w++) {
		for (int h = -1; h <= 2; h++) {
			ctui_meter_place(&m, 0, 0, w, h);
			screen_clear();
			ctui_meter_draw(
			    &m); /* must not crash or write outside */
		}
	}
	ctui_meter_place(&m, 0, 0, 1,
			 3); /* a clip indicator only is not drawn */
	screen_clear();
	ctui_meter_draw(&m);
	CHECK_STR(cell_text(0, 0), " ");
	ctui_meter_place(&m, 0, 0, 2,
			 1); /* one cell of scale and the indicator */
	screen_clear();
	ctui_meter_draw(&m);
	CHECK_STR(cell_text(0, 0), FULL);
	CHECK_STR(cell_text(0, 1), SHADE);
	m.vertical = true;
	ctui_meter_place(&m, 0, 0, 0, 5);
	screen_clear();
	ctui_meter_draw(&m);
}

int main(void)
{
	RUN(test_db_conversion);
	RUN(test_fraction_and_cells);
	RUN(test_cell_db);
	RUN(test_zones);
	RUN(test_scale_length);
	RUN(test_peak_update);
	RUN(test_peak_update_partial_step);
	RUN(test_init_and_set);
	RUN(test_place_and_pair);
	RUN(test_keys);
	RUN(test_mouse_horizontal);
	RUN(test_mouse_vertical);
	RUN(test_draw_horizontal);
	RUN(test_draw_peak);
	RUN(test_draw_vertical);
	RUN(test_draw_style);
	RUN(test_draw_pair);
	RUN(test_draw_degenerate);
	return CHECK_DONE();
}
