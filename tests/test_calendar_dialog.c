/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * The recurrence dialog: values, validation, application to a rule, the
 * preview, the form inside the dialog and its drawing.
 */
#include "check.h"
#include "screen.h"

#include <ctui/calendar_dialog.h>

static long day(int y, int m, int d)
{
	return ctui_date_to_days(y, m, d);
}

/* Values of a weekly-on-Monday-and-Thursday rule from 2026-11-02, typed in. */
static ctui_cal_recur_values sample(void)
{
	ctui_cal_recur r = {.repeat = CTUI_CAL_REPEAT_WEEKLY,
			    .interval = 2,
			    .weekdays = 1U << 0U | 1U << 3U,
			    .end = CTUI_CAL_END_COUNT,
			    .count = 6};
	ctui_cal_recur_values v;
	ctui_cal_recur_values_from(&v, day(2026, 11, 2), &r);
	return v;
}

static void test_from(void)
{
	ctui_cal_recur_values v = sample();
	CHECK(v.repeat == CTUI_CAL_REPEAT_WEEKLY);
	CHECK_STR(v.interval, "2");
	CHECK(v.weekdays[0] && !v.weekdays[1] && !v.weekdays[2] &&
	      v.weekdays[3] && !v.weekdays[4]);
	CHECK_STR(v.start, "2026-11-02");
	CHECK(v.end == CTUI_CAL_END_COUNT);
	CHECK_STR(v.count, "6");
	CHECK_STR(v.until,
		  "2026-11-02"); /* the start when there is no end date */

	/* until, kept */
	ctui_cal_recur r = {.repeat = CTUI_CAL_REPEAT_DAILY,
			    .interval = 1,
			    .end = CTUI_CAL_END_UNTIL,
			    .until = day(2028, 2, 29)};
	ctui_cal_recur_values_from(&v, day(2026, 11, 2), &r);
	CHECK_STR(v.until, "2028-02-29");
	CHECK_STR(v.count, "10");

	/* no rule: does not repeat, defaults; a weekly rule without days uses
	 * the start's weekday */
	ctui_cal_recur_values_from(&v, day(2026, 11, 5), NULL);
	CHECK(v.repeat == 0 && v.end == 0 && v.weekdays[3] &&
	      !v.weekdays[0]); /* Thursday */
	CHECK_STR(v.interval, "1");
	r = (ctui_cal_recur){.repeat = CTUI_CAL_REPEAT_WEEKLY};
	ctui_cal_recur_values_from(&v, day(2026, 11, 8), &r);
	CHECK(v.weekdays[6] && !v.weekdays[0] && !v.weekdays[3]); /* Sunday */
	CHECK_STR(v.interval, "1"); /* interval 0 is 1 */

	/* a start outside the years is clamped to a valid date text */
	ctui_cal_recur_values_from(&v, day(9999, 12, 31) + 400, NULL);
	CHECK_STR(v.start, "9999-12-31");
	ctui_cal_recur_values_from(&v, day(1, 1, 1) - 400, NULL);
	CHECK_STR(v.start, "0001-01-01");
	CHECK(ctui_cal_recur_values_check(&v) == CTUI_CAL_RECUR_OK);
}

