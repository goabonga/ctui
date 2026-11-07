/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * test_timeinput.c - time of day field: parsing, formatting, limits,
 * stepping, typing, mouse and drawing.
 */
#include "check.h"
#include "screen.h"

#include <curses.h>
#include <string.h>

#include <ctui/timeinput.h>

#define H 3600000000LL
#define M 60000000LL
#define S 1000000LL

static void type(ctui_timeinput *in, const char *keys)
{
	for (; *keys; keys++)
		ctui_timeinput_key(in, *keys);
}

static void test_valid_and_parse(void)
{
	long long v = -1;
	CHECK(ctui_timeinput_valid(0) && ctui_timeinput_valid(24 * H - 1));
	CHECK(!ctui_timeinput_valid(-1) && !ctui_timeinput_valid(24 * H));
	CHECK(ctui_timeinput_parse("00:00", &v) && v == 0);
	CHECK(ctui_timeinput_parse("23:59", &v) && v == 23 * H + 59 * M);
	CHECK(ctui_timeinput_parse("12:34:56", &v) &&
	      v == 12 * H + 34 * M + 56 * S);
	CHECK(ctui_timeinput_parse("12:34:56.5", &v) &&
	      v == 12 * H + 34 * M + 56 * S + 500000);
	CHECK(ctui_timeinput_parse("12:34:56.789", &v) &&
	      v == 12 * H + 34 * M + 56 * S + 789000);
	CHECK(ctui_timeinput_parse("00:00:00.000001", &v) && v == 1);
	CHECK(ctui_timeinput_parse("23:59:59.999999", &v) && v == 24 * H - 1);
	CHECK(ctui_timeinput_parse("01:02", NULL)); /* only checking */
	v = 77;
	CHECK(!ctui_timeinput_parse(NULL, &v) && !ctui_timeinput_parse("", &v));
	CHECK(!ctui_timeinput_parse("24:00", &v) &&
	      !ctui_timeinput_parse("12:60", &v) &&
	      !ctui_timeinput_parse("12:00:60", &v));
	CHECK(!ctui_timeinput_parse("1:00", &v) &&
	      !ctui_timeinput_parse("12-00", &v) &&
	      !ctui_timeinput_parse("12:00:", &v) &&
	      !ctui_timeinput_parse("12:00:0", &v) &&
	      !ctui_timeinput_parse("12:00:00.", &v) &&
	      !ctui_timeinput_parse("12:00:00.1234567", &v) &&
	      !ctui_timeinput_parse("12:00:00,5", &v) &&
	      !ctui_timeinput_parse("12:00:00.5x", &v) &&
	      !ctui_timeinput_parse(" 12:00", &v) &&
	      !ctui_timeinput_parse("+2:00", &v) &&
	      !ctui_timeinput_parse("12:00 ", &v));
	CHECK(v == 77); /* never touched on failure */
}

static void test_format_and_width(void)
{
	char buf[CTUI_TIMEINPUT_TEXT];
	long long t = 9 * H + 5 * M + 7 * S + 123456;
	CHECK_STR(ctui_timeinput_format(t, CTUI_TIME_MINUTES, buf, sizeof buf),
		  "09:05");
	CHECK_STR(ctui_timeinput_format(t, CTUI_TIME_SECONDS, buf, sizeof buf),
		  "09:05:07");
	CHECK_STR(ctui_timeinput_format(t, CTUI_TIME_MILLIS, buf, sizeof buf),
		  "09:05:07.123");
	CHECK_STR(ctui_timeinput_format(t, CTUI_TIME_MICROS, buf, sizeof buf),
		  "09:05:07.123456");
	CHECK(ctui_timeinput_format(-1, CTUI_TIME_SECONDS, buf, sizeof buf) ==
	      NULL);
	CHECK(ctui_timeinput_format(24 * H, CTUI_TIME_SECONDS, buf,
				    sizeof buf) == NULL);
	// NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
	CHECK(ctui_timeinput_format(0, (ctui_time_precision)9, buf,
				    sizeof buf) == NULL);
	// NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
	CHECK(ctui_timeinput_format(0, (ctui_time_precision)-1, buf,
				    sizeof buf) == NULL);
	CHECK(ctui_timeinput_format(0, CTUI_TIME_MICROS, buf, 15) == NULL);
	CHECK(ctui_timeinput_format(0, CTUI_TIME_MICROS, NULL, 16) == NULL);
	CHECK(ctui_timeinput_width(CTUI_TIME_MINUTES) == 7 &&
	      ctui_timeinput_width(CTUI_TIME_SECONDS) == 10 &&
	      ctui_timeinput_width(CTUI_TIME_MILLIS) == 14 &&
	      ctui_timeinput_width(CTUI_TIME_MICROS) == 17);
	// NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
	CHECK(ctui_timeinput_width((ctui_time_precision)7) == 0);
}

