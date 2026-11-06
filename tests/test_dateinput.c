/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * test_dateinput.c - date field: validation, parse / format, segment
 * editing, the month picker, mouse and drawing.
 */
#include "check.h"
#include "screen.h"

#include <curses.h>

#include <ctui/dateinput.h>

static void is_date(const ctui_dateinput *in, int y, int m, int d)
{
	ctui_date v = ctui_dateinput_date(in);
	CHECK(v.year == y && v.month == m && v.day == d);
	CHECK(ctui_dateinput_valid(v.year, v.month, v.day));
}

static void type(ctui_dateinput *in, const char *digits)
{
	for (; *digits; digits++)
		ctui_dateinput_key(in, *digits);
}

static void test_valid(void)
{
	CHECK(ctui_dateinput_valid(2024, 2, 29));
	CHECK(!ctui_dateinput_valid(2023, 2, 29));
	CHECK(!ctui_dateinput_valid(1900, 2, 29)); /* century, not leap */
	CHECK(ctui_dateinput_valid(2000, 2, 29));  /* every 400 years */
	CHECK(ctui_dateinput_valid(2026, 4, 30) &&
	      !ctui_dateinput_valid(2026, 4, 31));
	CHECK(ctui_dateinput_valid(2026, 12, 31));
	CHECK(!ctui_dateinput_valid(2026, 0, 1) &&
	      !ctui_dateinput_valid(2026, 13, 1));
	CHECK(!ctui_dateinput_valid(2026, 1, 0) &&
	      !ctui_dateinput_valid(2026, 1, 32));
	CHECK(ctui_dateinput_valid(1, 1, 1) &&
	      ctui_dateinput_valid(9999, 12, 31));
	CHECK(!ctui_dateinput_valid(0, 1, 1) &&
	      !ctui_dateinput_valid(10000, 1, 1));
	CHECK(!ctui_dateinput_valid(-5, 1, 1));
}

static void test_parse(void)
{
	ctui_date d = {7, 7, 7};
	CHECK(ctui_dateinput_parse("2026-11-02", &d) && d.year == 2026 &&
	      d.month == 11 && d.day == 2);
	CHECK(ctui_dateinput_parse("0001-01-01", &d) && d.year == 1);
	CHECK(ctui_dateinput_parse("9999-12-31", &d) && d.year == 9999);
	CHECK(ctui_dateinput_parse("2024-02-29", NULL));
	d = (ctui_date){7, 7, 7};
	const char *bad[] = {
	    "",		  "2026-11-2",	  "2026-1-02",	 "26-11-02",
	    "2026/11/02", "2026-11-02 ",  " 2026-11-02", "2026-11-022",
	    "+026-11-02", "2026-11-0a",	  "2023-02-29",	 "0000-01-01",
	    "2026-13-01", "2026-00-10",	  "2026-04-31",	 "2026-11-00",
	    "-001-01-01", "2026-11-02\n", "2026--11-02", "20261102"};
	for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
		CHECK(!ctui_dateinput_parse(bad[i], &d));
		CHECK(d.year == 7 && d.month == 7 &&
		      d.day == 7); /* untouched */
	}
	CHECK(!ctui_dateinput_parse(NULL, &d));
}

static void test_format(void)
{
	char buf[CTUI_DATEINPUT_TEXT];
	CHECK_STR(
	    ctui_dateinput_format((ctui_date){2026, 11, 2}, buf, sizeof buf),
	    "2026-11-02");
	CHECK_STR(ctui_dateinput_format((ctui_date){1, 1, 1}, buf, sizeof buf),
		  "0001-01-01");
	CHECK_STR(
	    ctui_dateinput_format((ctui_date){9999, 12, 31}, buf, sizeof buf),
	    "9999-12-31");
	CHECK(ctui_dateinput_format((ctui_date){2023, 2, 29}, buf,
				    sizeof buf) == NULL);
	CHECK(ctui_dateinput_format((ctui_date){2026, 1, 1}, buf, 10) ==
	      NULL); /* no room for the NUL */
	CHECK(ctui_dateinput_format((ctui_date){2026, 1, 1}, NULL, 11) == NULL);
	/* parse and format round-trip over a span of days */
	for (long days = -719162; days <= 2932896; days += 997) {
		ctui_date d = ctui_date_from_days(days), back;
		CHECK(ctui_dateinput_format(d, buf, sizeof buf) != NULL);
		CHECK(ctui_dateinput_parse(buf, &back) &&
		      ctui_date_to_days(back.year, back.month, back.day) ==
			  days);
	}
}

