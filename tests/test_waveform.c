/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */
#include "check.h"
#include "screen.h"

#include <curses.h>

#include <ctui/waveform.h>

static float SAMPLES[100 * 2];

static void fixture(ctui_waveform *w)
{
	for (size_t i = 0; i < 100; i++) {
		SAMPLES[i * 2] = (float)i / 100.0f;	 /* left ramps up */
		SAMPLES[i * 2 + 1] = -(float)i / 200.0f; /* right ramps down */
	}
	ctui_waveform_init(w, SAMPLES, 100, 2);
}

static void test_column_ranges(void)
{
	ctui_waveform w;
	fixture(&w);
	size_t a, b;
	ctui_waveform_column(&w, 0, 10, &a, &b);
	CHECK(a == 0 && b == 10);
	ctui_waveform_column(&w, 9, 10, &a, &b);
	CHECK(a == 90 && b == 100);
	ctui_waveform_column(&w, 3, 400, &a,
			     &b); /* more columns than frames: one frame */
	CHECK(b == a + 1);
	w.offset = 50;
	w.span = 50;
	ctui_waveform_column(&w, 0, 10, &a, &b);
	CHECK(a == 50 && b == 55);
}

static void test_peak(void)
{
	ctui_waveform w;
	fixture(&w);
	float lo, hi;
	ctui_waveform_peak(&w, 0, 10, &lo, &hi);
	CHECK_NEAR(hi, 0.09, 1e-6);
	CHECK_NEAR(lo, -0.045, 1e-6);
	ctui_waveform_peak(&w, 90, 200, &lo, &hi); /* clipped to the signal */
	CHECK_NEAR(hi, 0.99, 1e-6);
	CHECK_NEAR(lo, -0.495, 1e-6);
	ctui_waveform_peak(&w, 5, 5, &lo, &hi);
	CHECK(lo == 0 && hi == 0);
}

static void test_x(void)
{
	ctui_waveform w;
	fixture(&w);
	CHECK(ctui_waveform_x(&w, 0, 10) == 0);
	CHECK(ctui_waveform_x(&w, 55, 10) == 5);
	CHECK(ctui_waveform_x(&w, 99, 10) == 9);
	CHECK(ctui_waveform_x(&w, 100, 10) == -1);
	w.offset = 20;
	w.span = 20;
	CHECK(ctui_waveform_x(&w, 19, 10) == -1);
	CHECK(ctui_waveform_x(&w, 30, 10) == 5);
}

static void test_cursor_keys(void)
{
	ctui_waveform w;
	fixture(&w);
	CHECK(ctui_waveform_key(&w, KEY_RIGHT, 10) && w.cursor == 10);
	ctui_waveform_key(&w, KEY_RIGHT, 10);
	CHECK(w.cursor == 20);
	ctui_waveform_key(&w, KEY_LEFT, 10);
	CHECK(w.cursor == 10);
	ctui_waveform_key(&w, KEY_END, 10);
	CHECK(w.cursor == 99);
	ctui_waveform_key(&w, KEY_RIGHT, 10);
	CHECK(w.cursor == 99);
	ctui_waveform_key(&w, KEY_HOME, 10);
	CHECK(w.cursor == 0);
	ctui_waveform_key(&w, KEY_LEFT, 10);
	CHECK(w.cursor == 0);
	CHECK(!ctui_waveform_key(&w, 'x', 10));
}

static void test_zoom(void)
{
	ctui_waveform w;
	fixture(&w);
	ctui_waveform_key(&w, KEY_END, 10);
	ctui_waveform_key(&w, '+', 10);
	CHECK(w.span == 50 && w.offset == 50); /* window follows the cursor */
	ctui_waveform_key(&w, '+', 10);
	CHECK(w.span == 25 && w.offset == 75);
	ctui_waveform_key(&w, '-', 10);
	CHECK(w.span == 50 && w.offset + w.span <= 100);
	ctui_waveform_key(&w, '-', 10);
	CHECK(w.span == 100 && w.offset == 0);
	ctui_waveform_key(&w, '-', 10);
	CHECK(w.span == 100);
	for (int i = 0; i < 20; i++)
		ctui_waveform_key(&w, '+', 10);
	CHECK(w.span == 1); /* zooming stops at one frame */
}

