/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * test_numinput.c - number field: parsing, formatting, overflow, stepping,
 * typing, mouse and drawing.
 */
#include "check.h"
#include "screen.h"

#include <curses.h>
#include <locale.h>
#include <math.h>

#include <ctui/numinput.h>

static void type(ctui_numinput *in, const char *keys)
{
	for (; *keys; keys++)
		ctui_numinput_key(in, *keys);
}

static void test_parse(void)
{
	double v = 99;
	CHECK(ctui_numinput_parse("0", 0, &v) && v == 0);
	CHECK(ctui_numinput_parse("42", 0, &v) && v == 42);
	CHECK(ctui_numinput_parse("-42", 0, &v) && v == -42);
	CHECK(ctui_numinput_parse("+42", 0, &v) && v == 42);
	CHECK(ctui_numinput_parse("3.14", 2, &v) && fabs(v - 3.14) < 1e-12);
	CHECK(ctui_numinput_parse("-0.5", 2, &v) && v == -0.5);
	CHECK(ctui_numinput_parse("3.1", 4, &v) &&
	      fabs(v - 3.1) < 1e-12); /* fewer digits is fine */
	CHECK(ctui_numinput_parse("007", 0, &v) && v == 7);
	CHECK(ctui_numinput_parse("-0", 0, &v) && v == 0 && !signbit(v));
	CHECK(ctui_numinput_parse("-0.00", 2, &v) && v == 0 && !signbit(v));
	CHECK(ctui_numinput_parse("0.1", 1, &v) && v == 0.1);
	CHECK(ctui_numinput_parse("12345678.12345678", 8, &v) ==
	      false); /* 16 digits */
	CHECK(ctui_numinput_parse("1234567.12345678", 8, &v) &&
	      fabs(v - 1234567.12345678) < 1e-6);

	v = 99;
	const char *bad[] = {"",	 "-",	 "+",		".",   "-.",
			     ".5",	 "5.",	 "1.5",		"1,5", "1e3",
			     "1E3",	 " 1",	 "1 ",		"--1", "+-1",
			     "1-",	 "0x10", "1_000",	"abc", "1.2.3",
			     "\xd9\xa1", "1\n",	 "\xef\xbc\x91"};
	for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
		CHECK(!ctui_numinput_parse(bad[i], 0, &v));
		CHECK(v == 99);
	}
	CHECK(!ctui_numinput_parse(
	    "1.5", 0, &v)); /* more fraction digits than decimals */
	CHECK(!ctui_numinput_parse("1.234", 2, &v));
	CHECK(!ctui_numinput_parse("1", -1, &v) &&
	      !ctui_numinput_parse("1", 9, &v));
	CHECK(!ctui_numinput_parse(NULL, 0, &v));
	CHECK(ctui_numinput_parse("5", 0, NULL));
}

static void test_parse_overflow(void)
{
	double v = 99;
	CHECK(ctui_numinput_parse("999999999999999", 0, &v) &&
	      v == 999999999999999.0);
	CHECK(!ctui_numinput_parse("1000000000000000", 0, &v)); /* 16 digits */
	CHECK(!ctui_numinput_parse("-1000000000000000", 0, &v));
	CHECK(!ctui_numinput_parse("99999999999999999999999999999999999999", 0,
				   &v));
	CHECK(ctui_numinput_parse("0000000000000000000000000000000000000001", 0,
				  &v) &&
	      v == 1);
	CHECK(ctui_numinput_parse("0.000000000000000001", 8, &v) ==
	      false); /* too many decimals */
	CHECK(!ctui_numinput_parse("1.00000000000000000", 8, &v));
	char huge[4096];
	memset(huge, '9', sizeof huge - 1);
	huge[sizeof huge - 1] = '\0';
	v = 99;
	CHECK(!ctui_numinput_parse(huge, 0, &v) && v == 99);
	CHECK(ctui_numinput_parse("0.99999999", 8, &v) && v < 1 &&
	      v > 0.99999998);
}

