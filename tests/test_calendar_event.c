/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

#include "check.h"
#include "screen.h"

#include <curses.h>
#include <string.h>

#include <ctui/calendar_event.h>

static void test_values(void)
{
	ctui_cal_event_values v;
	int s = -1, e = -1;

	ctui_cal_event_values_from(&v, "Stand-up", 9 * 60 + 5, 10 * 60, false);
	CHECK_STR(v.title, "Stand-up");
	CHECK_STR(v.start, "09:05");
	CHECK_STR(v.end, "10:00");
	CHECK(ctui_cal_event_values_apply(&v, &s, &e) == CTUI_CAL_EVENT_OK);
	CHECK(s == 545 && e == 600);

	ctui_cal_event_values_from(&v, NULL, -5, 5000, false);
	CHECK_STR(v.title, "");
	CHECK_STR(v.start, "00:00");
	CHECK_STR(v.end, "24:00"); /* clamped to the day */
	CHECK(ctui_cal_event_values_apply(&v, &s, &e) == CTUI_CAL_EVENT_OK);
	CHECK(s == 0 && e == 1440);
}

static void test_check(void)
{
	ctui_cal_event_values v;
	int s = 7, e = 7;

	ctui_cal_event_values_from(&v, "x", 600, 660, false);
	snprintf(v.start, sizeof v.start, "9:00");
	CHECK(ctui_cal_event_values_check(&v) == CTUI_CAL_EVENT_BAD_START);
	snprintf(v.start, sizeof v.start, "24:00");
	CHECK(ctui_cal_event_values_check(&v) == CTUI_CAL_EVENT_BAD_START);
	snprintf(v.start, sizeof v.start, "10:60");
	CHECK(ctui_cal_event_values_check(&v) == CTUI_CAL_EVENT_BAD_START);
	snprintf(v.start, sizeof v.start, "ab:cd");
	CHECK(ctui_cal_event_values_check(&v) == CTUI_CAL_EVENT_BAD_START);
	snprintf(v.start, sizeof v.start, "10:00");
	snprintf(v.end, sizeof v.end, "25:00");
	CHECK(ctui_cal_event_values_check(&v) == CTUI_CAL_EVENT_BAD_END);
	snprintf(v.end, sizeof v.end, "24:01");
	CHECK(ctui_cal_event_values_check(&v) == CTUI_CAL_EVENT_BAD_END);
	snprintf(v.end, sizeof v.end, "10:00");
	CHECK(ctui_cal_event_values_check(&v) == CTUI_CAL_EVENT_END_BEFORE);
	snprintf(v.end, sizeof v.end, "09:59");
	CHECK(ctui_cal_event_values_apply(&v, &s, &e) ==
	      CTUI_CAL_EVENT_END_BEFORE);
	CHECK(s == 7 && e == 7); /* nothing written on an error */
	snprintf(v.end, sizeof v.end, "24:00");
	CHECK(ctui_cal_event_values_apply(&v, &s, &e) == CTUI_CAL_EVENT_OK);
	CHECK(s == 600 && e == 1440);

	/* an all-day event ignores the times */
	snprintf(v.start, sizeof v.start, "zz");
	v.all_day = true;
	CHECK(ctui_cal_event_values_apply(&v, &s, &e) == CTUI_CAL_EVENT_OK);
	CHECK(s == 0 && e == 1440);

	CHECK_STR(ctui_cal_event_error_text(CTUI_CAL_EVENT_OK), "");
	CHECK(strstr(ctui_cal_event_error_text(CTUI_CAL_EVENT_BAD_START),
		     "HH:MM") != NULL);
	CHECK(strstr(ctui_cal_event_error_text(CTUI_CAL_EVENT_END_BEFORE),
		     "after") != NULL);
}

