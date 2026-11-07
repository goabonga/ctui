/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * durationinput.c - duration field with per-unit editing (durationinput.h).
 */
#include <ctui/durationinput.h>

#include <curses.h>
#include <stdio.h>
#include <string.h>

#include <ctui/text.h>
#include <ctui/theme.h>

#define UNITS 5
#define PARSE_LIMIT 4000000000000000000LL

static const long long UNIT_MICROS[UNITS] = {3600000000LL, 60000000LL,
					     1000000LL, 1000LL, 1LL};
static const long RANGE[UNITS] = {0, 60, 60, 1000, 1000}; /* 0: unbounded */
static const int SMALL_WIDTH[UNITS] = {0, 2, 2, 3, 3};
static const char *const SUFFIX[UNITS] = {"h", "m", "s", "ms", "us"};

static bool known(int unit)
{
	return unit >= CTUI_DURATION_HOURS && unit <= CTUI_DURATION_MICROS;
}

long long ctui_duration_unit_micros(ctui_duration_unit unit)
{
	return known((int)unit) ? UNIT_MICROS[unit] : 0;
}

static long long power10(int n)
{
	long long v = 1;
	while (n-- > 0)
		v *= 10;
	return v;
}

/* ---- pure ------------------------------------------------------- */

bool ctui_durationinput_parse(const char *s, long long *out)
{
	if (!s)
		return false;
	long long total = 0;
	int last = -1;
	for (;;) {
		while (*s == ' ')
			s++;
		if (!*s)
			break;
		if (*s < '0' || *s > '9')
			return false;
		long long v = 0;
		int digits = 0;
		for (; *s >= '0' && *s <= '9'; s++, digits++) {
			if (digits >= 18)
				return false;
			v = v * 10 + (*s - '0');
		}
		int unit = -1;
		if (s[0] == 'm' && s[1] == 's') {
			unit = CTUI_DURATION_MILLIS;
			s += 2;
		} else if (s[0] == 'u' && s[1] == 's') {
			unit = CTUI_DURATION_MICROS;
			s += 2;
		} else if (s[0] == 'h' || s[0] == 'm' || s[0] == 's') {
			unit = s[0] == 'h'   ? CTUI_DURATION_HOURS
			       : s[0] == 'm' ? CTUI_DURATION_MINUTES
					     : CTUI_DURATION_SECONDS;
			s++;
		}
		if (unit <= last ||
		    v > (PARSE_LIMIT - total) / UNIT_MICROS[unit])
			return false;
		total += v * UNIT_MICROS[unit];
		last = unit;
	}
	if (last < 0)
		return false;
	if (out)
		*out = total;
	return true;
}

char *ctui_durationinput_format(long long micro, ctui_duration_unit largest,
				ctui_duration_unit smallest, int digits,
				char *buf, size_t cap)
{
	if (!buf || cap < CTUI_DURATION_TEXT || micro < 0 ||
	    !known((int)largest) || !known((int)smallest) ||
	    largest > smallest || digits < 1 ||
	    digits > CTUI_DURATION_MAX_DIGITS)
		return NULL;
	size_t n = 0;
	buf[0] = '\0';
	for (int u = (int)largest; u <= (int)smallest; u++) {
		long long v = micro / UNIT_MICROS[u];
		if (u != (int)largest)
			v %= RANGE[u];
		int width = u == (int)largest ? digits : SMALL_WIDTH[u];
		int w = snprintf(buf + n, cap - n, "%s%0*lld%s", n ? " " : "",
				 width, v, SUFFIX[u]);
		if (w < 0 || (size_t)w >= cap - n)
			return NULL;
		n += (size_t)w;
	}
	return buf;
}

/* ---- layout ----------------------------------------------------- */

static int unit_width(const ctui_durationinput *in, int unit)
{
	return unit == in->largest ? in->digits : SMALL_WIDTH[unit];
}

/* First column and number of digits of a unit, relative to the field. */
static void unit_columns(const ctui_durationinput *in, int unit, int *x,
			 int *digits)
{
	int col = 1;
	for (int u = in->largest; u < unit; u++)
		col += unit_width(in, u) + (int)strlen(SUFFIX[u]) + 1;
	*x = col;
	*digits = unit_width(in, unit);
}