static void test_format(void)
{
	char b[32];
	CHECK_STR(ctui_numinput_format(0, 0, b, sizeof b), "0");
	CHECK_STR(ctui_numinput_format(42, 0, b, sizeof b), "42");
	CHECK_STR(ctui_numinput_format(-42, 0, b, sizeof b), "-42");
	CHECK_STR(ctui_numinput_format(3.14159, 2, b, sizeof b), "3.14");
	CHECK_STR(ctui_numinput_format(3.145, 1, b, sizeof b), "3.1");
	CHECK_STR(ctui_numinput_format(2.5, 0, b, sizeof b),
		  "3"); /* rounds half away */
	CHECK_STR(ctui_numinput_format(-2.5, 0, b, sizeof b), "-3");
	CHECK_STR(ctui_numinput_format(0.05, 2, b, sizeof b), "0.05");
	CHECK_STR(ctui_numinput_format(-0.001, 2, b, sizeof b),
		  "0.00"); /* no "-0.00" */
	CHECK_STR(ctui_numinput_format(-0.0, 0, b, sizeof b), "0");
	CHECK_STR(ctui_numinput_format(1, 3, b, sizeof b), "1.000");
	CHECK_STR(ctui_numinput_format(123456789.5, 1, b, sizeof b),
		  "123456789.5");
	CHECK_STR(ctui_numinput_format(999999999999999.0, 0, b, sizeof b),
		  "999999999999999");
	CHECK_STR(ctui_numinput_format(0.00000001, 8, b, sizeof b),
		  "0.00000001");
	CHECK(ctui_numinput_format(1e15, 0, b, sizeof b) == NULL);
	CHECK(ctui_numinput_format(-1e300, 0, b, sizeof b) == NULL);
	CHECK(ctui_numinput_format(NAN, 0, b, sizeof b) == NULL);
	CHECK(ctui_numinput_format(INFINITY, 0, b, sizeof b) == NULL);
	CHECK(ctui_numinput_format(-INFINITY, 2, b, sizeof b) == NULL);
	CHECK(ctui_numinput_format(1, -1, b, sizeof b) == NULL &&
	      ctui_numinput_format(1, 9, b, sizeof b) == NULL);
	CHECK(ctui_numinput_format(12345, 0, b, 5) ==
	      NULL); /* no room for the NUL */
	CHECK_STR(ctui_numinput_format(12345, 0, b, 6), "12345");
	CHECK(ctui_numinput_format(-12345, 0, b, 6) == NULL);
	CHECK(ctui_numinput_format(1.5, 1, NULL, 8) == NULL);

	/* format and parse agree */
	for (int d = 0; d <= 4; d++)
		for (int k = 0; k < 19; k++) {
			double x = -123.456 + 17.771 * k;
			double back = 0;

			CHECK(ctui_numinput_format(x, d, b, sizeof b) &&
			      ctui_numinput_parse(b, d, &back));
			CHECK(fabs(back - x) <= 0.5 / pow(10, d) + 1e-9);
		}
}

static void test_locale_independent(void)
{
	/* A locale with a decimal comma must not change anything. */
	const char *names[] = {"fr_FR.UTF-8", "de_DE.UTF-8", "fr_FR", "de_DE"};
	for (size_t i = 0; i < sizeof names / sizeof names[0]; i++)
		if (setlocale(LC_NUMERIC, names[i]))
			break;
	char b[32];
	double v;
	CHECK_STR(ctui_numinput_format(1234.5, 1, b, sizeof b), "1234.5");
	CHECK(ctui_numinput_parse("1234.5", 1, &v) && v == 1234.5);
	CHECK(!ctui_numinput_parse("1234,5", 1, &v));
	setlocale(LC_NUMERIC, "C");
}