static void test_dialog_keys(void)
{
	ctui_cal_event_dialog d;
	int s = -1, e = -1;

	ctui_cal_event_dialog_init(&d, "Review", 840, 900, false);
	CHECK(d.error == CTUI_CAL_EVENT_OK);
	CHECK(!d.fields[2].disabled && !d.fields[3].disabled);
	CHECK(ctui_cal_event_dialog_key(&d, 27) == CTUI_DIALOG_CANCELLED);

	/* the All day box turns the times off, and the event takes the day */
	ctui_cal_event_dialog_init(&d, "Review", 840, 900, false);
	ctui_cal_event_dialog_key(&d, KEY_DOWN);
	ctui_cal_event_dialog_key(&d, ' ');
	CHECK(d.values.all_day);
	CHECK(d.fields[2].disabled && d.fields[3].disabled);
	CHECK(ctui_cal_event_dialog_result(&d, &s, &e) == CTUI_CAL_EVENT_OK);
	CHECK(s == 0 && e == 1440);
	ctui_cal_event_dialog_key(&d, ' ');
	CHECK(!d.values.all_day && !d.fields[2].disabled);
	CHECK(ctui_cal_event_dialog_result(&d, &s, &e) == CTUI_CAL_EVENT_OK);
	CHECK(s == 840 && e == 900);

	/* an end before the start is reported and refuses OK */
	ctui_timeinput_set(&d.fields[3].time, 13 * 3600000000LL);
	ctui_cal_event_dialog_key(&d, KEY_DOWN); /* any key syncs the check */
	CHECK(d.error == CTUI_CAL_EVENT_END_BEFORE);
	CHECK_STR(d.values.end, "13:00");
	CHECK(ctui_cal_event_dialog_result(&d, &s, &e) ==
	      CTUI_CAL_EVENT_END_BEFORE);
}

static void type(ctui_cal_event_dialog *d, const char *keys)
{
	for (; *keys; keys++)
		ctui_cal_event_dialog_key(d, *keys);
}

static void test_dialog_time_fields(void)
{
	ctui_cal_event_dialog d;
	int s = -1, e = -1;
	ctui_cal_event_dialog_init(&d, "Review", 0, 60, false);
	CHECK(d.fields[2].kind == CTUI_FIELD_TIME &&
	      d.fields[3].kind == CTUI_FIELD_TIME);
	CHECK(d.fields[2].time.precision == CTUI_TIME_MINUTES);
	/* Starts: hour and minute are typed, then Ends */
	ctui_cal_event_dialog_key(&d, KEY_DOWN);
	ctui_cal_event_dialog_key(&d, KEY_DOWN);
	type(&d, "0930");
	CHECK_STR(d.values.start, "09:30");
	ctui_cal_event_dialog_key(&d, KEY_DOWN);
	type(&d, "1015");
	CHECK_STR(d.values.end, "10:15");
	CHECK(ctui_cal_event_dialog_result(&d, &s, &e) == CTUI_CAL_EVENT_OK);
	CHECK(s == 570 && e == 615);
	/* PgUp / PgDn step the segment under the cursor */
	ctui_cal_event_dialog_key(&d, KEY_PPAGE);
	CHECK_STR(d.values.end, "10:16");
	/* an end that is not after the start is refused, Enter included */
	ctui_cal_event_dialog_key(&d, KEY_LEFT);
	type(&d, "0800");
	CHECK(d.error == CTUI_CAL_EVENT_END_BEFORE);
	CHECK(ctui_cal_event_dialog_key(&d, '\n') == CTUI_DIALOG_OPEN);
	ctui_cal_event_dialog_key(&d, KEY_LEFT);
	type(&d, "1130");
	CHECK(d.error == CTUI_CAL_EVENT_OK);
	CHECK(ctui_cal_event_dialog_key(&d, '\n') == CTUI_DIALOG_ACCEPTED);
	/* an event ending with the day is an all-day one: 24:00 becomes 23:59
	 */
	ctui_cal_event_dialog_init(&d, "Late", 1380, 1440, false);
	CHECK_STR(d.values.end, "23:59");
	ctui_cal_event_dialog_init(&d, "Late", 1500, -5, false);
	CHECK_STR(d.values.start, "23:59");
	CHECK_STR(d.values.end, "00:00");
	CHECK(d.error == CTUI_CAL_EVENT_END_BEFORE);
}

