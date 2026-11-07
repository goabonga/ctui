/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * test_durationinput.c - duration field: parsing, formatting, limits,
 * stepping with carry, typing, mouse and drawing.
 */
#include "check.h"
#include "screen.h"

#include <curses.h>
#include <string.h>

#include <ctui/durationinput.h>

#define H 3600000000LL
#define M 60000000LL
#define S 1000000LL
#define MS 1000LL

static void type(ctui_durationinput *in, const char *keys)
{
	for (; *keys; keys++)
		ctui_durationinput_key(in, *keys);
}

static void test_units(void)
{
	CHECK(ctui_duration_unit_micros(CTUI_DURATION_HOURS) == H);
	CHECK(ctui_duration_unit_micros(CTUI_DURATION_MINUTES) == M);
	CHECK(ctui_duration_unit_micros(CTUI_DURATION_SECONDS) == S);
	CHECK(ctui_duration_unit_micros(CTUI_DURATION_MILLIS) == MS);
	CHECK(ctui_duration_unit_micros(CTUI_DURATION_MICROS) == 1);
	// NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
	CHECK(ctui_duration_unit_micros((ctui_duration_unit)9) == 0);
	// NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
	CHECK(ctui_duration_unit_micros((ctui_duration_unit)-1) == 0);
}

static void test_parse(void)
{
	long long v = -1;
	CHECK(ctui_durationinput_parse("0s", &v) && v == 0);
	CHECK(ctui_durationinput_parse("1h30m", &v) && v == H + 30 * M);
	CHECK(ctui_durationinput_parse("90m", &v) && v == 90 * M);
	CHECK(ctui_durationinput_parse("1h 30m 5s 250ms 10us", &v) &&
	      v == H + 30 * M + 5 * S + 250 * MS + 10);
	CHECK(ctui_durationinput_parse("  2s  ", &v) && v == 2 * S);
	CHECK(ctui_durationinput_parse("500ms", &v) && v == 500 * MS);
	CHECK(ctui_durationinput_parse("7us", &v) && v == 7);
	CHECK(ctui_durationinput_parse("012h", &v) && v == 12 * H);
	CHECK(ctui_durationinput_parse("999999999h", &v) && v == 999999999 * H);
	CHECK(ctui_durationinput_parse("5m", NULL)); /* only checking */
	v = 77;
	CHECK(!ctui_durationinput_parse(NULL, &v) &&
	      !ctui_durationinput_parse("", &v) &&
	      !ctui_durationinput_parse("   ", &v));
	CHECK(!ctui_durationinput_parse("5", &v) &&
	      !ctui_durationinput_parse("h", &v) &&
	      !ctui_durationinput_parse("5x", &v) &&
	      !ctui_durationinput_parse("-5s", &v) &&
	      !ctui_durationinput_parse("1.5s", &v) &&
	      !ctui_durationinput_parse("5S", &v) &&
	      !ctui_durationinput_parse("5 s", &v));
	/* from the largest to the smallest, each once */
	CHECK(!ctui_durationinput_parse("30m1h", &v) &&
	      !ctui_durationinput_parse("1h1h", &v) &&
	      !ctui_durationinput_parse("1s1m", &v) &&
	      !ctui_durationinput_parse("1us1ms", &v));
	/* too big */
	CHECK(!ctui_durationinput_parse("1000000000000h", &v) &&
	      !ctui_durationinput_parse("99999999999999999999s", &v) &&
	      !ctui_durationinput_parse("1000000000000000000us1", &v));
	CHECK(ctui_durationinput_parse("1000000000000h", &v) == false);
	CHECK(v == 77); /* never touched on failure */
}

