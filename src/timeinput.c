/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * timeinput.c - time of day field with per-segment editing (timeinput.h).
 */
#include <ctui/timeinput.h>

#include <curses.h>
#include <stdio.h>
#include <string.h>

#include <ctui/text.h>
#include <ctui/theme.h>

#define MICROS_PER_HOUR 3600000000LL
#define MICROS_PER_MINUTE 60000000LL
#define MICROS_PER_SECOND 1000000LL
#define FRACTION_X 10 /* first column of the fraction digits */

/* ---- pure ------------------------------------------------------- */

bool ctui_timeinput_valid(long long micro)
{
	return micro >= 0 && micro < CTUI_TIME_MICROS_PER_DAY;
}

static bool two_digits(const char *s, int *out)
{
	if (s[0] < '0' || s[0] > '9' || s[1] < '0' || s[1] > '9')
		return false;
	*out = (s[0] - '0') * 10 + (s[1] - '0');
	return true;
}

bool ctui_timeinput_parse(const char *s, long long *out)
{
	if (!s)
		return false;
	size_t len = strlen(s);
	int h, m, sec = 0;
	long long micro;
	if (len < 5 || s[2] != ':' || !two_digits(s, &h) ||
	    !two_digits(s + 3, &m) || h > 23 || m > 59)
		return false;
	if (len > 5) {
		if (len < 8 || s[5] != ':' || !two_digits(s + 6, &sec) ||
		    sec > 59)
			return false;
	}
	micro = h * MICROS_PER_HOUR + m * MICROS_PER_MINUTE +
		sec * MICROS_PER_SECOND;
	if (len > 8) {
		if (s[8] != '.' || len < 10 || len > 15)
			return false;
		long long scale = MICROS_PER_SECOND / 10;
		for (size_t i = 9; i < len; i++, scale /= 10) {
			if (s[i] < '0' || s[i] > '9')
				return false;
			micro += (s[i] - '0') * scale;
		}
	}
	if (out)
		*out = micro;
	return true;
}

int ctui_timeinput_width(ctui_time_precision precision)
{
	static const int text[4] = {5, 8, 12, 15};
	return precision >= CTUI_TIME_MINUTES && precision <= CTUI_TIME_MICROS
		   ? text[precision] + 2
		   : 0;
}

/* Microseconds that make one unit of the fraction segment. */
static int fraction_unit(int precision)
{
	return precision == CTUI_TIME_MILLIS ? 1000 : 1;
}

/* Digits of the fraction segment. */
static int fraction_digits(int precision)
{
	return precision == CTUI_TIME_MILLIS ? 3 : 6;
}

char *ctui_timeinput_format(long long micro, ctui_time_precision precision,
			    char *buf, size_t cap)
{
	if (!buf || cap < CTUI_TIMEINPUT_TEXT || !ctui_timeinput_valid(micro) ||
	    precision < CTUI_TIME_MINUTES || precision > CTUI_TIME_MICROS)
		return NULL;
	int h = (int)(micro / MICROS_PER_HOUR),
	    m = (int)(micro / MICROS_PER_MINUTE % 60),
	    s = (int)(micro / MICROS_PER_SECOND % 60);
	int frac = (int)(micro % MICROS_PER_SECOND) / fraction_unit(precision);
	switch (precision) {
	case CTUI_TIME_MINUTES:
		snprintf(buf, cap, "%02d:%02d", h, m);
		break;
	case CTUI_TIME_SECONDS:
		snprintf(buf, cap, "%02d:%02d:%02d", h, m, s);
		break;
	case CTUI_TIME_MILLIS:
		snprintf(buf, cap, "%02d:%02d:%02d.%03d", h, m, s, frac);
		break;
	default:
		snprintf(buf, cap, "%02d:%02d:%02d.%06d", h, m, s, frac);
		break;
	}
	return buf;
}

/* ---- value ------------------------------------------------------ */

static long long truncate_to(long long micro, int precision)
{
	long long unit = precision == CTUI_TIME_MINUTES ? MICROS_PER_MINUTE
			 : precision == CTUI_TIME_SECONDS
			     ? MICROS_PER_SECOND
			     : fraction_unit(precision);
	return micro / unit * unit;
}

static int last_segment(int precision)
{
	return precision == CTUI_TIME_MINUTES	? CTUI_TIMEINPUT_MINUTE
	       : precision == CTUI_TIME_SECONDS ? CTUI_TIMEINPUT_SECOND
						: CTUI_TIMEINPUT_FRACTION;
}