static void test_apply(void)
{
	ctui_cal_recur_values v = sample();
	long start = 0;
	ctui_cal_recur r = {.exception_count = 1,
			    .exceptions = {day(2026, 11, 9)}};
	CHECK(ctui_cal_recur_values_apply(&v, &start, &r) == CTUI_CAL_RECUR_OK);
	CHECK(start == day(2026, 11, 2));
	CHECK(r.repeat == CTUI_CAL_REPEAT_WEEKLY && r.interval == 2 &&
	      r.weekdays == (1U | 1U << 3U));
	CHECK(r.end == CTUI_CAL_END_COUNT && r.count == 6 && r.until == 0);
	CHECK(r.exception_count == 1 &&
	      r.exceptions[0] == day(2026, 11, 9)); /* kept */

	/* until: the count is dropped, the weekdays only count for weekly */
	v.repeat = CTUI_CAL_REPEAT_MONTHLY;
	v.end = CTUI_CAL_END_UNTIL;
	snprintf(v.until, sizeof v.until, "2027-02-28");
	CHECK(ctui_cal_recur_values_apply(&v, &start, &r) == CTUI_CAL_RECUR_OK);
	CHECK(r.repeat == CTUI_CAL_REPEAT_MONTHLY && r.weekdays == 0 &&
	      r.count == 0 && r.until == day(2027, 2, 28));
	v.end = CTUI_CAL_END_NEVER;
	CHECK(ctui_cal_recur_values_apply(&v, &start, &r) == CTUI_CAL_RECUR_OK);
	CHECK(r.end == CTUI_CAL_END_NEVER && r.until == 0 && r.count == 0);

	/* does not repeat clears the rule and its exceptions */
	v.repeat = 0;
	CHECK(ctui_cal_recur_values_apply(&v, NULL, &r) == CTUI_CAL_RECUR_OK);
	CHECK(r.repeat == CTUI_CAL_REPEAT_NONE && r.exception_count == 0 &&
	      r.interval == 0);
	/* even if the other fields are garbage */
	snprintf(v.interval, sizeof v.interval, "abc");
	CHECK(ctui_cal_recur_values_apply(&v, NULL, NULL) == CTUI_CAL_RECUR_OK);

	/* round trip: values from a rule, applied back, give the rule */
	ctui_cal_recur in = {.repeat = CTUI_CAL_REPEAT_YEARLY,
			     .interval = 3,
			     .end = CTUI_CAL_END_UNTIL,
			     .until = day(2040, 2, 29)};
	ctui_cal_recur_values w;
	ctui_cal_recur_values_from(&w, day(2024, 2, 29), &in);
	ctui_cal_recur out = {0};
	CHECK(ctui_cal_recur_values_apply(&w, &start, &out) ==
	      CTUI_CAL_RECUR_OK);
	CHECK(start == day(2024, 2, 29) && out.repeat == in.repeat &&
	      out.interval == 3 && out.end == in.end && out.until == in.until);
}

static void test_nothing_written_on_error(void)
{
	ctui_cal_recur_values v = sample();
	snprintf(v.start, sizeof v.start, "2026-02-30");
	long start = 77;
	ctui_cal_recur r = {.repeat = CTUI_CAL_REPEAT_DAILY, .interval = 9};
	CHECK(ctui_cal_recur_values_apply(&v, &start, &r) ==
	      CTUI_CAL_RECUR_BAD_START);
	CHECK(start == 77 && r.repeat == CTUI_CAL_REPEAT_DAILY &&
	      r.interval == 9);
}

static void set_date(char *field, const char *text)
{
	snprintf(field, 11, "%s", text);
}