static void test_init(void)
{
	ctui_numinput in;
	ctui_numinput_init(&in, 0, 100, 5, 40, 0);
	CHECK(in.value == 40 && in.min == 0 && in.max == 100 && in.step == 5 &&
	      !in.editing);
	ctui_numinput_init(&in, 0, 100, 5, 400, 0);
	CHECK(in.value == 100);
	ctui_numinput_init(&in, 0, 100, 5, -4, 0);
	CHECK(in.value == 0);
	ctui_numinput_init(&in, 10, 5, 0, 7, 1); /* max below min, no step */
	CHECK(in.max == 10 && in.value == 10 && in.step == 0.1);
	ctui_numinput_init(&in, 0, 1, 0.1, 0.123456, 2);
	CHECK(in.value == 0.12 && in.decimals == 2);
	ctui_numinput_init(&in, 0, 1, 1, 0, 40);
	CHECK(in.decimals == CTUI_NUMINPUT_MAX_DECIMALS);
	ctui_numinput_init(&in, 0, 1, 1, 0, -2);
	CHECK(in.decimals == 0);
	ctui_numinput_init(&in, -1e300, 1e300, 1, 5,
			   0); /* limited to what 15 digits hold */
	CHECK(in.min == -999999999999999.0 && in.max == 999999999999999.0);
	ctui_numinput_init(&in, NAN, INFINITY, NAN, NAN, 0);
	CHECK(isfinite(in.min) && isfinite(in.max) && in.step > 0 &&
	      isfinite(in.value));
	ctui_numinput_init(&in, -1e300, 1e300, 1, 5,
			   8); /* fewer digits before the point */
	CHECK(in.max == 9999999.99999999);
}

static void test_set_add(void)
{
	ctui_numinput in;
	ctui_numinput_init(&in, -10, 10, 0.1, 0, 1);
	CHECK(ctui_numinput_set(&in, 3.14159) && in.value == 3.1);
	CHECK(!ctui_numinput_set(&in, 3.14) && in.value == 3.1);
	CHECK(ctui_numinput_set(&in, 99) && in.value == 10);
	CHECK(ctui_numinput_set(&in, -99) && in.value == -10);
	CHECK(!ctui_numinput_set(&in, NAN) && in.value == -10);
	CHECK(!ctui_numinput_set(&in, INFINITY) && in.value == -10);
	CHECK(ctui_numinput_set(&in, 0) && in.value == 0);
	/* decimal steps do not drift */
	for (int i = 0; i < 30; i++)
		ctui_numinput_add(&in, 1);
	CHECK(in.value == 3.0);
	for (int i = 0; i < 3; i++)
		ctui_numinput_add(&in, -1);
	CHECK(in.value == 2.7);
	CHECK(ctui_numinput_add(&in, 1000) && in.value == 10);
	CHECK(!ctui_numinput_add(&in, 1));
	CHECK(ctui_numinput_add(&in, -1000000) && in.value == -10);
	CHECK(ctui_numinput_add(&in, 0x7fffffffffffffffL) &&
	      in.value == 10); /* saturates */
	CHECK(ctui_numinput_add(&in, -0x7fffffffffffffffL - 1) &&
	      in.value == -10);
	CHECK(!ctui_numinput_add(&in, 0));

	ctui_numinput_init(&in, 0, 100, 2.5, 0,
			   1); /* a step of two and a half tenths... */
	ctui_numinput_add(&in, 1);
	CHECK(in.value == 2.5);
	ctui_numinput_init(&in, 0, 100, 0.04, 0,
			   1); /* finer than the decimals: one unit */
	ctui_numinput_add(&in, 3);
	CHECK(in.value == 0.3);
	ctui_numinput_init(&in, 0, 10, 3, 0, 0);
	ctui_numinput_add(&in, 3);
	ctui_numinput_add(&in, 1);
	CHECK(in.value == 10);
}

