/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * test_form.c - form fields: values, keys, layout and mouse handling.
 */
#include <curses.h>

#include "check.h"
#include "screen.h"
#include <ctui/form.h>

static const char *const SHADES[] = {"red", "green", "blue", "cyan"};
#define NSHADES (sizeof SHADES / sizeof SHADES[0])

static void test_constructors(void)
{
	char buf[8] = "xx";
	ctui_field t = ctui_field_text("Name", buf, sizeof buf);
	CHECK(t.kind == CTUI_FIELD_TEXT && t.capacity == 8);

	ctui_field s = ctui_field_slider("Volume", 0, 10, 0, 50, -3);
	CHECK_NEAR(s.value, 10, 1e-9); /* clamped */
	CHECK_NEAR(s.step, 0.5, 1e-9); /* a step of 0 means 20 steps */
	CHECK(s.decimals == 0);

	ctui_field c = ctui_field_select("Colour", SHADES, NSHADES, 9);
	CHECK(c.choice == 0); /* out of range */
	ctui_field l = ctui_field_list("Colours", SHADES, NSHADES, 2, 0);
	CHECK(l.choice == 2 && l.rows == 1);
	CHECK(l.first == 2); /* the initial choice is visible */
}

static void test_init_skips_disabled(void)
{
	char a[4] = "", b[4] = "";
	ctui_field fields[] = {ctui_field_text("A", a, 4),
			       ctui_field_text("B", b, 4)};
	fields[0].disabled = true;
	ctui_form f;
	ctui_form_init(&f, fields, 2);
	CHECK(f.focus == 1);
	CHECK(!f.dialog && !f.blurred);
}

static void test_text(void)
{
	char buf[6] = "";
	ctui_field fields[] = {ctui_field_text("Name", buf, sizeof buf)};
	ctui_form f;
	ctui_form_init(&f, fields, 1);
	CHECK(ctui_form_editing(&f));
	CHECK(ctui_form_key(&f, 'a') == CTUI_FORM_CHANGED);
	ctui_form_key(&f, 'b');
	CHECK_STR(buf, "ab");
	for (int i = 0; i < 10; i++)
		ctui_form_key(&f, 'z');
	CHECK(strlen(buf) == 5); /* capacity minus the NUL */
	CHECK(ctui_form_key(&f, 'q') == CTUI_FORM_NONE);
	CHECK(ctui_form_key(&f, KEY_BACKSPACE) == CTUI_FORM_CHANGED);
	CHECK(strlen(buf) == 4);
	CHECK(ctui_form_key(&f, 21) == CTUI_FORM_CHANGED);
	CHECK_STR(buf, "");
	CHECK(ctui_form_key(&f, 21) == CTUI_FORM_NONE);
	CHECK(ctui_form_key(&f, KEY_BACKSPACE) == CTUI_FORM_NONE);
	CHECK(ctui_form_key(&f, '\n') == CTUI_FORM_SUBMITTED);
	CHECK(ctui_form_key(&f, 27) == CTUI_FORM_CANCELLED);
}

static void test_text_backspace_utf8(void)
{
	char buf[16] = "";
	ctui_field fields[] = {ctui_field_text("Name", buf, sizeof buf)};
	ctui_form f;
	ctui_form_init(&f, fields, 1);
	ctui_form_key(&f, 0xC3);
	ctui_form_key(&f, 0xA9); /* e-acute, two bytes */
	CHECK_STR(buf, "\xc3\xa9");
	ctui_form_key(&f, KEY_BACKSPACE);
	CHECK_STR(buf, "");
}

static void test_check(void)
{
	ctui_field fields[] = {
	    ctui_field_check("Enabled", "turn it on", false)};
	ctui_form f;
	ctui_form_init(&f, fields, 1);
	CHECK(!ctui_form_editing(&f));
	CHECK(ctui_form_key(&f, ' ') == CTUI_FORM_CHANGED);
	CHECK(fields[0].checked);
	CHECK(ctui_form_key(&f, ' ') == CTUI_FORM_CHANGED);
	CHECK(!fields[0].checked);
	CHECK(ctui_form_key(&f, 'x') == CTUI_FORM_NONE);
	CHECK(ctui_form_key(&f, '\n') == CTUI_FORM_SUBMITTED);
}