int ctui_durationinput_width(const ctui_durationinput *in)
{
	int x, digits;
	unit_columns(in, in->smallest, &x, &digits);
	return x + digits + (int)strlen(SUFFIX[in->smallest]) + 1;
}

int ctui_durationinput_segment_at(const ctui_durationinput *in, int x)
{
	for (int u = in->largest; u <= in->smallest; u++) {
		int start, digits;
		unit_columns(in, u, &start, &digits);
		if (x >= start && x < start + digits + (int)strlen(SUFFIX[u]))
			return u;
	}
	return -1;
}

/* ---- value ------------------------------------------------------ */

long long ctui_durationinput_capacity(const ctui_durationinput *in)
{
	long long smallest = UNIT_MICROS[in->smallest];
	long long top =
	    in->has_max ? in->max
			: power10(in->digits) * UNIT_MICROS[in->largest] - 1;
	return top / smallest * smallest;
}

static void reset_entry(ctui_durationinput *in)
{
	in->typed = 0;
	in->entry = 0;
}

void ctui_durationinput_init(ctui_durationinput *in, ctui_duration_unit largest,
			     ctui_duration_unit smallest, long long micro)
{
	if (!known((int)largest) || !known((int)smallest) ||
	    largest > smallest) {
		largest = CTUI_DURATION_HOURS;
		smallest = CTUI_DURATION_SECONDS;
	}
	*in = (ctui_durationinput){.largest = (int)largest,
				   .smallest = (int)smallest,
				   .digits = CTUI_DURATION_DEFAULT_DIGITS,
				   .segment = (int)largest};
	long long cap = ctui_durationinput_capacity(in);
	micro = micro < 0 ? 0 : micro > cap ? cap : micro;
	in->micro = micro / UNIT_MICROS[smallest] * UNIT_MICROS[smallest];
}

bool ctui_durationinput_set(ctui_durationinput *in, long long micro)
{
	if (micro < 0 || micro > ctui_durationinput_capacity(in))
		return false;
	in->micro =
	    micro / UNIT_MICROS[in->smallest] * UNIT_MICROS[in->smallest];
	reset_entry(in);
	return true;
}

bool ctui_durationinput_set_max(ctui_durationinput *in, const long long *max)
{
	int digits = CTUI_DURATION_DEFAULT_DIGITS;
	if (max) {
		if (*max < 0)
			return false;
		long long top = *max / UNIT_MICROS[in->largest];
		digits = 1;
		for (long long t = top; t >= 10; t /= 10)
			digits++;
		if (digits > CTUI_DURATION_MAX_DIGITS)
			return false;
	}
	in->has_max = max != NULL;
	in->max = max ? *max : 0;
	in->digits = digits;
	long long cap = ctui_durationinput_capacity(in);
	if (in->micro > cap)
		in->micro = cap;
	reset_entry(in);
	return true;
}

static long long segment_value(const ctui_durationinput *in, int unit)
{
	long long v = in->micro / UNIT_MICROS[unit];
	return unit == in->largest ? v : v % RANGE[unit];
}

bool ctui_durationinput_step(ctui_durationinput *in, int delta)
{
	reset_entry(in);
	long long cap = ctui_durationinput_capacity(in);
	long long next =
	    in->micro + (long long)delta * UNIT_MICROS[in->segment];
	next = next < 0 ? 0 : next > cap ? cap : next;
	bool changed = next != in->micro;
	in->micro = next;
	return changed;
}

/* ---- typing ----------------------------------------------------- */

static ctui_durationinput_event move_segment(ctui_durationinput *in, int to)
{
	reset_entry(in);
	if (to < in->largest || to > in->smallest || to == in->segment)
		return CTUI_DURATIONINPUT_NONE;
	in->segment = to;
	return CTUI_DURATIONINPUT_MOVED;
}