static void test_arrow_keys(void)
{
	ctui_numinput in;
	ctui_numinput_init(&in, 0, 100, 5, 40, 0);
	CHECK(ctui_numinput_key(&in, KEY_UP) == CTUI_NUMINPUT_CHANGED &&
	      in.value == 45);
	CHECK(ctui_numinput_key(&in, KEY_DOWN) == CTUI_NUMINPUT_CHANGED &&
	      in.value == 40);
	CHECK(ctui_numinput_key(&in, KEY_PPAGE) == CTUI_NUMINPUT_CHANGED &&
	      in.value == 90);
	CHECK(ctui_numinput_key(&in, KEY_PPAGE) == CTUI_NUMINPUT_CHANGED &&
	      in.value == 100);
	CHECK(ctui_numinput_key(&in, KEY_PPAGE) == CTUI_NUMINPUT_NONE &&
	      in.value == 100);
	CHECK(ctui_numinput_key(&in, KEY_UP) == CTUI_NUMINPUT_NONE);
	CHECK(ctui_numinput_key(&in, KEY_NPAGE) == CTUI_NUMINPUT_CHANGED &&
	      in.value == 50);
	CHECK(ctui_numinput_key(&in, KEY_HOME) == CTUI_NUMINPUT_CHANGED &&
	      in.value == 0);
	CHECK(ctui_numinput_key(&in, KEY_HOME) == CTUI_NUMINPUT_NONE);
	CHECK(ctui_numinput_key(&in, KEY_DOWN) == CTUI_NUMINPUT_NONE);
	CHECK(ctui_numinput_key(&in, KEY_END) == CTUI_NUMINPUT_CHANGED &&
	      in.value == 100);
	CHECK(ctui_numinput_key(&in, '\n') == CTUI_NUMINPUT_SUBMITTED);
	CHECK(ctui_numinput_key(&in, 27) == CTUI_NUMINPUT_CANCELLED);
	CHECK(ctui_numinput_key(&in, 'x') == CTUI_NUMINPUT_NONE);
	CHECK(ctui_numinput_key(&in, KEY_LEFT) == CTUI_NUMINPUT_NONE);
}

static void test_typing(void)
{
	ctui_numinput in;
	ctui_numinput_init(&in, 0, 100, 1, 40, 0);
	CHECK(ctui_numinput_key(&in, '7') == CTUI_NUMINPUT_CHANGED);
	CHECK(in.editing && in.value == 7); /* replaces the number */
	CHECK(ctui_numinput_key(&in, '2') == CTUI_NUMINPUT_CHANGED &&
	      in.value == 72);
	CHECK(ctui_numinput_key(&in, '5') == CTUI_NUMINPUT_NONE &&
	      in.value == 72); /* 725 > max */
	CHECK_STR(in.edit, "72");
	CHECK(ctui_numinput_key(&in, '.') == CTUI_NUMINPUT_NONE); /* integer */
	CHECK(ctui_numinput_key(&in, '-') ==
	      CTUI_NUMINPUT_NONE); /* min is not negative */
	CHECK(ctui_numinput_key(&in, KEY_BACKSPACE) == CTUI_NUMINPUT_CHANGED &&
	      in.value == 7);
	CHECK(ctui_numinput_key(&in, 127) == CTUI_NUMINPUT_NONE ||
	      in.value == 7);
	CHECK(in.editing && in.edit[0] == '\0');
	CHECK(ctui_numinput_key(&in, '\n') == CTUI_NUMINPUT_SUBMITTED &&
	      !in.editing && in.value == 7);

	char b[16];
	CHECK_STR(ctui_numinput_text(&in, b, sizeof b), "7");
	type(&in, "100");
	CHECK(in.value == 100);
	CHECK(ctui_numinput_key(&in, '0') == CTUI_NUMINPUT_NONE);
	ctui_numinput_key(&in, 27); /* Esc during an edit: back to 7 */
	CHECK(!in.editing && in.value == 7);
	CHECK(ctui_numinput_key(&in, 27) == CTUI_NUMINPUT_CANCELLED);

	/* leading zeros, and a number below min is raised */
	ctui_numinput_init(&in, 10, 100, 1, 50, 0);
	type(&in, "0005");
	CHECK(in.value == 10); /* 5 < min: shown as min */
	CHECK_STR(ctui_numinput_text(&in, b, sizeof b), "0005");
	ctui_numinput_commit(&in);
	CHECK_STR(ctui_numinput_text(&in, b, sizeof b), "10");
	type(&in, "000000000000000000000000000"); /* the buffer bound holds */
	CHECK(strlen(in.edit) < CTUI_NUMINPUT_EDIT);
}