static void test_days(void)
{
	ctui_dateinput in;
	ctui_dateinput_init(&in, 1970, 1, 1);
	CHECK(ctui_dateinput_days(&in) == 0);
	ctui_dateinput_init(&in, 2000, 3, 1);
	CHECK(ctui_dateinput_days(&in) == ctui_date_to_days(2000, 3, 1));
	CHECK(ctui_dateinput_set_days(&in, 20000));
	ctui_date d = ctui_date_from_days(20000);
	is_date(&in, d.year, d.month, d.day);
	CHECK(ctui_dateinput_min_days() == ctui_date_to_days(1, 1, 1));
	CHECK(ctui_dateinput_max_days() == ctui_date_to_days(9999, 12, 31));
	CHECK(ctui_dateinput_set_days(&in, ctui_dateinput_min_days()));
	is_date(&in, 1, 1, 1);
	CHECK(!ctui_dateinput_set_days(&in, ctui_dateinput_min_days() - 1));
	is_date(&in, 1, 1, 1);
	CHECK(ctui_dateinput_set_days(&in, ctui_dateinput_max_days()));
	is_date(&in, 9999, 12, 31);
	CHECK(!ctui_dateinput_set_days(&in, ctui_dateinput_max_days() + 1));
	CHECK(!ctui_dateinput_set_days(&in, 0x7fffffffffffffffL) &&
	      !ctui_dateinput_set_days(&in, -0x7fffffffffffffffL));
	CHECK(!ctui_dateinput_set(&in, 2023, 2, 29));
	is_date(&in, 9999, 12, 31);
	ctui_dateinput_init(&in, 2023, 2, 30); /* invalid: 1970-01-01 */
	is_date(&in, 1970, 1, 1);
}

static void test_arrows(void)
{
	ctui_dateinput in;
	ctui_dateinput_init(&in, 2026, 11, 2);
	CHECK(in.segment == CTUI_DATEINPUT_YEAR);
	CHECK(ctui_dateinput_key(&in, KEY_LEFT) == CTUI_DATEINPUT_NONE);
	CHECK(ctui_dateinput_key(&in, KEY_UP) == CTUI_DATEINPUT_CHANGED);
	is_date(&in, 2027, 11, 2);
	CHECK(ctui_dateinput_key(&in, KEY_DOWN) == CTUI_DATEINPUT_CHANGED);
	is_date(&in, 2026, 11, 2);
	CHECK(ctui_dateinput_key(&in, KEY_RIGHT) == CTUI_DATEINPUT_MOVED &&
	      in.segment == CTUI_DATEINPUT_MONTH);
	ctui_dateinput_key(&in, KEY_UP);
	ctui_dateinput_key(&in,
			   KEY_UP); /* December wraps to January, same year */
	is_date(&in, 2026, 1, 2);
	ctui_dateinput_key(&in, KEY_DOWN);
	is_date(&in, 2026, 12, 2);
	ctui_dateinput_key(&in, KEY_RIGHT);
	CHECK(in.segment == CTUI_DATEINPUT_DAY);
	CHECK(ctui_dateinput_key(&in, KEY_RIGHT) == CTUI_DATEINPUT_NONE);
	ctui_dateinput_set(&in, 2026, 12, 31);
	ctui_dateinput_key(&in, KEY_UP); /* wraps inside the month */
	is_date(&in, 2026, 12, 1);
	ctui_dateinput_key(&in, KEY_DOWN);
	is_date(&in, 2026, 12, 31);
	CHECK(ctui_dateinput_key(&in, '-') == CTUI_DATEINPUT_NONE);

	/* the day is clamped when the month or the year changes */
	ctui_dateinput_set(&in, 2026, 1, 31);
	in.segment = CTUI_DATEINPUT_MONTH;
	ctui_dateinput_key(&in, KEY_UP);
	is_date(&in, 2026, 2, 28);
	ctui_dateinput_set(&in, 2024, 2, 29);
	in.segment = CTUI_DATEINPUT_YEAR;
	ctui_dateinput_key(&in, KEY_UP);
	is_date(&in, 2025, 2, 28);
	ctui_dateinput_set(&in, 2026, 3, 31);
	in.segment = CTUI_DATEINPUT_MONTH;
	ctui_dateinput_key(&in, KEY_DOWN);
	is_date(&in, 2026, 2, 28);
	ctui_dateinput_key(&in, '/');
	CHECK(in.segment == CTUI_DATEINPUT_DAY);
	ctui_dateinput_key(&in, KEY_LEFT);
	CHECK(in.segment == CTUI_DATEINPUT_MONTH);
}