void ctui_timeinput_init(ctui_timeinput *in, ctui_time_precision precision,
			 long long micro)
{
	if (precision < CTUI_TIME_MINUTES || precision > CTUI_TIME_MICROS)
		precision = CTUI_TIME_SECONDS;
	if (!ctui_timeinput_valid(micro))
		micro = 0;
	*in = (ctui_timeinput){.micro = truncate_to(micro, (int)precision),
			       .precision = (int)precision};
}

static void reset_entry(ctui_timeinput *in)
{
	in->typed = 0;
	in->entry = 0;
}

static long long clamp_limits(const ctui_timeinput *in, long long micro)
{
	if (in->has_min && micro < in->min)
		micro = in->min;
	if (in->has_max && micro > in->max)
		micro = in->max;
	return micro;
}

bool ctui_timeinput_set(ctui_timeinput *in, long long micro)
{
	if (!ctui_timeinput_valid(micro))
		return false;
	micro = truncate_to(micro, in->precision);
	if ((in->has_min && micro < in->min) ||
	    (in->has_max && micro > in->max))
		return false;
	in->micro = micro;
	reset_entry(in);
	return true;
}

bool ctui_timeinput_set_limits(ctui_timeinput *in, const long long *min,
			       const long long *max)
{
	if (min && max && *min > *max)
		return false;
	in->has_min = min != NULL;
	in->has_max = max != NULL;
	in->min = min ? *min : 0;
	in->max = max ? *max : 0;
	in->micro = clamp_limits(in, in->micro);
	reset_entry(in);
	return true;
}

/* Unit of a segment in microseconds, and the number of values it takes. */
static long long segment_unit(const ctui_timeinput *in, int seg)
{
	return seg == CTUI_TIMEINPUT_HOUR     ? MICROS_PER_HOUR
	       : seg == CTUI_TIMEINPUT_MINUTE ? MICROS_PER_MINUTE
	       : seg == CTUI_TIMEINPUT_SECOND ? MICROS_PER_SECOND
					      : fraction_unit(in->precision);
}

static long segment_count(const ctui_timeinput *in, int seg)
{
	if (seg == CTUI_TIMEINPUT_HOUR)
		return 24;
	if (seg == CTUI_TIMEINPUT_FRACTION)
		return in->precision == CTUI_TIME_MILLIS ? 1000 : 1000000;
	return 60;
}

static long segment_value(const ctui_timeinput *in, int seg)
{
	return (long)(in->micro / segment_unit(in, seg) %
		      segment_count(in, seg));
}

/* The value with segment `seg` replaced by v. */
static long long with_segment(const ctui_timeinput *in, int seg, long v)
{
	long long unit = segment_unit(in, seg);
	return in->micro +
	       ((long long)v - (long long)segment_value(in, seg)) * unit;
}

bool ctui_timeinput_step(ctui_timeinput *in, int delta)
{
	reset_entry(in);
	long n = segment_count(in, in->segment);
	long v = ((segment_value(in, in->segment) + delta) % n + n) % n;
	long long next = clamp_limits(in, with_segment(in, in->segment, v));
	bool changed = next != in->micro;
	in->micro = next;
	return changed;
}

/* ---- typing ----------------------------------------------------- */

static ctui_timeinput_event move_segment(ctui_timeinput *in, int to)
{
	reset_entry(in);
	if (to < CTUI_TIMEINPUT_HOUR || to > last_segment(in->precision) ||
	    to == in->segment)
		return CTUI_TIMEINPUT_NONE;
	in->segment = to;
	return CTUI_TIMEINPUT_MOVED;
}

static ctui_timeinput_event type_digit(ctui_timeinput *in, int d)
{
	int seg = in->segment;
	bool fraction = seg == CTUI_TIMEINPUT_FRACTION;
	int max_digits = fraction ? fraction_digits(in->precision) : 2;
	long limit = segment_count(in, seg) - 1;
	long candidate = in->typed ? in->entry * 10 + d : d;
	if (!fraction && in->typed &&
	    candidate > limit) { /* cannot continue: start over here */
		candidate = d;
		in->typed = 0;
	}
	in->entry = candidate;
	in->typed++;
	long v = candidate;
	for (int i = in->typed; fraction && i < max_digits; i++)
		v *= 10; /* the digits fill the fraction from the left */
	if (v > limit)
		v = limit;
	long long before = in->micro;
	in->micro = clamp_limits(in, with_segment(in, seg, v));
	bool done =
	    in->typed >= max_digits || (!fraction && candidate * 10 > limit);
	if (done) {
		reset_entry(in);
		if (seg < last_segment(in->precision))
			in->segment++;
	}
	return before != in->micro ? CTUI_TIMEINPUT_CHANGED
				   : CTUI_TIMEINPUT_MOVED;
}