static void test_selection_and_reset(void)
{
	ctui_waveform w;
	fixture(&w);
	w.cursor = 30;
	ctui_waveform_key(&w, '[', 10);
	CHECK(w.selection && w.first == 30);
	w.cursor = 60;
	ctui_waveform_key(&w, ']', 10);
	CHECK(w.end == 61);
	w.cursor = 70;
	ctui_waveform_key(&w, '[', 10); /* start after the end: refused */
	CHECK(w.first == 30);
	w.cursor = 10;
	ctui_waveform_key(&w, ']', 10); /* end before the start: refused */
	CHECK(w.end == 61);
	ctui_waveform_key(&w, 'r', 10);
	CHECK(!w.selection && w.span == 100 && w.cursor == 0 && w.end == 100);
}

static void test_mouse_selection(void)
{
	ctui_waveform w;
	fixture(&w);
	CHECK(!ctui_waveform_drag(&w, 5, 10)); /* no press yet */
	CHECK(!ctui_waveform_release(&w));

	ctui_waveform_press(&w, 3, 10); /* column 3 starts at frame 30 */
	CHECK(w.cursor == 30 && w.dragging && !w.selection);
	CHECK(ctui_waveform_drag(&w, 3, 10));
	CHECK(!w.selection); /* still on the pressed column */
	CHECK(ctui_waveform_drag(&w, 7, 10));
	CHECK(w.selection && w.first == 30 && w.end == 71 && w.cursor == 70);
	CHECK(ctui_waveform_release(&w));
	CHECK(!w.dragging);
	CHECK(!ctui_waveform_drag(&w, 9, 10)); /* the drag is over */
	CHECK(w.end == 71);

	/* leftwards, and past the edges of the view */
	ctui_waveform_press(&w, 5, 10);
	CHECK(ctui_waveform_drag(&w, -4, 10));
	CHECK(w.first == 0 && w.end == 51 && w.cursor == 0);
	CHECK(ctui_waveform_drag(&w, 99, 10));
	CHECK(w.first == 50 && w.end == 91 && w.cursor == 90);
	ctui_waveform_release(&w);

	/* a click moves the cursor and keeps the selection */
	ctui_waveform_press(&w, 1, 10);
	ctui_waveform_release(&w);
	CHECK(w.cursor == 10 && w.first == 50 && w.end == 91);

	/* a zoomed window */
	w.offset = 50;
	w.span = 50;
	ctui_waveform_press(&w, 0, 10);
	CHECK(w.cursor == 50);
	ctui_waveform_drag(&w, 9, 10);
	CHECK(w.first == 50 && w.end == 96);
	ctui_waveform_release(&w);
}

static void test_reset(void)
{
	ctui_waveform w;
	fixture(&w);
	ctui_waveform_press(&w, 2, 10);
	ctui_waveform_drag(&w, 6, 10);
	w.span = 40;
	w.offset = 30;
	ctui_waveform_reset(&w);
	CHECK(!w.selection && !w.dragging);
	CHECK(w.first == 0 && w.end == 100);
	CHECK(w.span == 100 && w.offset == 0 && w.cursor == 0);
}

static void test_filled(void)
{
	screen_open(24, 80);
	static float loud[40], quiet[40];
	for (int i = 0; i < 40; i++) {
		loud[i] = i % 2 ? 1.0f : -1.0f;
		quiet[i] = 0.25f;
	}
	ctui_waveform w;

	/* thin lines by default */
	ctui_waveform_init(&w, loud, 40, 1);
	screen_clear();
	ctui_waveform_draw(&w, 0, 0, 40, 6);
	CHECK(!screen_has_text("\xe2\x96\x88"));

	/* a full scale signal fills every row */
	w.filled = true;
	screen_clear();
	ctui_waveform_draw(&w, 0, 0, 40, 6);
	CHECK(screen_has_text("\xe2\x96\x88"));
	CHECK(!screen_has_text("\xe2\x96\x80") &&
	      !screen_has_text("\xe2\x96\x84"));

	/* a quarter of the scale: one half row above the middle line, which is
	 * a lower half */
	ctui_waveform_init(&w, quiet, 40, 1);
	w.filled = true;
	screen_clear();
	ctui_waveform_draw(&w, 0, 0, 40, 6);
	CHECK(screen_has_text("\xe2\x96\x84"));
	CHECK(!screen_has_text("\xe2\x96\x88"));
	endwin();
}

int main(void)
{
	RUN(test_column_ranges);
	RUN(test_peak);
	RUN(test_x);
	RUN(test_cursor_keys);
	RUN(test_zoom);
	RUN(test_selection_and_reset);
	RUN(test_mouse_selection);
	RUN(test_reset);
	RUN(test_filled);
	return CHECK_DONE();
}