static void test_format(void)
{
	char buf[CTUI_DURATION_TEXT];
	long long t = 12 * H + 30 * M + 5 * S + 250 * MS + 17;
	CHECK_STR(ctui_durationinput_format(t, CTUI_DURATION_HOURS,
					    CTUI_DURATION_MICROS, 3, buf,
					    sizeof buf),
		  "012h 30m 05s 250ms 017us");
	CHECK_STR(ctui_durationinput_format(t, CTUI_DURATION_HOURS,
					    CTUI_DURATION_MINUTES, 2, buf,
					    sizeof buf),
		  "12h 30m");
	CHECK_STR(ctui_durationinput_format(t, CTUI_DURATION_MINUTES,
					    CTUI_DURATION_SECONDS, 4, buf,
					    sizeof buf),
		  "0750m 05s");
	CHECK_STR(ctui_durationinput_format(t, CTUI_DURATION_SECONDS,
					    CTUI_DURATION_MILLIS, 5, buf,
					    sizeof buf),
		  "45005s 250ms");
	CHECK_STR(ctui_durationinput_format(t, CTUI_DURATION_MICROS,
					    CTUI_DURATION_MICROS, 9, buf,
					    sizeof buf),
		  "45005250017us");
	CHECK_STR(ctui_durationinput_format(0, CTUI_DURATION_HOURS,
					    CTUI_DURATION_HOURS, 1, buf,
					    sizeof buf),
		  "0h");
	CHECK(ctui_durationinput_format(-1, CTUI_DURATION_HOURS,
					CTUI_DURATION_SECONDS, 3, buf,
					sizeof buf) == NULL);
	CHECK(ctui_durationinput_format(0, CTUI_DURATION_SECONDS,
					CTUI_DURATION_HOURS, 3, buf,
					sizeof buf) == NULL);
	// NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
	CHECK(ctui_durationinput_format(0, (ctui_duration_unit)7,
					CTUI_DURATION_HOURS, 3, buf,
					sizeof buf) == NULL);
	CHECK(ctui_durationinput_format(0, CTUI_DURATION_HOURS,
					CTUI_DURATION_SECONDS, 0, buf,
					sizeof buf) == NULL);
	CHECK(ctui_durationinput_format(0, CTUI_DURATION_HOURS,
					CTUI_DURATION_SECONDS, 10, buf,
					sizeof buf) == NULL);
	CHECK(ctui_durationinput_format(0, CTUI_DURATION_HOURS,
					CTUI_DURATION_SECONDS, 3, buf,
					CTUI_DURATION_TEXT - 1) == NULL);
	CHECK(ctui_durationinput_format(0, CTUI_DURATION_HOURS,
					CTUI_DURATION_SECONDS, 3, NULL,
					CTUI_DURATION_TEXT) == NULL);
}

static void test_init_and_set(void)
{
	ctui_durationinput in;
	ctui_durationinput_init(&in, CTUI_DURATION_HOURS, CTUI_DURATION_SECONDS,
				H + 30 * M + 5 * S + 999999);
	CHECK(in.micro == H + 30 * M + 5 * S); /* cut to the seconds */
	CHECK(in.segment == CTUI_DURATION_HOURS && in.digits == 3);
	CHECK(ctui_durationinput_capacity(&in) == 999 * H + 59 * M + 59 * S);
	ctui_durationinput_init(&in, CTUI_DURATION_MINUTES,
				CTUI_DURATION_MINUTES, -5);
	CHECK(in.micro == 0);
	ctui_durationinput_init(&in, CTUI_DURATION_MINUTES,
				CTUI_DURATION_MINUTES, 5000 * H);
	CHECK(in.micro == 999 * M + 0 && in.segment == CTUI_DURATION_MINUTES);
	CHECK(in.micro == ctui_durationinput_capacity(&in));
	// NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
	ctui_durationinput_init(&in, (ctui_duration_unit)9, CTUI_DURATION_HOURS,
				S);
	CHECK(in.largest == CTUI_DURATION_HOURS &&
	      in.smallest == CTUI_DURATION_SECONDS && in.micro == S);
	ctui_durationinput_init(&in, CTUI_DURATION_SECONDS, CTUI_DURATION_HOURS,
				S);
	CHECK(in.largest == CTUI_DURATION_HOURS &&
	      in.smallest == CTUI_DURATION_SECONDS);

	ctui_durationinput_init(&in, CTUI_DURATION_HOURS, CTUI_DURATION_MINUTES,
				0);
	CHECK(ctui_durationinput_set(&in, 2 * H + 59 * S) && in.micro == 2 * H);
	CHECK(!ctui_durationinput_set(&in, -1) &&
	      !ctui_durationinput_set(&in, 1000 * H));
	CHECK(in.micro == 2 * H);
}