static void test_init_and_set(void)
{
	ctui_timeinput in;
	ctui_timeinput_init(&in, CTUI_TIME_SECONDS, 3 * H + 2 * S + 999999);
	CHECK(in.micro == 3 * H + 2 * S); /* cut, not rounded */
	CHECK(in.segment == CTUI_TIMEINPUT_HOUR && in.typed == 0);
	ctui_timeinput_init(&in, CTUI_TIME_MINUTES, 5 * M + 59 * S);
	CHECK(in.micro == 5 * M);
	ctui_timeinput_init(&in, CTUI_TIME_MILLIS, 1500);
	CHECK(in.micro == 1000);
	ctui_timeinput_init(&in, CTUI_TIME_SECONDS, -5);
	CHECK(in.micro == 0);
	ctui_timeinput_init(&in, CTUI_TIME_SECONDS, 24 * H);
	CHECK(in.micro == 0);
	// NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
	ctui_timeinput_init(&in, (ctui_time_precision)12, H);
	CHECK(in.precision == CTUI_TIME_SECONDS && in.micro == H);

	CHECK(ctui_timeinput_set(&in, 8 * H + 30 * M) &&
	      in.micro == 8 * H + 30 * M);
	CHECK(!ctui_timeinput_set(&in, -1) && !ctui_timeinput_set(&in, 24 * H));
	CHECK(in.micro == 8 * H + 30 * M);
	CHECK(ctui_timeinput_set(&in, 7 * S + 900000) && in.micro == 7 * S);
}

static void test_limits(void)
{
	ctui_timeinput in;
	ctui_timeinput_init(&in, CTUI_TIME_SECONDS, 12 * H);
	long long lo = 9 * H, hi = 17 * H;
	CHECK(ctui_timeinput_set_limits(&in, &lo, &hi));
	CHECK(in.has_min && in.has_max && in.min == lo && in.max == hi);
	CHECK(in.micro == 12 * H);
	/* setting outside the limits is refused */
	CHECK(!ctui_timeinput_set(&in, 8 * H) &&
	      !ctui_timeinput_set(&in, 18 * H));
	CHECK(ctui_timeinput_set(&in, 9 * H) &&
	      ctui_timeinput_set(&in, 17 * H));
	/* stepping stops at the limits */
	in.segment = CTUI_TIMEINPUT_HOUR;
	CHECK(!ctui_timeinput_step(&in, 1) && in.micro == 17 * H);
	CHECK(ctui_timeinput_step(&in, -1) && in.micro == 16 * H);
	ctui_timeinput_set(&in, 9 * H);
	CHECK(!ctui_timeinput_step(&in, -1) && in.micro == 9 * H);
	/* typing is held inside them too */
	ctui_timeinput_set(&in, 12 * H);
	in.segment = CTUI_TIMEINPUT_HOUR;
	type(&in, "23");
	CHECK(in.micro == 17 * H);
	in.segment = CTUI_TIMEINPUT_HOUR;
	type(&in, "03");
	CHECK(in.micro == 9 * H);
	/* new limits bring the value in */
	long long hi2 = 10 * H;
	CHECK(ctui_timeinput_set_limits(&in, NULL, &hi2));
	CHECK(!in.has_min && in.has_max && in.micro == 9 * H);
	long long hi3 = 5 * H;
	CHECK(ctui_timeinput_set_limits(&in, NULL, &hi3) && in.micro == 5 * H);
	/* a minimum above the maximum is refused and changes nothing */
	long long a = 10 * H, b = 2 * H;
	CHECK(!ctui_timeinput_set_limits(&in, &a, &b));
	CHECK(!in.has_min && in.max == hi3);
	/* no limits */
	CHECK(ctui_timeinput_set_limits(&in, NULL, NULL));
	CHECK(!in.has_min && !in.has_max);
	CHECK(ctui_timeinput_set(&in, 23 * H));
}