static void test_select_cycles(void)
{
	ctui_field fields[] = {ctui_field_select("Colour", SHADES, NSHADES, 0)};
	ctui_form f;
	ctui_form_init(&f, fields, 1);
	CHECK(ctui_form_key(&f, KEY_LEFT) == CTUI_FORM_NONE);
	CHECK(ctui_form_key(&f, KEY_RIGHT) == CTUI_FORM_CHANGED);
	CHECK(fields[0].choice == 1);
	ctui_form_key(&f, KEY_RIGHT);
	ctui_form_key(&f, KEY_RIGHT);
	CHECK(fields[0].choice == 3);
	CHECK(ctui_form_key(&f, KEY_RIGHT) == CTUI_FORM_NONE);
	CHECK(ctui_form_key(&f, KEY_LEFT) == CTUI_FORM_CHANGED);
	CHECK(fields[0].choice == 2);
}

static void test_select_dropdown(void)
{
	ctui_field fields[] = {ctui_field_select("Colour", SHADES, NSHADES, 1)};
	ctui_form f;
	ctui_form_init(&f, fields, 1);
	CHECK(ctui_form_key(&f, '\n') == CTUI_FORM_MOVED);
	CHECK(f.dropdown && f.pending == 1 && ctui_form_editing(&f));
	ctui_form_key(&f, KEY_DOWN);
	ctui_form_key(&f, KEY_DOWN);
	ctui_form_key(&f, KEY_DOWN);
	CHECK(f.pending == 3);
	CHECK(fields[0].choice == 1); /* not chosen yet */
	ctui_form_key(&f, KEY_HOME);
	CHECK(f.pending == 0);
	CHECK(ctui_form_key(&f, '\n') == CTUI_FORM_CHANGED);
	CHECK(!f.dropdown && fields[0].choice == 0);

	/* Esc closes the dropdown without choosing and without cancelling */
	ctui_form_key(&f, ' ');
	ctui_form_key(&f, KEY_END);
	CHECK(ctui_form_key(&f, 27) == CTUI_FORM_MOVED);
	CHECK(!f.dropdown && fields[0].choice == 0);

	/* choosing the current option is not a change */
	ctui_form_key(&f, ' ');
	CHECK(ctui_form_key(&f, ' ') == CTUI_FORM_MOVED);
}

static void test_slider(void)
{
	ctui_field fields[] = {ctui_field_slider("Volume", 0, 100, 5, 50, 0)};
	ctui_form f;
	ctui_form_init(&f, fields, 1);
	CHECK(ctui_form_key(&f, KEY_RIGHT) == CTUI_FORM_CHANGED);
	CHECK_NEAR(fields[0].value, 55, 1e-9);
	ctui_form_key(&f, KEY_LEFT);
	ctui_form_key(&f, KEY_LEFT);
	CHECK_NEAR(fields[0].value, 45, 1e-9);
	ctui_form_key(&f, KEY_NPAGE);
	CHECK_NEAR(fields[0].value, 95, 1e-9);
	ctui_form_key(&f, KEY_NPAGE);
	CHECK_NEAR(fields[0].value, 100, 1e-9);
	CHECK(ctui_form_key(&f, KEY_RIGHT) == CTUI_FORM_NONE);
	ctui_form_key(&f, KEY_HOME);
	CHECK_NEAR(fields[0].value, 0, 1e-9);
	CHECK(ctui_form_key(&f, KEY_LEFT) == CTUI_FORM_NONE);
	ctui_form_key(&f, KEY_END);
	CHECK_NEAR(fields[0].value, 100, 1e-9);
}

static void test_slider_float_steps(void)
{
	ctui_field fields[] = {ctui_field_slider("Gain", 0, 1, 0.1, 0, 1)};
	ctui_form f;
	ctui_form_init(&f, fields, 1);
	for (int i = 0; i < 3; i++)
		ctui_form_key(&f, KEY_RIGHT);
	CHECK_NEAR(fields[0].value, 0.3, 1e-12); /* snapped, no drift */
}