static void test_dates(void)
{
	ctui_cal_recur_values v = sample();
	static const struct {
		const char *text;
		bool ok;
	} STARTS[] = {
	    {"2026-11-02", true},  {"2024-02-29", true},  {"2026-02-29", false},
	    {"2000-02-29", true},  {"1900-02-29", false}, {"2100-02-29", false},
	    {"2026-04-31", false}, {"2026-04-30", true},  {"2026-12-31", true},
	    {"2026-13-01", false}, {"2026-00-10", false}, {"2026-01-00", false},
	    {"0001-01-01", true},  {"0000-12-31", false}, {"9999-12-31", true},
	    {"10000-1-1", false},  {"2026-1-5", false},	  {"26-01-05", false},
	    {"2026/01/05", false}, {"2026-01-0", false},  {" 2026-01-0", false},
	    {"", false},	   {"abcd-ef-gh", false}, {"2026-11-3x", false},
	};
	for (size_t i = 0; i < sizeof STARTS / sizeof STARTS[0]; i++) {
		set_date(v.start, STARTS[i].text);
		set_date(v.until, "9999-12-31");
		v.end = CTUI_CAL_END_UNTIL;
		ctui_cal_recur_error e = ctui_cal_recur_values_check(&v);
		if (STARTS[i].ok != (e == CTUI_CAL_RECUR_OK))
			fprintf(stderr, "start \"%s\" -> %d\n", STARTS[i].text,
				(int)e);
		CHECK(STARTS[i].ok == (e == CTUI_CAL_RECUR_OK));
		CHECK(STARTS[i].ok || e == CTUI_CAL_RECUR_BAD_START);
	}
	/* the end date is checked the same way */
	set_date(v.start, "2024-01-01");
	for (size_t i = 0; i < sizeof STARTS / sizeof STARTS[0]; i++) {
		set_date(v.until, STARTS[i].text);
		ctui_cal_recur_error e = ctui_cal_recur_values_check(&v);
		if (STARTS[i].ok)
			CHECK(e == CTUI_CAL_RECUR_OK ||
			      e == CTUI_CAL_RECUR_UNTIL_BEFORE);
		else
			CHECK(e == CTUI_CAL_RECUR_BAD_UNTIL);
	}
	/* until before the start is refused, the same day is fine */
	set_date(v.start, "2026-11-02");
	set_date(v.until, "2026-11-01");
	CHECK(ctui_cal_recur_values_check(&v) == CTUI_CAL_RECUR_UNTIL_BEFORE);
	set_date(v.until, "2026-11-02");
	CHECK(ctui_cal_recur_values_check(&v) == CTUI_CAL_RECUR_OK);
	/* leap day starts and ends of months */
	set_date(v.start, "2028-02-29");
	set_date(v.until, "2028-02-28");
	CHECK(ctui_cal_recur_values_check(&v) == CTUI_CAL_RECUR_UNTIL_BEFORE);
	set_date(v.until, "2028-03-01");
	CHECK(ctui_cal_recur_values_check(&v) == CTUI_CAL_RECUR_OK);
	/* the end date does not count when the rule never ends or counts */
	set_date(v.until, "not a date");
	v.end = CTUI_CAL_END_NEVER;
	CHECK(ctui_cal_recur_values_check(&v) == CTUI_CAL_RECUR_OK);
	v.end = CTUI_CAL_END_COUNT;
	CHECK(ctui_cal_recur_values_check(&v) == CTUI_CAL_RECUR_OK);
}

static void test_numbers(void)
{
	ctui_cal_recur_values v = sample();
	static const struct {
		const char *text;
		bool ok;
	} NUMBERS[] = {{"1", true},	 {"2", true},	 {"10000", true},
		       {"10001", false}, {"0", false},	 {"", false},
		       {"-1", false},	 {"1.5", false}, {"1x", false},
		       {"x", false},	 {" 3 ", true},	 {"99999", false},
		       {"007", true},	 {"   ", false}};
	for (size_t i = 0; i < sizeof NUMBERS / sizeof NUMBERS[0]; i++) {
		snprintf(v.interval, sizeof v.interval, "%s", NUMBERS[i].text);
		ctui_cal_recur_error e = ctui_cal_recur_values_check(&v);
		CHECK(NUMBERS[i].ok ? e == CTUI_CAL_RECUR_OK
				    : e == CTUI_CAL_RECUR_BAD_INTERVAL);
	}
	/* the interval does not matter when it does not repeat */
	v.repeat = 0;
	snprintf(v.interval, sizeof v.interval, "0");
	CHECK(ctui_cal_recur_values_check(&v) == CTUI_CAL_RECUR_OK);

	v = sample();
	v.end = CTUI_CAL_END_COUNT;
	static const struct {
		const char *text;
		bool ok;
	} COUNTS[] = {{"1", true},	  {"6", true},	      {"0", false},
		      {"", false},	  {"-3", false},      {"1000000", true},
		      {"1000001", false}, {"9999999", false}, {"2a", false}};
	for (size_t i = 0; i < sizeof COUNTS / sizeof COUNTS[0]; i++) {
		snprintf(v.count, sizeof v.count, "%s", COUNTS[i].text);
		ctui_cal_recur_error e = ctui_cal_recur_values_check(&v);
		CHECK(COUNTS[i].ok ? e == CTUI_CAL_RECUR_OK
				   : e == CTUI_CAL_RECUR_BAD_COUNT);
	}
	/* the count does not matter for another end */
	v.end = CTUI_CAL_END_NEVER;
	snprintf(v.count, sizeof v.count, "0");
	CHECK(ctui_cal_recur_values_check(&v) == CTUI_CAL_RECUR_OK);
}