static void test_year_limits(void)
{
	ctui_dateinput in;
	ctui_dateinput_init(&in, 9999, 12, 31);
	CHECK(ctui_dateinput_key(&in, KEY_UP) ==
	      CTUI_DATEINPUT_MOVED); /* stays */
	is_date(&in, 9999, 12, 31);
	CHECK(!ctui_dateinput_step(&in, 1));
	CHECK(ctui_dateinput_step(
	    &in, -2000000000)); /* huge deltas do not overflow */
	is_date(&in, 1, 12, 31);
	CHECK(!ctui_dateinput_step(&in, -1));
	CHECK(ctui_dateinput_step(&in, 2000000000));
	is_date(&in, 9999, 12, 31);
	in.segment = CTUI_DATEINPUT_MONTH;
	CHECK(!ctui_dateinput_step(&in, 12)); /* a full turn */
	CHECK(ctui_dateinput_step(&in, 13) && in.month == 1);
	CHECK(ctui_dateinput_step(&in, -2000000000));
	in.segment = CTUI_DATEINPUT_DAY;
	CHECK(ctui_dateinput_step(&in, 1) && in.day == 1);
}

static void test_typing(void)
{
	ctui_dateinput in;
	ctui_dateinput_init(&in, 2026, 11, 2);
	type(&in, "1999");
	is_date(&in, 1999, 11, 2);
	CHECK(in.segment == CTUI_DATEINPUT_MONTH); /* four digits moved on */
	type(&in, "0");
	is_date(&in, 1999, 1, 2); /* 0 alone shows as 1 */
	CHECK(in.segment == CTUI_DATEINPUT_MONTH);
	type(&in, "2"); /* 02 */
	is_date(&in, 1999, 2, 2);
	CHECK(in.segment == CTUI_DATEINPUT_DAY);
	type(&in, "3"); /* 3 cannot take a second digit */
	is_date(&in, 1999, 2, 3);
	CHECK(in.segment == CTUI_DATEINPUT_DAY); /* last segment: stays */
	type(&in, "31");			 /* 31 > 28: restart with 1 */
	is_date(&in, 1999, 2, 1);

	/* a single digit above 1 completes the month at once */
	ctui_dateinput_set(&in, 2026, 8, 31);
	in.segment = CTUI_DATEINPUT_MONTH;
	type(&in, "2");
	is_date(&in, 2026, 2, 28); /* clamped */
	CHECK(in.segment == CTUI_DATEINPUT_DAY);
	/* 1 waits for the second digit */
	ctui_dateinput_set(&in, 2026, 8, 5);
	in.segment = CTUI_DATEINPUT_MONTH;
	type(&in, "1");
	CHECK(in.segment == CTUI_DATEINPUT_MONTH && in.month == 1);
	type(&in, "3"); /* 13 is not a month: restart with 3 */
	is_date(&in, 2026, 3, 5);
	CHECK(in.segment == CTUI_DATEINPUT_DAY);

	/* year 0000 and 0 are shown as year 1 */
	ctui_dateinput_set(&in, 2026, 2, 28);
	in.segment = CTUI_DATEINPUT_YEAR;
	type(&in, "0");
	is_date(&in, 1, 2, 28);
	type(&in, "000");
	is_date(&in, 1, 2, 28);

	/* a leap year typed over the 29th keeps it */
	ctui_dateinput_set(&in, 2023, 2, 28);
	in.segment = CTUI_DATEINPUT_DAY;
	type(&in, "29");
	is_date(&in, 2023, 2, 9); /* 29 > 28: restart with the 9 */
}