static void test_dialog_ways(void)
{
	static const char *const WAYS[] = {"On each day", "Weekly",
					   "One event"};
	ctui_cal_event_dialog d;
	ctui_cal_event_dialog_layout l, l3;
	screen_open(24, 80);
	ctui_cal_event_dialog_init(&d, "Review", 600, 660, false);
	CHECK(d.form.count == 4 && ctui_cal_event_dialog_way(&d) == 0);
	CHECK(ctui_cal_event_dialog_layout_for(&d, LINES, COLS, &l));
	/* no ways, or a single one: no field */
	ctui_cal_event_dialog_set_ways(&d, WAYS, 1);
	CHECK(d.form.count == 4 && d.way_count == 0);
	ctui_cal_event_dialog_set_ways(&d, NULL, 3);
	CHECK(d.form.count == 4);
	/* three ways: a select under the times, and a box one row taller */
	ctui_cal_event_dialog_set_ways(&d, WAYS, 3);
	CHECK(d.form.count == 5 && d.way_count == 3 &&
	      d.fields[4].kind == CTUI_FIELD_SELECT);
	CHECK(ctui_cal_event_dialog_layout_for(&d, LINES, COLS, &l3));
	CHECK(l3.box.height == l.box.height + 1 &&
	      l3.form.height == l.form.height + 1);
	screen_clear();
	ctui_cal_event_dialog_draw(&d);
	CHECK(screen_has_text("Days") && screen_has_text("On each day"));
	/* the first way is chosen; Right on the field picks the next */
	CHECK(ctui_cal_event_dialog_way(&d) == 0);
	for (int i = 0; i < 4; i++)
		ctui_cal_event_dialog_key(&d, KEY_DOWN);
	CHECK(d.form.focus == 4);
	ctui_cal_event_dialog_key(&d, KEY_RIGHT);
	CHECK(ctui_cal_event_dialog_way(&d) == 1);
	ctui_cal_event_dialog_key(&d, KEY_RIGHT);
	ctui_cal_event_dialog_key(&d, KEY_RIGHT); /* stays on the last one */
	CHECK(ctui_cal_event_dialog_way(&d) == 2);
	ctui_cal_event_dialog_key(&d, KEY_LEFT);
	CHECK(ctui_cal_event_dialog_way(&d) == 1);
	/* the times are still the ones of the dialog */
	int s = -1, e = -1;
	CHECK(ctui_cal_event_dialog_result(&d, &s, &e) == CTUI_CAL_EVENT_OK &&
	      s == 600 && e == 660);
	/* hiding the field again */
	ctui_cal_event_dialog_set_ways(&d, NULL, 0);
	CHECK(d.form.count == 4 && ctui_cal_event_dialog_way(&d) == 0);
	endwin();
}

static void test_layout_click_draw(void)
{
	ctui_cal_event_dialog d;
	ctui_cal_event_dialog_layout l;
	ctui_cal_event_dialog_layout small;

	screen_open(24, 80);
	ctui_cal_event_dialog_init(&d, "Review", 840, 900, false);
	d.note = "the same times on each of the 3 days";
	CHECK(ctui_cal_event_dialog_layout_for(&d, LINES, COLS, &l));
	CHECK(l.box.width >= 36 && l.box.height >= 9);
	CHECK(l.ok.y == l.cancel.y && l.ok.x < l.cancel.x);
	CHECK(!ctui_cal_event_dialog_layout_for(&d, 6, 30, &small));

	screen_clear();
	ctui_cal_event_dialog_draw(&d);
	CHECK(screen_has_text("New event"));
	CHECK(screen_has_text("Review"));
	CHECK(screen_has_text("14:00"));
	CHECK(screen_has_text("the same times"));
	CHECK(screen_has_text("[ OK ]") && screen_has_text("[ Cancel ]"));

	CHECK(ctui_cal_event_dialog_click(&d, LINES, COLS, l.ok.y, l.ok.x) ==
	      CTUI_DIALOG_ACCEPTED);
	CHECK(ctui_cal_event_dialog_click(&d, LINES, COLS, l.cancel.y,
					  l.cancel.x) == CTUI_DIALOG_CANCELLED);
	CHECK(ctui_cal_event_dialog_click(&d, LINES, COLS, l.box.y, l.box.x) ==
	      CTUI_DIALOG_OPEN);

	/* OK is refused while the end is not after the start, and the message
	 * shows */
	ctui_timeinput_set(&d.fields[3].time, 13 * 3600000000LL);
	ctui_cal_event_dialog_key(&d, KEY_DOWN);
	CHECK(d.error == CTUI_CAL_EVENT_END_BEFORE);
	CHECK(ctui_cal_event_dialog_click(&d, LINES, COLS, l.ok.y, l.ok.x) ==
	      CTUI_DIALOG_OPEN);
	screen_clear();
	ctui_cal_event_dialog_draw(&d);
	CHECK(screen_has_text("must come after the start"));
	/* a click on the hour of Starts focuses the field */
	ctui_rect form = l.form;
	CHECK(ctui_cal_event_dialog_click(&d, LINES, COLS, form.y + 2,
					  form.x + 9 + 1) == CTUI_DIALOG_OPEN);
	CHECK(d.form.focus == 2);
}

int main(void)
{
	RUN(test_values);
	RUN(test_check);
	RUN(test_dialog_keys);
	RUN(test_dialog_time_fields);
	RUN(test_dialog_ways);
	RUN(test_layout_click_draw);
	return CHECK_DONE();
}