static void test_list(void)
{
	ctui_field fields[] = {
	    ctui_field_list("Colours", SHADES, NSHADES, 0, 2)};
	ctui_form f;
	ctui_form_init(&f, fields, 1);
	CHECK(ctui_form_key(&f, KEY_UP) == CTUI_FORM_NONE); /* nothing before */
	CHECK(ctui_form_key(&f, KEY_DOWN) == CTUI_FORM_CHANGED);
	CHECK(ctui_form_key(&f, KEY_DOWN) == CTUI_FORM_CHANGED);
	CHECK(fields[0].choice == 2 && fields[0].first == 1);
	ctui_form_key(&f, KEY_END);
	CHECK(fields[0].choice == 3 && fields[0].first == 2);
	ctui_form_key(&f, KEY_HOME);
	CHECK(fields[0].choice == 0 && fields[0].first == 0);
	ctui_form_key(&f, KEY_NPAGE);
	CHECK(fields[0].choice == 2);
	ctui_form_key(&f, KEY_PPAGE);
	CHECK(fields[0].choice == 0);
	CHECK(ctui_form_key(&f, '\n') == CTUI_FORM_SUBMITTED);
}

static void test_focus_moves(void)
{
	char a[4] = "", b[4] = "";
	const char *const two[] = {"one", "two"};
	ctui_field fields[] = {
	    ctui_field_text("A", a, 4),
	    ctui_field_check("B", NULL, false),
	    ctui_field_text("C", b, 4),
	    ctui_field_list("D", two, 2, 0, 2),
	};
	fields[2].disabled = true;
	ctui_form f;
	ctui_form_init(&f, fields, 4);
	CHECK(ctui_form_key(&f, KEY_UP) == CTUI_FORM_NONE);
	CHECK(ctui_form_key(&f, KEY_DOWN) == CTUI_FORM_MOVED && f.focus == 1);
	CHECK(ctui_form_key(&f, KEY_DOWN) == CTUI_FORM_MOVED &&
	      f.focus == 3); /* skips C */
	CHECK(ctui_form_key(&f, KEY_DOWN) ==
	      CTUI_FORM_CHANGED);			      /* list: choice */
	CHECK(ctui_form_key(&f, KEY_DOWN) == CTUI_FORM_NONE); /* last of all */
	CHECK(ctui_form_key(&f, KEY_UP) == CTUI_FORM_CHANGED);
	CHECK(ctui_form_key(&f, KEY_UP) == CTUI_FORM_MOVED &&
	      f.focus == 1); /* leaves the list */
	CHECK(ctui_form_key(&f, KEY_BTAB) == CTUI_FORM_MOVED && f.focus == 0);
	CHECK(ctui_form_key(&f, '\t') == CTUI_FORM_MOVED && f.focus == 1);
}

static void test_rows_and_slots(void)
{
	char a[4] = "";
	ctui_field fields[] = {
	    ctui_field_text("Name", a, 4),
	    ctui_field_check("Enabled", NULL, false),
	    ctui_field_list("Colours", SHADES, NSHADES, 0, 3),
	};
	ctui_form f;
	ctui_form_init(&f, fields, 3);
	CHECK(ctui_form_rows(&f) == 5);

	ctui_rect label, control;
	CHECK(ctui_form_slot(&f, 0, 40, &label, &control));
	CHECK(label.y == 0 && label.x == 0 && label.width == 7);
	CHECK(control.x == 9 && control.width == 31 && control.height == 1);
	CHECK(ctui_form_slot(&f, 2, 40, &label, &control));
	CHECK(label.y == 2 && control.height == 3);
	CHECK(!ctui_form_slot(&f, 3, 40, &label, &control));
	CHECK(!ctui_form_slot(&f, 0, 12, &label, &control)); /* too narrow */
}

static void test_click_check_and_focus(void)
{
	char a[4] = "";
	ctui_field fields[] = {
	    ctui_field_text("Name", a, 4),
	    ctui_field_check("Enabled", "on", false),
	};
	ctui_form f;
	ctui_form_init(&f, fields, 2);
	CHECK(ctui_form_click(&f, 40, 10, 1, 0) ==
	      CTUI_FORM_MOVED); /* on the label */
	CHECK(f.focus == 1 && !fields[1].checked);
	CHECK(ctui_form_click(&f, 40, 10, 1, 12) == CTUI_FORM_CHANGED);
	CHECK(fields[1].checked);
	CHECK(ctui_form_click(&f, 40, 10, 5, 12) ==
	      CTUI_FORM_NONE); /* below the form */
	CHECK(ctui_form_click(&f, 40, 10, 0, 12) == CTUI_FORM_MOVED &&
	      f.focus == 0);
}