static void test_step(void)
{
	ctui_timeinput in;
	ctui_timeinput_init(&in, CTUI_TIME_MICROS, 23 * H + 59 * M + 59 * S);
	in.segment = CTUI_TIMEINPUT_HOUR;
	CHECK(ctui_timeinput_step(&in, 1)); /* wraps, no carry */
	CHECK(in.micro == 59 * M + 59 * S);
	CHECK(ctui_timeinput_step(&in, -1) &&
	      in.micro == 23 * H + 59 * M + 59 * S);
	in.segment = CTUI_TIMEINPUT_MINUTE;
	CHECK(ctui_timeinput_step(&in, 1) && in.micro == 23 * H + 59 * S);
	in.segment = CTUI_TIMEINPUT_SECOND;
	CHECK(ctui_timeinput_step(&in, 1) && in.micro == 23 * H);
	in.segment = CTUI_TIMEINPUT_FRACTION;
	CHECK(ctui_timeinput_step(&in, -1) && in.micro == 23 * H + 999999);
	CHECK(ctui_timeinput_step(&in, 1) && in.micro == 23 * H);
	CHECK(ctui_timeinput_step(&in, 10) && in.micro == 23 * H + 10);
	/* in milliseconds the fraction steps by a millisecond */
	ctui_timeinput_init(&in, CTUI_TIME_MILLIS, 0);
	in.segment = CTUI_TIMEINPUT_FRACTION;
	CHECK(ctui_timeinput_step(&in, 1) && in.micro == 1000);
	CHECK(ctui_timeinput_step(&in, -2) && in.micro == 999000);
	/* a full turn gets back where it started */
	ctui_timeinput_init(&in, CTUI_TIME_SECONDS, 5 * H);
	CHECK(!ctui_timeinput_step(&in, 24) && in.micro == 5 * H);
}

static void test_typing(void)
{
	ctui_timeinput in;
	ctui_timeinput_init(&in, CTUI_TIME_SECONDS, 0);
	/* two digits fill the hour and move on to the minute */
	CHECK(ctui_timeinput_key(&in, '1') == CTUI_TIMEINPUT_CHANGED);
	CHECK(in.micro == H && in.segment == CTUI_TIMEINPUT_HOUR);
	CHECK(ctui_timeinput_key(&in, '4') == CTUI_TIMEINPUT_CHANGED);
	CHECK(in.micro == 14 * H && in.segment == CTUI_TIMEINPUT_MINUTE);
	/* a first digit that cannot be followed moves on at once */
	type(&in, "7");
	CHECK(in.micro == 14 * H + 7 * M &&
	      in.segment == CTUI_TIMEINPUT_SECOND);
	type(&in, "45");
	CHECK(in.micro == 14 * H + 7 * M + 45 * S);
	CHECK(in.segment == CTUI_TIMEINPUT_SECOND); /* the last stays */
	/* an hour that would exceed 23 starts over */
	in.segment = CTUI_TIMEINPUT_HOUR;
	type(&in, "2");
	CHECK(in.micro == 2 * H + 7 * M + 45 * S &&
	      in.segment == CTUI_TIMEINPUT_HOUR);
	type(&in, "9");
	CHECK(in.micro == 9 * H + 7 * M + 45 * S &&
	      in.segment == CTUI_TIMEINPUT_MINUTE);
	/* 6 cannot take another digit in the tens of the minutes */
	in.segment = CTUI_TIMEINPUT_MINUTE;
	type(&in, "6");
	CHECK(in.segment == CTUI_TIMEINPUT_SECOND);
	CHECK(in.micro == 9 * H + 6 * M + 45 * S);
	/* other keys end a half typed number */
	in.segment = CTUI_TIMEINPUT_HOUR;
	type(&in, "1");
	ctui_timeinput_key(&in, KEY_LEFT);
	CHECK(in.typed == 0);
	ctui_timeinput_key(&in, '5');
	CHECK(in.micro == 5 * H + 6 * M + 45 * S);
}

static void test_typing_fraction(void)
{
	ctui_timeinput in;
	ctui_timeinput_init(&in, CTUI_TIME_MILLIS, 0);
	in.segment = CTUI_TIMEINPUT_FRACTION;
	type(&in, "5");
	CHECK(in.micro == 500000); /* filled from the left */
	type(&in, "2");
	CHECK(in.micro == 520000);
	type(&in, "7");
	CHECK(in.micro == 527000 && in.typed == 0);
	ctui_timeinput_init(&in, CTUI_TIME_MICROS, 0);
	in.segment = CTUI_TIMEINPUT_FRACTION;
	type(&in, "123456");
	CHECK(in.micro == 123456 && in.typed == 0);
	type(&in, "9");
	CHECK(in.micro == 900000);
}