static void test_weekdays(void)
{
	ctui_cal_recur_values v = sample();
	for (int i = 0; i < 7; i++)
		v.weekdays[i] = false;
	CHECK(ctui_cal_recur_values_check(&v) == CTUI_CAL_RECUR_NO_WEEKDAY);
	long start;
	ctui_cal_recur r = {0};
	CHECK(ctui_cal_recur_values_apply(&v, &start, &r) ==
		  CTUI_CAL_RECUR_NO_WEEKDAY &&
	      r.repeat == 0);
	v.repeat = CTUI_CAL_REPEAT_DAILY; /* weekdays only matter for weekly */
	CHECK(ctui_cal_recur_values_check(&v) == CTUI_CAL_RECUR_OK);
	v.repeat = CTUI_CAL_REPEAT_WEEKLY;
	v.weekdays[6] = true;
	CHECK(ctui_cal_recur_values_apply(&v, &start, &r) ==
		  CTUI_CAL_RECUR_OK &&
	      r.weekdays == 1U << 6U);
	/* the first error wins: start, interval, weekdays, end */
	v.weekdays[6] = false;
	snprintf(v.interval, sizeof v.interval, "0");
	CHECK(ctui_cal_recur_values_check(&v) == CTUI_CAL_RECUR_BAD_INTERVAL);
	set_date(v.start, "2026-02-30");
	CHECK(ctui_cal_recur_values_check(&v) == CTUI_CAL_RECUR_BAD_START);
}

static void test_error_text(void)
{
	CHECK_STR(ctui_cal_recur_error_text(CTUI_CAL_RECUR_OK), "");
	for (int e = 1; e <= CTUI_CAL_RECUR_BAD_COUNT; e++)
		CHECK(strlen(ctui_cal_recur_error_text(
			  (ctui_cal_recur_error)e)) > 10);
	CHECK(strstr(ctui_cal_recur_error_text(CTUI_CAL_RECUR_UNTIL_BEFORE),
		     "before") != NULL);
	CHECK(strstr(ctui_cal_recur_error_text(CTUI_CAL_RECUR_NO_WEEKDAY),
		     "weekday") != NULL);
}

static void test_preview(void)
{
	ctui_cal_recur_values v = sample();
	char text[128];
	CHECK(ctui_cal_recur_values_preview(&v, text, sizeof text));
	/* every 2 weeks on Monday and Thursday from Monday 2026-11-02 */
	CHECK_STR(text, "Next: 2026-11-02, 2026-11-05, 2026-11-16");
	v.repeat = 0;
	CHECK(ctui_cal_recur_values_preview(&v, text, sizeof text));
	CHECK_STR(text, "Does not repeat");
	/* monthly on the 31st skips the short months; a leap day yearly skips
	 * years */
	v = sample();
	v.repeat = CTUI_CAL_REPEAT_MONTHLY;
	v.end = CTUI_CAL_END_NEVER;
	snprintf(v.interval, sizeof v.interval, "1");
	set_date(v.start, "2026-01-31");
	CHECK(ctui_cal_recur_values_preview(&v, text, sizeof text));
	CHECK_STR(text, "Next: 2026-01-31, 2026-03-31, 2026-05-31");
	v.repeat = CTUI_CAL_REPEAT_YEARLY;
	set_date(v.start, "2024-02-29");
	CHECK(ctui_cal_recur_values_preview(&v, text, sizeof text));
	CHECK_STR(text, "Next: 2024-02-29, 2028-02-29, 2032-02-29");
	/* fewer than three */
	v.end = CTUI_CAL_END_COUNT;
	snprintf(v.count, sizeof v.count, "2");
	CHECK(ctui_cal_recur_values_preview(&v, text, sizeof text));
	CHECK_STR(text, "Next: 2024-02-29, 2028-02-29");
	/* invalid values give nothing */
	snprintf(v.count, sizeof v.count, "0");
	CHECK(!ctui_cal_recur_values_preview(&v, text, sizeof text) &&
	      text[0] == '\0');
	/* a small buffer is cut, never overrun */
	v = sample();
	char small[12];
	CHECK(ctui_cal_recur_values_preview(&v, small, sizeof small) &&
	      strlen(small) < sizeof small);
	CHECK(!ctui_cal_recur_values_preview(&v, small, 0));
}