static void test_max(void)
{
	ctui_durationinput in;
	ctui_durationinput_init(&in, CTUI_DURATION_HOURS, CTUI_DURATION_SECONDS,
				30 * H);
	long long max = 24 * H;
	CHECK(ctui_durationinput_set_max(&in, &max));
	CHECK(in.has_max && in.max == max && in.digits == 2);
	CHECK(in.micro == 24 * H); /* brought under it */
	CHECK(ctui_durationinput_capacity(&in) == 24 * H);
	CHECK(!ctui_durationinput_set(&in, 24 * H + S));
	CHECK(ctui_durationinput_set(&in, 24 * H));
	/* stepping stops at it */
	in.segment = CTUI_DURATION_HOURS;
	CHECK(!ctui_durationinput_step(&in, 1) && in.micro == 24 * H);
	CHECK(ctui_durationinput_step(&in, -1) && in.micro == 23 * H);
	in.segment = CTUI_DURATION_MINUTES;
	ctui_durationinput_set(&in, 23 * H + 59 * M);
	CHECK(ctui_durationinput_step(&in, 1) && in.micro == 24 * H);
	CHECK(!ctui_durationinput_step(&in, 1));
	/* typing too */
	ctui_durationinput_set(&in, 0);
	in.segment = CTUI_DURATION_HOURS;
	type(&in, "99");
	CHECK(in.micro == 24 * H);
	/* the digits follow the maximum */
	max = 100 * H;
	CHECK(ctui_durationinput_set_max(&in, &max) && in.digits == 3);
	max = 5 * M; /* a maximum under one hour: one digit of hours */
	CHECK(ctui_durationinput_set_max(&in, &max) && in.digits == 1);
	CHECK(in.micro == 5 * M);
	max = 0;
	CHECK(ctui_durationinput_set_max(&in, &max) && in.digits == 1 &&
	      in.micro == 0);
	/* too many digits, or negative: refused, nothing changes */
	ctui_durationinput_set(&in, 0);
	max = 1000000000LL * H;
	CHECK(!ctui_durationinput_set_max(&in, &max) && in.has_max &&
	      in.max == 0);
	max = -1;
	CHECK(!ctui_durationinput_set_max(&in, &max));
	/* none again: back to the default digits */
	CHECK(ctui_durationinput_set_max(&in, NULL));
	CHECK(!in.has_max && in.digits == CTUI_DURATION_DEFAULT_DIGITS);
	/* the maximum is cut to the smallest unit */
	ctui_durationinput_init(&in, CTUI_DURATION_HOURS, CTUI_DURATION_MINUTES,
				0);
	max = H + 59 * S;
	ctui_durationinput_set_max(&in, &max);
	CHECK(ctui_durationinput_capacity(&in) == H);
}