ctui_timeinput_event ctui_timeinput_key(ctui_timeinput *in, int key)
{
	switch (key) {
	case KEY_LEFT:
		return move_segment(in, in->segment - 1);
	case KEY_RIGHT:
	case ':':
	case '.':
	case ' ':
		return move_segment(in, in->segment + 1);
	case KEY_UP:
		return ctui_timeinput_step(in, 1) ? CTUI_TIMEINPUT_CHANGED
						  : CTUI_TIMEINPUT_MOVED;
	case KEY_DOWN:
		return ctui_timeinput_step(in, -1) ? CTUI_TIMEINPUT_CHANGED
						   : CTUI_TIMEINPUT_MOVED;
	case KEY_PPAGE:
		return ctui_timeinput_step(in, 10) ? CTUI_TIMEINPUT_CHANGED
						   : CTUI_TIMEINPUT_MOVED;
	case KEY_NPAGE:
		return ctui_timeinput_step(in, -10) ? CTUI_TIMEINPUT_CHANGED
						    : CTUI_TIMEINPUT_MOVED;
	case '\n':
	case '\r':
	case KEY_ENTER:
		reset_entry(in);
		return CTUI_TIMEINPUT_SUBMITTED;
	case 27:
		reset_entry(in);
		return CTUI_TIMEINPUT_CANCELLED;
	default:
		if (key >= '0' && key <= '9')
			return type_digit(in, key - '0');
		return CTUI_TIMEINPUT_NONE;
	}
}

/* ---- mouse ------------------------------------------------------ */

int ctui_timeinput_segment_at(ctui_time_precision precision, int x)
{
	int seg = -1;
	if (x >= 1 && x <= 3)
		seg = CTUI_TIMEINPUT_HOUR;
	else if (x >= 4 && x <= 6)
		seg = CTUI_TIMEINPUT_MINUTE;
	else if (x >= 7 && x <= 9)
		seg = CTUI_TIMEINPUT_SECOND;
	else if (x >= FRACTION_X &&
		 x < FRACTION_X + fraction_digits((int)precision))
		seg = CTUI_TIMEINPUT_FRACTION;
	if (precision < CTUI_TIME_MINUTES || precision > CTUI_TIME_MICROS ||
	    seg > last_segment((int)precision))
		return -1;
	return seg;
}

ctui_timeinput_event ctui_timeinput_click(ctui_timeinput *in, int y, int x)
{
	if (y != 0 || x < 0 ||
	    x >= ctui_timeinput_width((ctui_time_precision)in->precision))
		return CTUI_TIMEINPUT_NONE;
	int seg =
	    ctui_timeinput_segment_at((ctui_time_precision)in->precision, x);
	return seg < 0 ? CTUI_TIMEINPUT_NONE : move_segment(in, seg);
}

ctui_timeinput_event ctui_timeinput_wheel(ctui_timeinput *in, int notches)
{
	if (notches == 0)
		return CTUI_TIMEINPUT_NONE;
	return ctui_timeinput_step(in, notches) ? CTUI_TIMEINPUT_CHANGED
						: CTUI_TIMEINPUT_NONE;
}

/* ---- drawing ---------------------------------------------------- */

void ctui_timeinput_draw(const ctui_timeinput *in, int y, int x, bool focused)
{
	ctui_style_begin(in->style);
	int attr = ctui_attr(focused ? CTUI_PAIR_SELECTION : CTUI_PAIR_DIALOG,
			     focused);
	int width = ctui_timeinput_width((ctui_time_precision)in->precision);
	ctui_fill(y, x, width, attr);
	char text[CTUI_TIMEINPUT_TEXT];
	// NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
	if (ctui_timeinput_format(in->micro, (ctui_time_precision)in->precision,
				  text, sizeof text))
		ctui_put(y, x + 1, width - 2, text, attr);
	if (focused) {
		/* the columns of the segment under the cursor */
		static const int start[4] = {1, 4, 7, FRACTION_X};
		int s = in->segment < 0 || in->segment > 3 ? 0 : in->segment;
		int len = s == CTUI_TIMEINPUT_FRACTION
			      ? fraction_digits(in->precision)
			      : 2;
		char part[8];
		snprintf(part, sizeof part, "%.*s", len, text + start[s] - 1);
		ctui_put(y, x + start[s], len, part,
			 (int)((unsigned)attr | (unsigned)A_REVERSE));
	}
	ctui_style_end();
}