static void type(ctui_cal_recur_dialog *d, const char *s)
{
	for (; *s; s++)
		ctui_cal_recur_dialog_key(d, (unsigned char)*s);
}

static void test_dialog_keys(void)
{
	ctui_cal_recur_dialog d;
	ctui_cal_recur_dialog_init(&d, day(2026, 11, 2), NULL);
	CHECK(d.error == CTUI_CAL_RECUR_OK && d.form.focus == 0);
	/* a repeat of none leaves the other fields out of the way */
	CHECK(d.fields[1].disabled && d.fields[2].disabled &&
	      d.fields[10].disabled);
	CHECK(ctui_cal_recur_dialog_editing(&d));

	/* Right on the select picks daily: the interval is open, the weekdays
	 * are not */
	CHECK(ctui_cal_recur_dialog_key(&d, KEY_RIGHT) == CTUI_DIALOG_OPEN);
	CHECK(d.values.repeat == CTUI_CAL_REPEAT_DAILY &&
	      !d.fields[1].disabled && d.fields[2].disabled);
	CHECK(!d.fields[10].disabled && d.fields[11].disabled &&
	      d.fields[12].disabled);
	ctui_cal_recur_dialog_key(&d, KEY_RIGHT);
	CHECK(d.values.repeat == CTUI_CAL_REPEAT_WEEKLY &&
	      !d.fields[2].disabled);

	/* Down to the interval, which is replaced by a bad number: the error
	 * shows and OK is refused */
	ctui_cal_recur_dialog_key(&d, KEY_DOWN);
	CHECK(d.form.focus == 1);
	ctui_cal_recur_dialog_key(&d, KEY_BACKSPACE);
	type(&d, "0");
	CHECK(d.error == CTUI_CAL_RECUR_BAD_INTERVAL);
	CHECK(ctui_cal_recur_dialog_key(&d, '\n') == CTUI_DIALOG_OPEN);
	CHECK(d.error == CTUI_CAL_RECUR_BAD_INTERVAL);
	ctui_cal_recur_dialog_key(&d, KEY_BACKSPACE);
	type(&d, "3");
	CHECK(d.error == CTUI_CAL_RECUR_OK);

	/* OK applies the values */
	CHECK(ctui_cal_recur_dialog_key(&d, '\n') == CTUI_DIALOG_ACCEPTED);
	long start = 0;
	ctui_cal_recur r = {0};
	CHECK(ctui_cal_recur_dialog_result(&d, &start, &r) ==
	      CTUI_CAL_RECUR_OK);
	CHECK(start == day(2026, 11, 2) && r.repeat == CTUI_CAL_REPEAT_WEEKLY &&
	      r.interval == 3 &&
	      r.weekdays == 1U); /* Monday: the start's weekday */

	CHECK(ctui_cal_recur_dialog_key(&d, 27) == CTUI_DIALOG_CANCELLED);
}

static void test_dialog_weekly(void)
{
	ctui_cal_recur r = {.repeat = CTUI_CAL_REPEAT_WEEKLY,
			    .interval = 1,
			    .weekdays = 1U << 2U};
	ctui_cal_recur_dialog d;
	ctui_cal_recur_dialog_init(&d, day(2026, 11, 4), &r); /* Wednesday */
	CHECK(d.form.focus == 0 && d.values.weekdays[2]);
	ctui_cal_recur_dialog_key(&d, KEY_DOWN); /* interval */
	ctui_cal_recur_dialog_key(&d, KEY_DOWN); /* Monday */
	ctui_cal_recur_dialog_key(&d, KEY_DOWN); /* Tuesday */
	ctui_cal_recur_dialog_key(&d, KEY_DOWN); /* Wednesday */
	CHECK(d.form.focus == 4);
	ctui_cal_recur_dialog_key(&d, ' '); /* uncheck: no weekday left */
	CHECK(!d.values.weekdays[2] && d.error == CTUI_CAL_RECUR_NO_WEEKDAY);
	CHECK(ctui_cal_recur_dialog_key(&d, '\n') == CTUI_DIALOG_OPEN);
	ctui_cal_recur_dialog_key(&d, KEY_UP); /* Tuesday */
	ctui_cal_recur_dialog_key(&d, ' ');
	CHECK(d.values.weekdays[1] && d.error == CTUI_CAL_RECUR_OK);
	CHECK(ctui_cal_recur_dialog_key(&d, '\n') == CTUI_DIALOG_ACCEPTED);
	ctui_cal_recur out = {0};
	CHECK(ctui_cal_recur_dialog_result(&d, NULL, &out) ==
		  CTUI_CAL_RECUR_OK &&
	      out.weekdays == 2U);
}