static void test_keys(void)
{
	ctui_timeinput in;
	ctui_timeinput_init(&in, CTUI_TIME_MILLIS, 0);
	CHECK(ctui_timeinput_key(&in, KEY_LEFT) == CTUI_TIMEINPUT_NONE);
	CHECK(ctui_timeinput_key(&in, KEY_RIGHT) == CTUI_TIMEINPUT_MOVED &&
	      in.segment == CTUI_TIMEINPUT_MINUTE);
	CHECK(ctui_timeinput_key(&in, ':') == CTUI_TIMEINPUT_MOVED &&
	      in.segment == CTUI_TIMEINPUT_SECOND);
	CHECK(ctui_timeinput_key(&in, '.') == CTUI_TIMEINPUT_MOVED &&
	      in.segment == CTUI_TIMEINPUT_FRACTION);
	CHECK(ctui_timeinput_key(&in, ' ') == CTUI_TIMEINPUT_NONE);
	CHECK(ctui_timeinput_key(&in, KEY_LEFT) == CTUI_TIMEINPUT_MOVED &&
	      in.segment == CTUI_TIMEINPUT_SECOND);
	CHECK(ctui_timeinput_key(&in, KEY_UP) == CTUI_TIMEINPUT_CHANGED &&
	      in.micro == S);
	CHECK(ctui_timeinput_key(&in, KEY_DOWN) == CTUI_TIMEINPUT_CHANGED &&
	      in.micro == 0);
	CHECK(ctui_timeinput_key(&in, KEY_PPAGE) == CTUI_TIMEINPUT_CHANGED &&
	      in.micro == 10 * S);
	CHECK(ctui_timeinput_key(&in, KEY_NPAGE) == CTUI_TIMEINPUT_CHANGED &&
	      in.micro == 0);
	/* the segments a precision hides cannot be reached */
	ctui_timeinput_init(&in, CTUI_TIME_MINUTES, 0);
	ctui_timeinput_key(&in, KEY_RIGHT);
	CHECK(ctui_timeinput_key(&in, KEY_RIGHT) == CTUI_TIMEINPUT_NONE &&
	      in.segment == CTUI_TIMEINPUT_MINUTE);
	ctui_timeinput_init(&in, CTUI_TIME_SECONDS, 0);
	in.segment = CTUI_TIMEINPUT_SECOND;
	CHECK(ctui_timeinput_key(&in, KEY_RIGHT) == CTUI_TIMEINPUT_NONE);
	/* Up on a limit moves nothing but is still a key press */
	long long hi = 0;
	ctui_timeinput_set_limits(&in, NULL, &hi);
	CHECK(ctui_timeinput_key(&in, KEY_UP) == CTUI_TIMEINPUT_MOVED);
	CHECK(ctui_timeinput_key(&in, '\n') == CTUI_TIMEINPUT_SUBMITTED);
	CHECK(ctui_timeinput_key(&in, '\r') == CTUI_TIMEINPUT_SUBMITTED);
	CHECK(ctui_timeinput_key(&in, KEY_ENTER) == CTUI_TIMEINPUT_SUBMITTED);
	CHECK(ctui_timeinput_key(&in, 27) == CTUI_TIMEINPUT_CANCELLED);
	CHECK(ctui_timeinput_key(&in, 'x') == CTUI_TIMEINPUT_NONE);
}