static void test_step(void)
{
	ctui_durationinput in;
	ctui_durationinput_init(&in, CTUI_DURATION_HOURS, CTUI_DURATION_MICROS,
				59 * M + 59 * S + 999 * MS + 999);
	in.segment = CTUI_DURATION_MICROS;
	CHECK(ctui_durationinput_step(&in, 1)); /* carries all the way up */
	CHECK(in.micro == H);
	CHECK(ctui_durationinput_step(&in, -1) &&
	      in.micro == 59 * M + 59 * S + 999 * MS + 999);
	in.segment = CTUI_DURATION_MINUTES;
	CHECK(ctui_durationinput_step(&in, 1) &&
	      in.micro == H + 59 * S + 999 * MS + 999);
	in.segment = CTUI_DURATION_MILLIS;
	CHECK(ctui_durationinput_step(&in, 10) &&
	      in.micro == H + 60 * S + 9 * MS + 999);
	/* it stops at zero */
	ctui_durationinput_set(&in, 3 * S);
	in.segment = CTUI_DURATION_SECONDS;
	CHECK(ctui_durationinput_step(&in, -10) && in.micro == 0);
	CHECK(!ctui_durationinput_step(&in, -1) && in.micro == 0);
	/* and at what the digits hold */
	ctui_durationinput_init(&in, CTUI_DURATION_MINUTES,
				CTUI_DURATION_MINUTES, 998 * M);
	CHECK(ctui_durationinput_step(&in, 5) && in.micro == 999 * M);
	CHECK(!ctui_durationinput_step(&in, 1));
}

static void test_typing(void)
{
	ctui_durationinput in;
	ctui_durationinput_init(&in, CTUI_DURATION_HOURS, CTUI_DURATION_MILLIS,
				0);
	/* three digits fill the hours, then the next unit takes over */
	type(&in, "12");
	CHECK(in.micro == 12 * H && in.segment == CTUI_DURATION_HOURS);
	type(&in, "5");
	CHECK(in.micro == 125 * H && in.segment == CTUI_DURATION_MINUTES);
	/* 7 cannot be followed in the tens of the minutes */
	type(&in, "7");
	CHECK(in.micro == 125 * H + 7 * M &&
	      in.segment == CTUI_DURATION_SECONDS);
	type(&in, "45");
	CHECK(in.micro == 125 * H + 7 * M + 45 * S &&
	      in.segment == CTUI_DURATION_MILLIS);
	type(&in, "2");
	CHECK(in.micro == 125 * H + 7 * M + 45 * S + 2 * MS &&
	      in.segment == CTUI_DURATION_MILLIS);
	type(&in, "50");
	CHECK(in.micro == 125 * H + 7 * M + 45 * S + 250 * MS);
	/* the last unit stays selected */
	CHECK(in.segment == CTUI_DURATION_MILLIS && in.typed == 0);
	/* 6 cannot be followed (60 is not a minute count): on to the seconds */
	in.segment = CTUI_DURATION_MINUTES;
	type(&in, "6");
	CHECK(in.micro == 125 * H + 6 * M + 45 * S + 250 * MS &&
	      in.segment == CTUI_DURATION_SECONDS);
	in.segment = CTUI_DURATION_MINUTES;
	type(&in, "3");
	CHECK(in.typed == 1 && in.entry == 3);
	type(&in, "9");
	CHECK(in.micro == 125 * H + 39 * M + 45 * S + 250 * MS);
	/* other keys end a half typed number */
	in.segment = CTUI_DURATION_HOURS;
	type(&in, "4");
	ctui_durationinput_key(&in, KEY_RIGHT);
	CHECK(in.typed == 0);
	/* typing the units of a field that starts below hours */
	ctui_durationinput_init(&in, CTUI_DURATION_SECONDS,
				CTUI_DURATION_SECONDS, 0);
	type(&in, "123");
	CHECK(in.micro == 123 * S);
	type(&in, "456");
	CHECK(in.micro == 456 * S);
}