static void test_click_slider(void)
{
	ctui_field fields[] = {ctui_field_slider("Volume", 0, 100, 1, 0, 0)};
	ctui_form f;
	ctui_form_init(&f, fields, 1);
	/* control: x = 8, width 32, track = 24 columns */
	CHECK(ctui_form_click(&f, 40, 10, 0, 8) ==
	      CTUI_FORM_NONE); /* already 0 */
	CHECK(ctui_form_click(&f, 40, 10, 0, 8 + 23) == CTUI_FORM_CHANGED);
	CHECK_NEAR(fields[0].value, 100, 1e-9);
	ctui_form_click(&f, 40, 10, 0, 8 + 23 / 2);
	CHECK(fields[0].value > 40 && fields[0].value < 60);
}

static void test_click_list_and_select(void)
{
	ctui_field fields[] = {
	    ctui_field_list("Colours", SHADES, NSHADES, 0, 3),
	    ctui_field_select("Pick", SHADES, NSHADES, 0),
	};
	ctui_form f;
	ctui_form_init(&f, fields, 2);
	CHECK(ctui_form_click(&f, 40, 10, 2, 12) == CTUI_FORM_CHANGED);
	CHECK(fields[0].choice == 2);

	/* open the select, then click the third option of its dropdown */
	CHECK(ctui_form_click(&f, 40, 10, 3, 12) == CTUI_FORM_MOVED);
	CHECK(f.dropdown && f.focus == 1);
	/* the dropdown is framed: border on row 4, options on rows 5..8 */
	CHECK(ctui_form_click(&f, 40, 10, 4, 12) == CTUI_FORM_NONE &&
	      f.dropdown);
	CHECK(ctui_form_click(&f, 40, 10, 7, 12) == CTUI_FORM_CHANGED);
	CHECK(!f.dropdown && fields[1].choice == 2);

	/* a click elsewhere closes it */
	ctui_form_click(&f, 40, 10, 3, 12);
	CHECK(f.dropdown);
	ctui_form_click(&f, 40, 10, 0, 0);
	CHECK(!f.dropdown && f.focus == 0);
}

static void test_scroll_keeps_focus_visible(void)
{
	char buf[8][4] = {{0}};
	ctui_field fields[8];
	for (int i = 0; i < 8; i++)
		fields[i] = ctui_field_text("Field", buf[i], 4);
	ctui_form f;
	ctui_form_init(&f, fields, 8);
	for (int i = 0; i < 7; i++)
		ctui_form_key(&f, KEY_DOWN);
	/* 4 rows visible, focus on the last of 8: the form is scrolled by 4 */
	CHECK(ctui_form_click(&f, 40, 4, 0, 12) == CTUI_FORM_MOVED);
	CHECK(f.focus == 4);
}

#define H 3600000000LL
#define M 60000000LL
#define S 1000000LL

static void keys(ctui_form *f, const char *text)
{
	for (; *text; text++)
		ctui_form_key(f, *text);
}

static void test_input_constructors(void)
{
	ctui_field d = ctui_field_date("Birth", 2024, 2, 29);
	CHECK(d.kind == CTUI_FIELD_DATE && d.date.year == 2024 &&
	      d.date.month == 2 && d.date.day == 29);
	ctui_field bad = ctui_field_date("Birth", 2023, 2, 29);
	CHECK(bad.date.year == 1970 && bad.date.day == 1);
	ctui_field t =
	    ctui_field_time("Start", CTUI_TIME_SECONDS, 9 * H + 30 * M);
	CHECK(t.kind == CTUI_FIELD_TIME && t.time.micro == 9 * H + 30 * M &&
	      t.time.precision == CTUI_TIME_SECONDS);
	ctui_field l = ctui_field_duration("Length", CTUI_DURATION_HOURS,
					   CTUI_DURATION_SECONDS, 90 * M);
	CHECK(l.kind == CTUI_FIELD_DURATION && l.duration.micro == 90 * M &&
	      l.duration.largest == CTUI_DURATION_HOURS);
	CHECK(l.label != NULL && !l.disabled);
}