static void test_typing_decimals(void)
{
	ctui_numinput in;
	ctui_numinput_init(&in, -50, 50, 0.5, 0, 2);
	type(&in, "-");
	CHECK(in.editing &&
	      in.value == 0); /* a lone sign is not a number yet */
	type(&in, "12.");
	CHECK(in.value == -12);
	type(&in, "."); /* a second point */
	CHECK_STR(in.edit, "-12.");
	type(&in, "34");
	CHECK(in.value == -12.34);
	type(&in, "5"); /* third decimal */
	CHECK_STR(in.edit, "-12.34");
	type(&in, "-+"); /* a sign only first */
	CHECK_STR(in.edit, "-12.34");
	char b[16];
	CHECK_STR(ctui_numinput_text(&in, b, sizeof b), "-12.34");
	ctui_numinput_commit(&in);
	CHECK_STR(ctui_numinput_text(&in, b, sizeof b), "-12.34");

	type(&in, ".5"); /* ".5" is 0.5 */
	CHECK(in.value == 0.5);
	type(&in, "-");
	CHECK_STR(in.edit, ".5");
	ctui_numinput_key(&in, 27);
	CHECK(in.value == -12.34);

	type(&in, "-60"); /* below min: refused at the 6 */
	CHECK_STR(in.edit, "-6");
	type(&in, "1");
	CHECK_STR(in.edit, "-6");
	ctui_numinput_key(&in, 27);

	type(&in, "+3");
	CHECK(in.value == 3 && in.edit[0] == '+');
	ctui_numinput_commit(&in);

	/* digits are bounded: 15 significant at most */
	ctui_numinput_init(&in, -1e300, 1e300, 1, 0, 2);
	type(&in, "1234567890123"); /* 13 integer digits */
	CHECK(in.value == 1234567890123.0 || in.value == 9999999999999.0 ||
	      in.editing);
	type(&in, "12345");
	CHECK(strlen(in.edit) <= 15);
	ctui_numinput_key(&in, 27);

	/* Backspace on a number that is not being edited starts from its text
	 */
	ctui_numinput_init(&in, 0, 1000, 1, 123, 0);
	CHECK(ctui_numinput_key(&in, KEY_BACKSPACE) == CTUI_NUMINPUT_CHANGED);
	CHECK(in.value == 12 && in.editing);
	ctui_numinput_key(&in, KEY_BACKSPACE);
	ctui_numinput_key(&in, KEY_BACKSPACE);
	CHECK(in.value == 1 && in.edit[0] == '\0');
	ctui_numinput_key(&in, KEY_BACKSPACE);
	CHECK(in.value == 1); /* empty text: value stays */
	ctui_numinput_key(&in, 27);
	CHECK(in.value == 123);

	/* Up ends the edit and steps from the live value */
	type(&in, "40");
	CHECK(ctui_numinput_key(&in, KEY_UP) == CTUI_NUMINPUT_CHANGED);
	CHECK(!in.editing && in.value == 41);
	type(&in, "41"); /* same as before: commit reports no change */
	CHECK(!ctui_numinput_commit(&in));
	CHECK(!ctui_numinput_commit(&in));
	type(&in, "9");
	CHECK(ctui_numinput_commit(&in) && in.value == 9);
}