static void test_typing_day_edges(void)
{
	ctui_dateinput in;
	ctui_dateinput_init(&in, 2024, 2, 1);
	in.segment = CTUI_DATEINPUT_DAY;
	type(&in, "29");
	is_date(&in, 2024, 2, 29); /* leap year: allowed */
	ctui_dateinput_set(&in, 2023, 2, 1);
	in.segment = CTUI_DATEINPUT_DAY;
	type(&in, "2");
	is_date(&in, 2023, 2, 2);
	type(&in, "9"); /* 29 does not exist in 2023 */
	is_date(&in, 2023, 2, 9);
	ctui_dateinput_set(&in, 2026, 4, 1);
	in.segment = CTUI_DATEINPUT_DAY;
	type(&in, "31");
	is_date(&in, 2026, 4, 1); /* April has 30 days */
	ctui_dateinput_set(&in, 2026, 4, 1);
	in.segment = CTUI_DATEINPUT_DAY;
	type(&in, "00");
	is_date(&in, 2026, 4, 1);
	CHECK(ctui_dateinput_key(&in, 'x') == CTUI_DATEINPUT_NONE);
	CHECK(ctui_dateinput_key(&in, 27) == CTUI_DATEINPUT_CANCELLED);
}

static void test_picker_keys(void)
{
	ctui_dateinput in;
	ctui_dateinput_init(&in, 2026, 11, 2);
	in.today = (ctui_date){2026, 11, 20};
	CHECK(ctui_dateinput_key(&in, '\n') == CTUI_DATEINPUT_OPENED &&
	      in.open);
	CHECK(in.picker.year == 2026 && in.picker.month == 11 &&
	      in.picker.day == 2);
	CHECK(in.picker.today.day == 20);
	CHECK(ctui_dateinput_key(&in, KEY_RIGHT) == CTUI_DATEINPUT_MOVED);
	CHECK(ctui_dateinput_key(&in, KEY_NPAGE) == CTUI_DATEINPUT_MOVED);
	CHECK(ctui_dateinput_key(&in, KEY_DOWN) == CTUI_DATEINPUT_MOVED);
	is_date(&in, 2026, 11, 2); /* nothing committed yet */
	CHECK(ctui_dateinput_key(&in, '\n') == CTUI_DATEINPUT_CHANGED &&
	      !in.open);
	is_date(&in, 2026, 12, 10);
	CHECK(ctui_dateinput_days(&in) == ctui_date_to_days(2026, 12, 10));

	ctui_dateinput_key(&in, '\n');
	ctui_dateinput_key(&in, KEY_NPAGE);
	CHECK(ctui_dateinput_key(&in, 27) == CTUI_DATEINPUT_CLOSED && !in.open);
	is_date(&in, 2026, 12, 10);

	ctui_dateinput_key(&in, KEY_ENTER);
	CHECK(ctui_dateinput_key(&in, '\n') ==
	      CTUI_DATEINPUT_CLOSED); /* nothing moved */
	ctui_dateinput_key(&in, '\n');
	CHECK(ctui_dateinput_key(&in, KEY_HOME) == CTUI_DATEINPUT_MOVED);
	CHECK(ctui_dateinput_key(&in, 'z') == CTUI_DATEINPUT_NONE);
	CHECK(ctui_dateinput_key(&in, '\n') == CTUI_DATEINPUT_CHANGED);
	is_date(&in, 2026, 11, 20);

	/* the 31st carries over month ends in the picker */
	ctui_dateinput_set(&in, 2026, 1, 31);
	ctui_dateinput_key(&in, '\n');
	ctui_dateinput_key(&in, KEY_NPAGE);
	ctui_dateinput_key(&in, '\n');
	is_date(&in, 2026, 2, 28);

	/* the supported range */
	ctui_dateinput_set(&in, 9999, 12, 31);
	ctui_dateinput_key(&in, '\n');
	ctui_dateinput_key(&in, KEY_NPAGE);
	ctui_dateinput_key(&in, '\n');
	CHECK(ctui_dateinput_valid(in.year, in.month, in.day));
}