static void test_date_field(void)
{
	ctui_field fields[] = {ctui_field_date("Birth", 2024, 2, 28),
			       ctui_field_check("Done", "yes", false)};
	ctui_form f;
	ctui_form_init(&f, fields, 2);
	CHECK(ctui_form_editing(&f));
	/* the segment moves, PgUp / PgDn step it, Up / Down leave the field */
	CHECK(ctui_form_key(&f, KEY_RIGHT) == CTUI_FORM_MOVED);
	CHECK(ctui_form_key(&f, KEY_PPAGE) == CTUI_FORM_CHANGED);
	CHECK(fields[0].date.month == 3 && fields[0].date.day == 28);
	CHECK(ctui_form_key(&f, KEY_NPAGE) == CTUI_FORM_CHANGED);
	CHECK(fields[0].date.month == 2);
	CHECK(ctui_form_key(&f, '-') == CTUI_FORM_MOVED &&
	      fields[0].date.segment == CTUI_DATEINPUT_DAY);
	/* '+' and '-' are not steps in a date */
	CHECK(ctui_form_key(&f, '+') == CTUI_FORM_NONE);
	CHECK(ctui_form_key(&f, KEY_LEFT) == CTUI_FORM_MOVED);
	/* typing */
	keys(&f, "07");
	CHECK(fields[0].date.month == 7);
	/* Enter opens the picker, which owns the keys until it closes */
	CHECK(ctui_form_key(&f, '\n') == CTUI_FORM_MOVED &&
	      fields[0].date.open);
	CHECK(ctui_form_editing(&f));
	CHECK(ctui_form_key(&f, KEY_RIGHT) == CTUI_FORM_MOVED);
	CHECK(ctui_form_key(&f, KEY_DOWN) == CTUI_FORM_MOVED); /* a week on */
	CHECK(f.focus == 0);
	CHECK(ctui_form_key(&f, '\n') == CTUI_FORM_CHANGED &&
	      !fields[0].date.open);
	/* the cursor went a day on, then a week: 28 + 1 + 7 days of July */
	CHECK(fields[0].date.month == 8 && fields[0].date.day == 5);
	/* Esc closes the picker without cancelling the form, then cancels it */
	ctui_form_key(&f, '\n');
	CHECK(ctui_form_key(&f, 27) == CTUI_FORM_MOVED && !fields[0].date.open);
	CHECK(ctui_form_key(&f, 27) == CTUI_FORM_CANCELLED);
	/* Up / Down and Tab move between the fields */
	CHECK(ctui_form_key(&f, KEY_DOWN) == CTUI_FORM_MOVED && f.focus == 1);
	CHECK(ctui_form_key(&f, KEY_UP) == CTUI_FORM_MOVED && f.focus == 0);
	CHECK(ctui_form_key(&f, KEY_DOWN) == CTUI_FORM_MOVED && f.focus == 1);
	CHECK(!ctui_form_editing(&f));
}

static void test_time_field(void)
{
	ctui_field fields[] = {ctui_field_time("Start", CTUI_TIME_SECONDS, 0)};
	ctui_form f;
	ctui_form_init(&f, fields, 1);
	CHECK(ctui_form_editing(&f));
	CHECK(ctui_form_key(&f, '0') == CTUI_FORM_MOVED); /* 0 stays 0 */
	CHECK(ctui_form_key(&f, '9') == CTUI_FORM_CHANGED);
	CHECK(fields[0].time.micro == 9 * H &&
	      fields[0].time.segment == CTUI_TIMEINPUT_MINUTE);
	keys(&f, "30");
	CHECK(fields[0].time.micro == 9 * H + 30 * M);
	CHECK(ctui_form_key(&f, '+') == CTUI_FORM_CHANGED &&
	      fields[0].time.micro == 9 * H + 30 * M + S);
	CHECK(ctui_form_key(&f, KEY_PPAGE) == CTUI_FORM_CHANGED &&
	      fields[0].time.micro == 9 * H + 30 * M + 2 * S);
	CHECK(ctui_form_key(&f, '-') == CTUI_FORM_CHANGED);
	CHECK(ctui_form_key(&f, KEY_NPAGE) == CTUI_FORM_CHANGED &&
	      fields[0].time.micro == 9 * H + 30 * M);
	/* the limits of the time are kept */
	long long hi = 9 * H + 30 * M;
	ctui_timeinput_set_limits(&fields[0].time, NULL, &hi);
	CHECK(ctui_form_key(&f, '+') == CTUI_FORM_NONE);
	CHECK(ctui_form_key(&f, '\n') == CTUI_FORM_SUBMITTED);
	CHECK(ctui_form_key(&f, 'x') == CTUI_FORM_NONE);
	CHECK(ctui_form_key(&f, 27) == CTUI_FORM_CANCELLED);
}