static void test_mouse(void)
{
	ctui_timeinput in;
	ctui_timeinput_init(&in, CTUI_TIME_MICROS, 0);
	CHECK(ctui_timeinput_segment_at(CTUI_TIME_MICROS, 0) == -1);
	CHECK(ctui_timeinput_segment_at(CTUI_TIME_MICROS, 1) ==
	      CTUI_TIMEINPUT_HOUR);
	CHECK(ctui_timeinput_segment_at(CTUI_TIME_MICROS, 5) ==
	      CTUI_TIMEINPUT_MINUTE);
	CHECK(ctui_timeinput_segment_at(CTUI_TIME_MICROS, 8) ==
	      CTUI_TIMEINPUT_SECOND);
	CHECK(ctui_timeinput_segment_at(CTUI_TIME_MICROS, 12) ==
	      CTUI_TIMEINPUT_FRACTION);
	CHECK(ctui_timeinput_segment_at(CTUI_TIME_MICROS, 16) == -1);
	CHECK(ctui_timeinput_segment_at(CTUI_TIME_MILLIS, 12) ==
	      CTUI_TIMEINPUT_FRACTION);
	CHECK(ctui_timeinput_segment_at(CTUI_TIME_MILLIS, 13) == -1);
	CHECK(ctui_timeinput_segment_at(CTUI_TIME_SECONDS, 12) == -1);
	CHECK(ctui_timeinput_segment_at(CTUI_TIME_MINUTES, 8) == -1);
	CHECK(ctui_timeinput_segment_at(CTUI_TIME_MINUTES, 5) ==
	      CTUI_TIMEINPUT_MINUTE);
	// NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
	CHECK(ctui_timeinput_segment_at((ctui_time_precision)9, 1) == -1);

	CHECK(ctui_timeinput_click(&in, 0, 8) == CTUI_TIMEINPUT_MOVED &&
	      in.segment == CTUI_TIMEINPUT_SECOND);
	CHECK(ctui_timeinput_click(&in, 0, 12) == CTUI_TIMEINPUT_MOVED &&
	      in.segment == CTUI_TIMEINPUT_FRACTION);
	CHECK(ctui_timeinput_click(&in, 0, 12) == CTUI_TIMEINPUT_NONE);
	CHECK(ctui_timeinput_click(&in, 0, 0) == CTUI_TIMEINPUT_NONE);
	CHECK(ctui_timeinput_click(&in, 1, 2) == CTUI_TIMEINPUT_NONE);
	CHECK(ctui_timeinput_click(&in, 0, 17) == CTUI_TIMEINPUT_NONE);
	CHECK(ctui_timeinput_click(&in, 0, -1) == CTUI_TIMEINPUT_NONE);

	in.segment = CTUI_TIMEINPUT_MINUTE;
	CHECK(ctui_timeinput_wheel(&in, 3) == CTUI_TIMEINPUT_CHANGED &&
	      in.micro == 3 * M);
	CHECK(ctui_timeinput_wheel(&in, -1) == CTUI_TIMEINPUT_CHANGED &&
	      in.micro == 2 * M);
	CHECK(ctui_timeinput_wheel(&in, 0) == CTUI_TIMEINPUT_NONE);
	CHECK(ctui_timeinput_wheel(&in, 60) == CTUI_TIMEINPUT_NONE);
}

static void row_text(int y, int x, int n, char *out)
{
	mvinnstr(y, x, out, n);
	out[n] = '\0';
}

static void test_draw(void)
{
	screen_open(24, 80);
	ctui_timeinput in;
	ctui_timeinput_init(&in, CTUI_TIME_MICROS,
			    9 * H + 5 * M + 7 * S + 123456);
	char row[32];
	screen_clear();
	ctui_timeinput_draw(&in, 2, 3, false);
	row_text(2, 3, 17, row);
	CHECK_STR(row, " 09:05:07.123456 ");
	ctui_timeinput_init(&in, CTUI_TIME_MINUTES, 9 * H + 5 * M);
	screen_clear();
	ctui_timeinput_draw(&in, 0, 0, true);
	row_text(0, 0, 7, row);
	CHECK_STR(row, " 09:05 ");
	/* the segment under the cursor is reversed when focused */
	cchar_t cell;
	wchar_t wch[CCHARW_MAX];
	attr_t attrs;
	short pair;
	in.segment = CTUI_TIMEINPUT_MINUTE;
	screen_clear();
	ctui_timeinput_draw(&in, 0, 0, true);
	mvin_wch(0, 4, &cell);
	getcchar(&cell, wch, &attrs, &pair, NULL);
	CHECK(attrs & A_REVERSE);
	mvin_wch(0, 1, &cell);
	getcchar(&cell, wch, &attrs, &pair, NULL);
	CHECK(!(attrs & A_REVERSE));
	/* the fraction too, and a style */
	ctui_timeinput_init(&in, CTUI_TIME_MILLIS, 0);
	in.segment = CTUI_TIMEINPUT_FRACTION;
	screen_clear();
	ctui_timeinput_draw(&in, 0, 0, true);
	mvin_wch(0, 10, &cell);
	getcchar(&cell, wch, &attrs, &pair, NULL);
	CHECK(attrs & A_REVERSE);
	ctui_style style = {0};
	in.style = &style;
	ctui_timeinput_draw(&in, 0, 0, false);
	endwin();
}

int main(void)
{
	RUN(test_valid_and_parse);
	RUN(test_format_and_width);
	RUN(test_init_and_set);
	RUN(test_limits);
	RUN(test_step);
	RUN(test_typing);
	RUN(test_typing_fraction);
	RUN(test_keys);
	RUN(test_mouse);
	RUN(test_draw);
	return CHECK_DONE();
}