static void test_dialog_end(void)
{
	ctui_cal_recur_dialog d;
	ctui_cal_recur_dialog_init(&d, day(2026, 11, 2), NULL);
	ctui_cal_recur_dialog_key(&d, KEY_RIGHT); /* daily */
	/* Ends: Tab to the field (every, start, ends), and pick "On a date" */
	ctui_cal_recur_dialog_key(&d, '\t');
	CHECK(d.form.focus == 1); /* every */
	ctui_cal_recur_dialog_key(&d, '\t');
	CHECK(d.form.focus ==
	      9); /* start date: the weekday checks are disabled */
	ctui_cal_recur_dialog_key(&d, '\t');
	CHECK(d.form.focus == 10);
	ctui_cal_recur_dialog_key(&d, KEY_RIGHT);
	CHECK(d.values.end == CTUI_CAL_END_UNTIL && !d.fields[11].disabled &&
	      d.fields[12].disabled);
	/* the end date equals the start: fine; type an earlier day: refused */
	ctui_cal_recur_dialog_key(&d, '\t');
	CHECK(d.form.focus == 11);
	for (int i = 0; i < 10; i++)
		ctui_cal_recur_dialog_key(&d, KEY_BACKSPACE);
	type(&d, "2026-10-31");
	CHECK(d.error == CTUI_CAL_RECUR_UNTIL_BEFORE);
	CHECK(ctui_cal_recur_dialog_key(&d, '\n') == CTUI_DIALOG_OPEN);
	for (int i = 0; i < 10; i++)
		ctui_cal_recur_dialog_key(&d, KEY_BACKSPACE);
	type(&d, "2026-11-30");
	CHECK(d.error == CTUI_CAL_RECUR_OK);
	CHECK(ctui_cal_recur_dialog_key(&d, '\n') == CTUI_DIALOG_ACCEPTED);
	ctui_cal_recur out = {0};
	CHECK(ctui_cal_recur_dialog_result(&d, NULL, &out) ==
	      CTUI_CAL_RECUR_OK);
	CHECK(out.end == CTUI_CAL_END_UNTIL && out.until == day(2026, 11, 30));
}