static void test_duration_field(void)
{
	ctui_field fields[] = {ctui_field_duration(
	    "Length", CTUI_DURATION_HOURS, CTUI_DURATION_SECONDS, 0)};
	ctui_form f;
	ctui_form_init(&f, fields, 1);
	CHECK(ctui_form_editing(&f));
	keys(&f, "012");
	CHECK(fields[0].duration.micro == 12 * H &&
	      fields[0].duration.segment == CTUI_DURATION_MINUTES);
	CHECK(ctui_form_key(&f, '5') == CTUI_FORM_CHANGED);
	CHECK(ctui_form_key(&f, KEY_RIGHT) == CTUI_FORM_MOVED);
	CHECK(ctui_form_key(&f, '+') == CTUI_FORM_CHANGED &&
	      fields[0].duration.micro == 12 * H + 5 * M + S);
	CHECK(ctui_form_key(&f, KEY_NPAGE) == CTUI_FORM_CHANGED &&
	      fields[0].duration.micro == 12 * H + 5 * M);
	long long max = 12 * H + 5 * M;
	ctui_durationinput_set_max(&fields[0].duration, &max);
	CHECK(ctui_form_key(&f, KEY_PPAGE) == CTUI_FORM_NONE);
	CHECK(ctui_form_key(&f, '\n') == CTUI_FORM_SUBMITTED);
	CHECK(ctui_form_key(&f, 27) == CTUI_FORM_CANCELLED);
	/* a disabled input is skipped */
	ctui_field two[] = {ctui_field_time("A", CTUI_TIME_MINUTES, 0),
			    ctui_field_duration("B", CTUI_DURATION_HOURS,
						CTUI_DURATION_MINUTES, 0)};
	two[0].disabled = true;
	ctui_form_init(&f, two, 2);
	CHECK(f.focus == 1);
}

static void test_input_clicks(void)
{
	ctui_field fields[] = {
	    ctui_field_date("Birth", 2024, 2, 28),
	    ctui_field_time("Start", CTUI_TIME_SECONDS, 0),
	    ctui_field_duration("Length", CTUI_DURATION_HOURS,
				CTUI_DURATION_SECONDS, 0),
	};
	ctui_form f;
	ctui_form_init(&f, fields, 3);
	/* control: x = 8; the date's segments start at column 1 of the field */
	CHECK(ctui_form_click(&f, 60, 12, 0, 8 + 6) == CTUI_FORM_MOVED &&
	      fields[0].date.segment == CTUI_DATEINPUT_MONTH);
	CHECK(ctui_form_click(&f, 60, 12, 0, 8 + 6) == CTUI_FORM_NONE);
	/* the time: focus moves, then the segment */
	CHECK(ctui_form_click(&f, 60, 12, 1, 8 + 7) == CTUI_FORM_MOVED);
	CHECK(f.focus == 1 && fields[1].time.segment == CTUI_TIMEINPUT_SECOND);
	CHECK(ctui_form_click(&f, 60, 12, 1, 8 + 4) == CTUI_FORM_MOVED &&
	      fields[1].time.segment == CTUI_TIMEINPUT_MINUTE);
	CHECK(ctui_form_click(&f, 60, 12, 2, 8 + 10) == CTUI_FORM_MOVED);
	CHECK(f.focus == 2 &&
	      fields[2].duration.segment == CTUI_DURATION_SECONDS);
	CHECK(ctui_form_click(&f, 60, 12, 2, 8 + 6) == CTUI_FORM_MOVED &&
	      fields[2].duration.segment == CTUI_DURATION_MINUTES);
	/* on the label only the focus moves */
	CHECK(ctui_form_click(&f, 60, 12, 0, 0) == CTUI_FORM_MOVED &&
	      f.focus == 0);
	/* the arrow opens the date picker; its days are picked by a click */
	CHECK(ctui_form_click(&f, 60, 12, 0, 8 + 12) == CTUI_FORM_MOVED &&
	      fields[0].date.open);
	int row = 0, col = 0;
	ctui_calendar picker = fields[0].date.picker;
	/* the calendar sits in a frame, one row and one column in */
	for (int r = 0; r < ctui_calendar_height() && !row; r++)
		for (int c = 0; c < ctui_calendar_width() && !row; c++) {
			int day = 0;
			if (ctui_calendar_hit(&picker, r, c, &day) ==
				CTUI_CALENDAR_DAY &&
			    day == 10) {
				row = r + 2;
				col = c + 1;
			}
		}
	CHECK(row > 0);
	CHECK(ctui_form_click(&f, 60, 12, row, 8 + col) == CTUI_FORM_CHANGED);
	CHECK(!fields[0].date.open && fields[0].date.day == 10);
	/* a click elsewhere closes the picker and goes on to what it hit */
	ctui_form_click(&f, 60, 12, 0, 8 + 12);
	CHECK(fields[0].date.open);
	CHECK(ctui_form_click(&f, 60, 12, 1, 0) == CTUI_FORM_MOVED);
	CHECK(!fields[0].date.open && f.focus == 1);
}