static void test_keys(void)
{
	ctui_durationinput in;
	ctui_durationinput_init(&in, CTUI_DURATION_HOURS, CTUI_DURATION_SECONDS,
				0);
	CHECK(ctui_durationinput_key(&in, KEY_LEFT) == CTUI_DURATIONINPUT_NONE);
	CHECK(ctui_durationinput_key(&in, KEY_RIGHT) ==
		  CTUI_DURATIONINPUT_MOVED &&
	      in.segment == CTUI_DURATION_MINUTES);
	CHECK(ctui_durationinput_key(&in, ':') == CTUI_DURATIONINPUT_MOVED &&
	      in.segment == CTUI_DURATION_SECONDS);
	CHECK(ctui_durationinput_key(&in, ' ') == CTUI_DURATIONINPUT_NONE);
	CHECK(ctui_durationinput_key(&in, KEY_UP) ==
		  CTUI_DURATIONINPUT_CHANGED &&
	      in.micro == S);
	CHECK(ctui_durationinput_key(&in, KEY_DOWN) ==
		  CTUI_DURATIONINPUT_CHANGED &&
	      in.micro == 0);
	CHECK(ctui_durationinput_key(&in, KEY_DOWN) ==
	      CTUI_DURATIONINPUT_MOVED);
	CHECK(ctui_durationinput_key(&in, KEY_PPAGE) ==
		  CTUI_DURATIONINPUT_CHANGED &&
	      in.micro == 10 * S);
	CHECK(ctui_durationinput_key(&in, KEY_NPAGE) ==
		  CTUI_DURATIONINPUT_CHANGED &&
	      in.micro == 0);
	CHECK(ctui_durationinput_key(&in, KEY_LEFT) ==
	      CTUI_DURATIONINPUT_MOVED);
	CHECK(ctui_durationinput_key(&in, '\n') ==
	      CTUI_DURATIONINPUT_SUBMITTED);
	CHECK(ctui_durationinput_key(&in, '\r') ==
	      CTUI_DURATIONINPUT_SUBMITTED);
	CHECK(ctui_durationinput_key(&in, KEY_ENTER) ==
	      CTUI_DURATIONINPUT_SUBMITTED);
	CHECK(ctui_durationinput_key(&in, 27) == CTUI_DURATIONINPUT_CANCELLED);
	CHECK(ctui_durationinput_key(&in, 'x') == CTUI_DURATIONINPUT_NONE);
}

static void test_mouse(void)
{
	ctui_durationinput in;
	ctui_durationinput_init(&in, CTUI_DURATION_HOURS, CTUI_DURATION_MICROS,
				0);
	/* "012h 30m 05s 250ms 017us" with a margin on both sides */
	CHECK(ctui_durationinput_width(&in) == 26);
	CHECK(ctui_durationinput_segment_at(&in, 0) == -1);
	CHECK(ctui_durationinput_segment_at(&in, 1) == CTUI_DURATION_HOURS);
	CHECK(ctui_durationinput_segment_at(&in, 4) == CTUI_DURATION_HOURS);
	CHECK(ctui_durationinput_segment_at(&in, 5) == -1);
	CHECK(ctui_durationinput_segment_at(&in, 6) == CTUI_DURATION_MINUTES);
	CHECK(ctui_durationinput_segment_at(&in, 9) == -1);
	CHECK(ctui_durationinput_segment_at(&in, 10) == CTUI_DURATION_SECONDS);
	CHECK(ctui_durationinput_segment_at(&in, 14) == CTUI_DURATION_MILLIS);
	CHECK(ctui_durationinput_segment_at(&in, 20) == CTUI_DURATION_MICROS);
	CHECK(ctui_durationinput_segment_at(&in, 24) == CTUI_DURATION_MICROS);
	CHECK(ctui_durationinput_segment_at(&in, 25) == -1);
	CHECK(ctui_durationinput_segment_at(&in, 40) == -1);
	CHECK(ctui_durationinput_click(&in, 0, 14) ==
		  CTUI_DURATIONINPUT_MOVED &&
	      in.segment == CTUI_DURATION_MILLIS);
	CHECK(ctui_durationinput_click(&in, 0, 14) == CTUI_DURATIONINPUT_NONE);
	CHECK(ctui_durationinput_click(&in, 0, 5) == CTUI_DURATIONINPUT_NONE);
	CHECK(ctui_durationinput_click(&in, 1, 6) == CTUI_DURATIONINPUT_NONE);
	CHECK(ctui_durationinput_click(&in, 0, -1) == CTUI_DURATIONINPUT_NONE);
	CHECK(ctui_durationinput_click(&in, 0, 26) == CTUI_DURATIONINPUT_NONE);
	CHECK(ctui_durationinput_wheel(&in, 2) == CTUI_DURATIONINPUT_CHANGED &&
	      in.micro == 2 * MS);
	CHECK(ctui_durationinput_wheel(&in, -1) == CTUI_DURATIONINPUT_CHANGED &&
	      in.micro == MS);
	CHECK(ctui_durationinput_wheel(&in, 0) == CTUI_DURATIONINPUT_NONE);
	CHECK(ctui_durationinput_wheel(&in, -5) == CTUI_DURATIONINPUT_CHANGED &&
	      in.micro == 0);
	CHECK(ctui_durationinput_wheel(&in, -5) == CTUI_DURATIONINPUT_NONE);

	/* a narrower field */
	ctui_durationinput_init(&in, CTUI_DURATION_MINUTES,
				CTUI_DURATION_SECONDS, 0);
	CHECK(ctui_durationinput_width(&in) == 10);
	CHECK(ctui_durationinput_segment_at(&in, 1) == CTUI_DURATION_MINUTES);
	CHECK(ctui_durationinput_segment_at(&in, 6) == CTUI_DURATION_SECONDS);
}