static void test_mouse(void)
{
	ctui_numinput in;
	ctui_numinput_init(&in, 0, 100, 5, 40, 0);
	CHECK(ctui_numinput_click(&in, 12, 0, 0) == CTUI_NUMINPUT_CHANGED &&
	      in.value == 35);
	CHECK(ctui_numinput_click(&in, 12, 0, 11) == CTUI_NUMINPUT_CHANGED &&
	      in.value == 40);
	CHECK(ctui_numinput_click(&in, 12, 0, 5) == CTUI_NUMINPUT_NONE);
	CHECK(ctui_numinput_click(&in, 12, 1, 0) == CTUI_NUMINPUT_NONE);
	CHECK(ctui_numinput_click(&in, 12, 0, 12) == CTUI_NUMINPUT_NONE);
	CHECK(ctui_numinput_click(&in, 12, 0, -1) == CTUI_NUMINPUT_NONE);
	CHECK(ctui_numinput_click(&in, 2, 0, 0) ==
	      CTUI_NUMINPUT_NONE); /* too narrow */
	CHECK(ctui_numinput_wheel(&in, 2) == CTUI_NUMINPUT_CHANGED &&
	      in.value == 50);
	CHECK(ctui_numinput_wheel(&in, -1) == CTUI_NUMINPUT_CHANGED &&
	      in.value == 45);
	CHECK(ctui_numinput_wheel(&in, 0) == CTUI_NUMINPUT_NONE);
	CHECK(ctui_numinput_wheel(&in, 1000) == CTUI_NUMINPUT_CHANGED &&
	      in.value == 100);
	CHECK(ctui_numinput_wheel(&in, 1) == CTUI_NUMINPUT_NONE);
	type(&in, "7"); /* a click ends the edit */
	CHECK(ctui_numinput_click(&in, 12, 0, 11) == CTUI_NUMINPUT_CHANGED &&
	      !in.editing && in.value == 12);
}

static void test_draw(void)
{
	screen_open(24, 80);
	ctui_numinput in;
	ctui_numinput_init(&in, 0, 100, 0.5, 12.5, 1);
	in.unit = "kg";
	screen_clear();
	ctui_numinput_draw(&in, 2, 3, 16, false);
	CHECK(screen_has_text("12.5 kg") && screen_has_text("-") &&
	      screen_has_text("+"));
	type(&in, "8");
	screen_clear();
	ctui_numinput_draw(&in, 2, 3, 16, true);
	CHECK(screen_has_text("8") && !screen_has_text("12.5"));
	in.unit = NULL;
	screen_clear();
	ctui_numinput_draw(&in, 2, 3, 16, true);
	ctui_numinput_draw(&in, 2, 3, 3, true);
	ctui_numinput_draw(&in, 2, 3, 2,
			   true); /* too narrow for the buttons: no crash */
	ctui_numinput_draw(&in, 2, 3, 0, true);
	ctui_numinput_key(&in, 27);
	in.unit = "a rather long unit name";
	screen_clear();
	ctui_numinput_draw(&in, 2, 3, 10,
			   true); /* clipped from the left, no crash */
	CHECK(screen_has_text("name"));

	ctui_style hot = {0};
	hot.attr[CTUI_PAIR_SELECTION] = ctui_attr_color(CTUI_C_RED, -1);
	in.style = &hot;
	screen_clear();
	ctui_numinput_draw(&in, 2, 3, 16, true);
	CHECK(screen_has_color(ctui_attr_color(CTUI_C_RED, -1)));
	in.style = NULL;
	screen_clear();
	ctui_numinput_draw(&in, 2, 3, 16, true);
	CHECK(!screen_has_color(ctui_attr_color(CTUI_C_RED, -1)));
}

int main(void)
{
	RUN(test_parse);
	RUN(test_parse_overflow);
	RUN(test_format);
	RUN(test_locale_independent);
	RUN(test_init);
	RUN(test_set_add);
	RUN(test_arrow_keys);
	RUN(test_typing);
	RUN(test_typing_decimals);
	RUN(test_mouse);
	RUN(test_draw);
	return CHECK_DONE();
}