static void test_input_draw(void)
{
	screen_open(24, 80);
	ctui_field fields[] = {
	    ctui_field_date("Birth", 2024, 2, 28),
	    ctui_field_time("Start", CTUI_TIME_MILLIS,
			    9 * H + 5 * M + 7 * S + 123000),
	    ctui_field_duration("Length", CTUI_DURATION_HOURS,
				CTUI_DURATION_SECONDS, 90 * M),
	};
	ctui_form f;
	ctui_form_init(&f, fields, 3);
	screen_clear();
	ctui_form_draw(&f, 0, 0, 60, 12);
	CHECK(screen_has_text("2024-02-28") &&
	      screen_has_text("09:05:07.123") &&
	      screen_has_text("001h 30m 00s"));
	/* a narrow control shows the text cut to it */
	screen_clear();
	ctui_form_draw(&f, 0, 0, 20, 12);
	CHECK(screen_has_text("2024-02-2") || screen_has_text("2024-02-"));
	/* an open picker hangs under its field */
	fields[0].date.picker.year = 2024;
	ctui_form_key(&f, '\n');
	CHECK(fields[0].date.open);
	screen_clear();
	ctui_form_draw(&f, 0, 0, 60, 12);
	CHECK(screen_has_text("February 2024") &&
	      screen_has_text("M  T  W  T  F  S  S"));
	/* a disabled input shows a dash and the blurred form no focus */
	fields[1].disabled = true;
	fields[0].date.open = false;
	f.blurred = true;
	screen_clear();
	ctui_form_draw(&f, 0, 0, 60, 12);
	CHECK(!screen_has_text("09:05:07.123"));
	endwin();
}

static void test_text_editor(void)
{
	char buf[16] = "Ada";
	ctui_field fields[] = {ctui_field_text("Name", buf, sizeof buf),
			       ctui_field_text("Other", buf + 8, 8)};
	ctui_form f;
	ctui_form_init(&f, fields, 2);
	/* the cursor starts at the end and moves, typing inserts at it */
	CHECK(ctui_form_key(&f, KEY_LEFT) == CTUI_FORM_MOVED);
	CHECK(ctui_form_key(&f, 'X') == CTUI_FORM_CHANGED);
	CHECK_STR(buf, "AdXa");
	CHECK(ctui_form_key(&f, KEY_HOME) == CTUI_FORM_MOVED);
	ctui_form_key(&f, '>');
	CHECK_STR(buf, ">AdXa");
	CHECK(ctui_form_key(&f, KEY_DC) == CTUI_FORM_NONE ||
	      strcmp(buf, ">dXa") == 0);
	CHECK(ctui_form_key(&f, 5) == CTUI_FORM_MOVED);	   /* Ctrl-E */
	CHECK(ctui_form_key(&f, 21) == CTUI_FORM_CHANGED); /* Ctrl-U */
	CHECK_STR(buf, "");
	/* Enter submits, the settings of the field reach the editor */
	CHECK(ctui_form_key(&f, '\n') == CTUI_FORM_SUBMITTED);
	fields[0].max_chars = 3;
	keys(&f, "abcdef");
	CHECK_STR(buf, "abc");
	fields[0].max_chars = 0;
	fields[0].mask = true;
	fields[0].placeholder = "your name";
	CHECK(f.fields[0].input.cursor == 3);
	/* a click places the cursor: control at column 7, text from column 1 */
	CHECK(ctui_form_click(&f, 40, 4, 0, 7) == CTUI_FORM_MOVED);
	CHECK(f.fields[0].input.cursor == 0);
	CHECK(ctui_form_click(&f, 40, 4, 0, 7) == CTUI_FORM_NONE);
	screen_open(24, 80);
	screen_clear();
	ctui_form_draw(&f, 0, 0, 40, 4);
	CHECK(screen_has_text("***") && !screen_has_text("abc"));
	buf[0] = '\0';
	screen_clear();
	ctui_form_draw(&f, 0, 0, 40, 4);
	CHECK(screen_has_text("your name"));
	endwin();
}