static void row_text(int y, int x, int n, char *out)
{
	mvinnstr(y, x, out, n);
	out[n] = '\0';
}

static void test_draw(void)
{
	screen_open(24, 80);
	ctui_durationinput in;
	ctui_durationinput_init(&in, CTUI_DURATION_HOURS, CTUI_DURATION_MICROS,
				12 * H + 30 * M + 5 * S + 250 * MS + 17);
	char row[64];
	screen_clear();
	ctui_durationinput_draw(&in, 2, 3, false);
	row_text(2, 3, 26, row);
	CHECK_STR(row, " 012h 30m 05s 250ms 017us ");
	ctui_durationinput_init(&in, CTUI_DURATION_MINUTES,
				CTUI_DURATION_SECONDS, 90 * M);
	screen_clear();
	ctui_durationinput_draw(&in, 0, 0, true);
	row_text(0, 0, 11, row);
	CHECK_STR(row, " 090m 00s  ");
	/* the unit under the cursor is reversed when focused */
	cchar_t cell;
	wchar_t wch[CCHARW_MAX];
	attr_t attrs;
	short pair;
	in.segment = CTUI_DURATION_SECONDS;
	screen_clear();
	ctui_durationinput_draw(&in, 0, 0, true);
	mvin_wch(0, 7, &cell);
	getcchar(&cell, wch, &attrs, &pair, NULL);
	CHECK(attrs & A_REVERSE);
	mvin_wch(0, 1, &cell);
	getcchar(&cell, wch, &attrs, &pair, NULL);
	CHECK(!(attrs & A_REVERSE));
	mvin_wch(0, 9, &cell); /* the suffix is not reversed */
	getcchar(&cell, wch, &attrs, &pair, NULL);
	CHECK(!(attrs & A_REVERSE));
	ctui_style style = {0};
	in.style = &style;
	ctui_durationinput_draw(&in, 0, 0, false);
	endwin();
}

int main(void)
{
	RUN(test_units);
	RUN(test_parse);
	RUN(test_format);
	RUN(test_init_and_set);
	RUN(test_max);
	RUN(test_step);
	RUN(test_typing);
	RUN(test_keys);
	RUN(test_mouse);
	RUN(test_draw);
	return CHECK_DONE();
}