static void test_layout_click(void)
{
	ctui_cal_recur_dialog_layout l;
	CHECK(!ctui_cal_recur_dialog_layout_for(NULL, 9, 30, &l));
	CHECK(!ctui_cal_recur_dialog_layout_for(NULL, 40, 41, &l) ||
	      l.box.width >= 40);
	CHECK(ctui_cal_recur_dialog_layout_for(NULL, 30, 100, &l));
	CHECK(l.box.width == 56 && l.box.height == 18 && l.box.x == 22 &&
	      l.box.y == 6);
	CHECK(l.form.height == 13 && l.form.width == 52);
	CHECK(l.error_y == l.box.y + l.box.height - 4 &&
	      l.preview_y == l.error_y + 1);
	CHECK(l.ok.y == l.cancel.y && l.ok.y == l.box.y + l.box.height - 2 &&
	      l.ok.x < l.cancel.x);
	CHECK(l.ok.x >= l.box.x &&
	      l.cancel.x + l.cancel.width <= l.box.x + l.box.width);
	/* a short terminal still gives a dialog; its form scrolls */
	CHECK(ctui_cal_recur_dialog_layout_for(NULL, 14, 80, &l) &&
	      l.form.height == l.box.height - 5);

	ctui_cal_recur_dialog d;
	ctui_cal_recur_dialog_init(&d, day(2026, 11, 2), NULL);
	ctui_cal_recur_dialog_layout_for(NULL, 30, 100, &l);
	CHECK(ctui_cal_recur_dialog_click(&d, 30, 100, l.cancel.y,
					  l.cancel.x + 1) ==
	      CTUI_DIALOG_CANCELLED);
	CHECK(ctui_cal_recur_dialog_click(&d, 30, 100, l.ok.y, l.ok.x + 1) ==
	      CTUI_DIALOG_ACCEPTED);
	CHECK(ctui_cal_recur_dialog_click(&d, 30, 100, l.box.y,
					  l.box.x + l.box.width - 2) ==
	      CTUI_DIALOG_CANCELLED); /* the close button */
	CHECK(ctui_cal_recur_dialog_click(&d, 30, 100, 0, 0) ==
	      CTUI_DIALOG_OPEN);
	/* a click on the select's arrow area opens it */
	CHECK(ctui_cal_recur_dialog_click(&d, 30, 100, l.form.y,
					  l.form.x + 20) == CTUI_DIALOG_OPEN);
	/* OK with a bad value is refused */
	d.values.repeat = CTUI_CAL_REPEAT_DAILY;
	snprintf(d.values.interval, sizeof d.values.interval, "x");
	d.error = ctui_cal_recur_values_check(&d.values);
	CHECK(ctui_cal_recur_dialog_click(&d, 30, 100, l.ok.y, l.ok.x + 1) ==
	      CTUI_DIALOG_OPEN);
}

static void test_draw(void)
{
	screen_open(30, 100);
	ctui_cal_recur_dialog d;
	ctui_cal_recur r = {.repeat = CTUI_CAL_REPEAT_WEEKLY,
			    .interval = 2,
			    .weekdays = 1U | 1U << 3U};
	ctui_cal_recur_dialog_init(&d, day(2026, 11, 2), &r);
	screen_clear();
	ctui_cal_recur_dialog_draw(&d);
	CHECK(screen_has_text("Repeat") && screen_has_text("Weekly") &&
	      screen_has_text("Monday"));
	CHECK(screen_has_text("Start date") && screen_has_text("2026-11-02"));
	CHECK(screen_has_text("Next: 2026-11-02, 2026-11-05, 2026-11-16"));
	CHECK(screen_has_text("[ OK ]") && screen_has_text("[ Cancel ]"));

	/* an error replaces the preview and is drawn in the error colour */
	snprintf(d.values.interval, sizeof d.values.interval, "0");
	d.error = ctui_cal_recur_values_check(&d.values);
	screen_clear();
	ctui_cal_recur_dialog_draw(&d);
	CHECK(screen_has_text(
	    ctui_cal_recur_error_text(CTUI_CAL_RECUR_BAD_INTERVAL)));
	CHECK(!screen_has_text("Next:"));
	CHECK(screen_has_color(ctui_attr(CTUI_PAIR_NOTICE_ERROR, true)));

	/* custom button words */
	d.ok = "Apply";
	ctui_labels words = {.cancel = "Annuler"};
	d.labels = &words;
	screen_clear();
	ctui_cal_recur_dialog_draw(&d);
	CHECK(screen_has_text("[ Apply ]") && screen_has_text("[ Annuler ]"));

	/* too small a screen */
	resizeterm(8, 30);
	screen_clear();
	ctui_cal_recur_dialog_draw(&d);
	CHECK(screen_has_text("Resize"));
}

int main(void)
{
	RUN(test_from);
	RUN(test_apply);
	RUN(test_nothing_written_on_error);
	RUN(test_dates);
	RUN(test_numbers);
	RUN(test_weekdays);
	RUN(test_error_text);
	RUN(test_preview);
	RUN(test_dialog_keys);
	RUN(test_dialog_weekly);
	RUN(test_dialog_end);
	RUN(test_layout_click);
	RUN(test_draw);
	return CHECK_DONE();
}