static void test_number_field(void)
{
	ctui_field fields[] = {
	    ctui_field_number("Quantity", 0, 100, 5, 20, 0),
	    ctui_field_number("Price", -10, 10, 0.5, 1.5, 2)};
	fields[0].number.unit = "pcs";
	ctui_form f;
	ctui_form_init(&f, fields, 2);
	CHECK(fields[0].kind == CTUI_FIELD_NUMBER && ctui_form_editing(&f));
	CHECK_NEAR(fields[0].number.value, 20, 1e-9);
	/* PgUp / PgDn add or subtract one step, Up / Down change the field */
	CHECK(ctui_form_key(&f, KEY_PPAGE) == CTUI_FORM_CHANGED);
	CHECK_NEAR(fields[0].number.value, 25, 1e-9);
	CHECK(ctui_form_key(&f, KEY_NPAGE) == CTUI_FORM_CHANGED);
	CHECK(ctui_form_key(&f, KEY_NPAGE) == CTUI_FORM_CHANGED);
	CHECK_NEAR(fields[0].number.value, 15, 1e-9);
	CHECK(ctui_form_key(&f, KEY_END) == CTUI_FORM_CHANGED);
	CHECK_NEAR(fields[0].number.value, 100, 1e-9);
	CHECK(ctui_form_key(&f, KEY_PPAGE) == CTUI_FORM_NONE); /* at the end */
	/* typed digits are validated; leaving the field commits them */
	keys(&f, "7");
	CHECK(fields[0].number.editing);
	CHECK(ctui_form_key(&f, KEY_DOWN) == CTUI_FORM_MOVED && f.focus == 1);
	CHECK(!fields[0].number.editing);
	CHECK_NEAR(fields[0].number.value, 7, 1e-9);
	keys(&f, "-3.25");
	CHECK(ctui_form_key(&f, '\n') == CTUI_FORM_SUBMITTED);
	CHECK_NEAR(fields[1].number.value, -3.25, 1e-9);
	CHECK(ctui_form_key(&f, 'x') == CTUI_FORM_NONE);
	CHECK(ctui_form_key(&f, 27) == CTUI_FORM_CANCELLED);
	/* the buttons of the field: control from column 10, 20 wide at most */
	CHECK(ctui_form_click(&f, 60, 4, 1, 10 + 19) == CTUI_FORM_CHANGED);
	CHECK_NEAR(fields[1].number.value, -2.75, 1e-9);
	CHECK(ctui_form_click(&f, 60, 4, 1, 10) == CTUI_FORM_CHANGED);
	CHECK(ctui_form_click(&f, 60, 4, 1, 10 + 5) == CTUI_FORM_NONE);
	/* clicking another field commits the one being typed */
	ctui_form_key(&f, KEY_UP);
	keys(&f, "42");
	CHECK(fields[0].number.editing);
	CHECK(ctui_form_click(&f, 60, 4, 1, 0) == CTUI_FORM_MOVED);
	CHECK(!fields[0].number.editing);
	CHECK_NEAR(fields[0].number.value, 42, 1e-9);

	screen_open(24, 80);
	screen_clear();
	ctui_form_draw(&f, 0, 0, 60, 4);
	CHECK(screen_has_text("42") && screen_has_text("pcs") &&
	      screen_has_text("-3.25"));
	f.dialog = true;
	screen_clear();
	ctui_form_draw(&f, 0, 0, 60, 4);
	CHECK(screen_has_text("42"));
	endwin();
}

int main(void)
{
	RUN(test_constructors);
	RUN(test_init_skips_disabled);
	RUN(test_text);
	RUN(test_text_backspace_utf8);
	RUN(test_check);
	RUN(test_select_cycles);
	RUN(test_select_dropdown);
	RUN(test_slider);
	RUN(test_slider_float_steps);
	RUN(test_list);
	RUN(test_focus_moves);
	RUN(test_rows_and_slots);
	RUN(test_click_check_and_focus);
	RUN(test_click_slider);
	RUN(test_click_list_and_select);
	RUN(test_scroll_keeps_focus_visible);
	RUN(test_input_constructors);
	RUN(test_date_field);
	RUN(test_time_field);
	RUN(test_duration_field);
	RUN(test_input_clicks);
	RUN(test_input_draw);
	RUN(test_text_editor);
	RUN(test_number_field);
	return CHECK_DONE();
}