static void test_mouse(void)
{
	ctui_dateinput in;
	ctui_dateinput_init(&in, 2026, 11, 2);
	CHECK(ctui_dateinput_segment_at(0) == -1 &&
	      ctui_dateinput_segment_at(1) == CTUI_DATEINPUT_YEAR);
	CHECK(ctui_dateinput_segment_at(6) == CTUI_DATEINPUT_MONTH &&
	      ctui_dateinput_segment_at(10) == CTUI_DATEINPUT_DAY);
	CHECK(ctui_dateinput_segment_at(12) == -1 &&
	      ctui_dateinput_segment_at(-4) == -1);
	CHECK(ctui_dateinput_click(&in, 0, 7) == CTUI_DATEINPUT_MOVED &&
	      in.segment == CTUI_DATEINPUT_MONTH);
	CHECK(ctui_dateinput_click(&in, 0, 7) == CTUI_DATEINPUT_NONE);
	CHECK(ctui_dateinput_click(&in, 0, 10) == CTUI_DATEINPUT_MOVED &&
	      in.segment == CTUI_DATEINPUT_DAY);
	CHECK(ctui_dateinput_click(&in, 0, 0) == CTUI_DATEINPUT_NONE);
	CHECK(ctui_dateinput_click(&in, 1, 5) ==
	      CTUI_DATEINPUT_NONE); /* not on the field */
	CHECK(ctui_dateinput_click(&in, 0, 14) == CTUI_DATEINPUT_NONE);

	CHECK(ctui_dateinput_wheel(&in, 1) == CTUI_DATEINPUT_CHANGED);
	is_date(&in, 2026, 11, 3);
	CHECK(ctui_dateinput_wheel(&in, -3) == CTUI_DATEINPUT_CHANGED);
	is_date(&in, 2026, 11, 30);
	CHECK(ctui_dateinput_wheel(&in, 0) == CTUI_DATEINPUT_NONE);

	/* the arrow opens the picker; a day click picks it */
	CHECK(ctui_dateinput_click(&in, 0, 12) == CTUI_DATEINPUT_OPENED &&
	      in.open);
	int row, col;
	CHECK(
	    ctui_calendar_cell(2026, 11, in.picker.week_start, 17, &row, &col));
	/* the calendar sits in a frame: one row and one column in */
	CHECK(ctui_dateinput_click(&in, 2 + 2 + row, 1 + col * 3) ==
	      CTUI_DATEINPUT_CHANGED);
	CHECK(!in.open);
	is_date(&in, 2026, 11, 17);

	/* month buttons and the wheel change the month, the date waits */
	ctui_dateinput_click(&in, 0, 13);
	CHECK(ctui_dateinput_click(&in, 2, 1 + 19) ==
	      CTUI_DATEINPUT_MOVED); /* ">" */
	CHECK(in.picker.month == 12);
	CHECK(ctui_dateinput_click(&in, 2, 1) ==
	      CTUI_DATEINPUT_MOVED); /* "<" */
	CHECK(in.picker.month == 11);
	CHECK(ctui_dateinput_wheel(&in, -1) ==
	      CTUI_DATEINPUT_MOVED); /* wheel down: later month */
	CHECK(in.picker.month == 12);
	CHECK(ctui_dateinput_click(&in, 3, 1 + 5) ==
	      CTUI_DATEINPUT_NONE); /* weekday row */
	/* the frame is part of the picker: a click on it changes nothing */
	CHECK(ctui_dateinput_click(&in, 1, 0) == CTUI_DATEINPUT_NONE &&
	      in.open);
	CHECK(ctui_dateinput_click(&in, 5, 0) == CTUI_DATEINPUT_NONE &&
	      in.open);
	CHECK(ctui_dateinput_click(&in, ctui_calendar_height() + 2, 4) ==
		  CTUI_DATEINPUT_NONE &&
	      in.open);
	CHECK(ctui_dateinput_click(&in, 5, ctui_calendar_width() + 1) ==
	      CTUI_DATEINPUT_NONE);
	CHECK(ctui_dateinput_click(&in, ctui_calendar_height() + 3, 4) ==
		  CTUI_DATEINPUT_CLOSED &&
	      !in.open);
	ctui_dateinput_click(&in, 0, 12);
	is_date(&in, 2026, 11, 17);
	CHECK(ctui_dateinput_click(&in, 20, 3) == CTUI_DATEINPUT_CLOSED &&
	      !in.open); /* outside */
	is_date(&in, 2026, 11, 17);
	ctui_dateinput_click(&in, 0, 12);
	CHECK(ctui_dateinput_click(&in, 0, 12) == CTUI_DATEINPUT_CLOSED &&
	      !in.open); /* arrow again */
}