static ctui_durationinput_event type_digit(ctui_durationinput *in, int d)
{
	int unit = in->segment;
	int max_digits = unit_width(in, unit);
	long long limit =
	    unit == in->largest ? power10(in->digits) - 1 : RANGE[unit] - 1;
	long long candidate = in->typed ? in->entry * 10 + d : d;
	if (in->typed &&
	    candidate > limit) { /* cannot continue: start over here */
		candidate = d;
		in->typed = 0;
	}
	in->entry = candidate;
	in->typed++;
	long long before = in->micro;
	long long next = in->micro + (candidate - segment_value(in, unit)) *
					 UNIT_MICROS[unit];
	long long cap = ctui_durationinput_capacity(in);
	in->micro = next < 0 ? 0 : next > cap ? cap : next;
	if (in->typed >= max_digits || candidate * 10 > limit) {
		reset_entry(in);
		if (unit < in->smallest)
			in->segment++;
	}
	return before != in->micro ? CTUI_DURATIONINPUT_CHANGED
				   : CTUI_DURATIONINPUT_MOVED;
}

ctui_durationinput_event ctui_durationinput_key(ctui_durationinput *in, int key)
{
	switch (key) {
	case KEY_LEFT:
		return move_segment(in, in->segment - 1);
	case KEY_RIGHT:
	case ':':
	case ' ':
		return move_segment(in, in->segment + 1);
	case KEY_UP:
		return ctui_durationinput_step(in, 1)
			   ? CTUI_DURATIONINPUT_CHANGED
			   : CTUI_DURATIONINPUT_MOVED;
	case KEY_DOWN:
		return ctui_durationinput_step(in, -1)
			   ? CTUI_DURATIONINPUT_CHANGED
			   : CTUI_DURATIONINPUT_MOVED;
	case KEY_PPAGE:
		return ctui_durationinput_step(in, 10)
			   ? CTUI_DURATIONINPUT_CHANGED
			   : CTUI_DURATIONINPUT_MOVED;
	case KEY_NPAGE:
		return ctui_durationinput_step(in, -10)
			   ? CTUI_DURATIONINPUT_CHANGED
			   : CTUI_DURATIONINPUT_MOVED;
	case '\n':
	case '\r':
	case KEY_ENTER:
		reset_entry(in);
		return CTUI_DURATIONINPUT_SUBMITTED;
	case 27:
		reset_entry(in);
		return CTUI_DURATIONINPUT_CANCELLED;
	default:
		if (key >= '0' && key <= '9')
			return type_digit(in, key - '0');
		return CTUI_DURATIONINPUT_NONE;
	}
}

/* ---- mouse ------------------------------------------------------ */

ctui_durationinput_event ctui_durationinput_click(ctui_durationinput *in, int y,
						  int x)
{
	if (y != 0 || x < 0 || x >= ctui_durationinput_width(in))
		return CTUI_DURATIONINPUT_NONE;
	int unit = ctui_durationinput_segment_at(in, x);
	return unit < 0 ? CTUI_DURATIONINPUT_NONE : move_segment(in, unit);
}

ctui_durationinput_event ctui_durationinput_wheel(ctui_durationinput *in,
						  int notches)
{
	if (notches == 0)
		return CTUI_DURATIONINPUT_NONE;
	return ctui_durationinput_step(in, notches) ? CTUI_DURATIONINPUT_CHANGED
						    : CTUI_DURATIONINPUT_NONE;
}

/* ---- drawing ---------------------------------------------------- */

void ctui_durationinput_draw(const ctui_durationinput *in, int y, int x,
			     bool focused)
{
	ctui_style_begin(in->style);
	int attr = ctui_attr(focused ? CTUI_PAIR_SELECTION : CTUI_PAIR_DIALOG,
			     focused);
	int width = ctui_durationinput_width(in);
	ctui_fill(y, x, width, attr);
	char text[CTUI_DURATION_TEXT];
	if (ctui_durationinput_format(
		in->micro,
		// NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
		(ctui_duration_unit)in->largest,
		(ctui_duration_unit)in->smallest, in->digits, text,
		sizeof text))
		ctui_put(y, x + 1, width - 2, text, attr);
	if (focused && known(in->segment)) {
		int start, digits;
		unit_columns(in, in->segment, &start, &digits);
		char part[CTUI_DURATION_MAX_DIGITS + 1];
		snprintf(part, sizeof part, "%.*s", digits, text + start - 1);
		ctui_put(y, x + start, digits, part,
			 (int)((unsigned)attr | (unsigned)A_REVERSE));
	}
	ctui_style_end();
}