static void test_draw(void)
{
	screen_open(24, 80);
	ctui_dateinput in;
	ctui_dateinput_init(&in, 2026, 11, 2);
	screen_clear();
	ctui_dateinput_draw(&in, 2, 3, true);
	CHECK(screen_has_text("2026-11-02"));
	CHECK(!screen_has_text("November"));
	in.today = (ctui_date){2026, 11, 2};
	ctui_dateinput_key(&in, '\n');
	screen_clear();
	ctui_dateinput_draw(&in, 2, 3, false);
	/* the calendar has the background of the frame around it */
	short body =
	    (short)PAIR_NUMBER((chtype)ctui_attr(CTUI_PAIR_BODY, false));
	short dialog =
	    (short)PAIR_NUMBER((chtype)ctui_attr(CTUI_PAIR_DIALOG, false));
	int on_body = 0, on_dialog = 0;
	for (int row = 4; row < 4 + ctui_calendar_height(); row++)
		for (int col = 4; col < 4 + ctui_calendar_width(); col++) {
			cchar_t cell;
			wchar_t wch[CCHARW_MAX];
			attr_t attrs;
			short pair;
			mvin_wch(row, col, &cell);
			getcchar(&cell, wch, &attrs, &pair, NULL);
			on_body += pair == body && body != dialog;
			on_dialog += pair == dialog;
		}
	CHECK(on_body == 0 && on_dialog > 100);
	CHECK(screen_has_text("November 2026") &&
	      screen_has_text("M  T  W  T  F  S  S")); /* one letter a day */
	static const char *const NAMES[7] = {"L", "M", "M", "J", "V", "S", "D"};
	ctui_dateinput_key(&in, 27);
	in.weekday_names = NAMES;
	ctui_dateinput_key(&in, '\n');
	screen_clear();
	ctui_dateinput_draw(&in, 2, 3, false);
	CHECK(screen_has_text("L  M  M  J  V  S  D"));
	in.weekday_names = NULL;
	ctui_dateinput_draw(&in, 22, 70,
			    true); /* near the corner: clipped, no crash */

	ctui_dateinput_key(&in, 27);
	ctui_style hot = {0};
	hot.attr[CTUI_PAIR_SELECTION] = ctui_attr_color(CTUI_C_RED, -1);
	in.style = &hot;
	screen_clear();
	ctui_dateinput_draw(&in, 2, 3, true);
	CHECK(screen_has_color(ctui_attr_color(CTUI_C_RED, -1)));
	in.style = NULL;
	screen_clear();
	ctui_dateinput_draw(&in, 2, 3, true);
	CHECK(!screen_has_color(ctui_attr_color(CTUI_C_RED, -1)));
}

int main(void)
{
	RUN(test_valid);
	RUN(test_parse);
	RUN(test_format);
	RUN(test_days);
	RUN(test_arrows);
	RUN(test_year_limits);
	RUN(test_typing);
	RUN(test_typing_day_edges);
	RUN(test_picker_keys);
	RUN(test_mouse);
	RUN(test_draw);
	return CHECK_DONE();
}
