/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * numinput.c - number field with range, step and unit (numinput.h).
 */
#include <ctui/numinput.h>

#include <curses.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include <ctui/text.h>
#include <ctui/theme.h>

#define MAX_MANTISSA 999999999999999LL /* 15 digits */
#define PAGE_STEPS 10

static const double POW10[CTUI_NUMINPUT_MAX_DECIMALS + 1] = {
    1, 1e1, 1e2, 1e3, 1e4, 1e5, 1e6, 1e7, 1e8};

/* ---- pure ------------------------------------------------------- */

bool ctui_numinput_parse(const char *s, int decimals, double *out)
{
	if (!s || decimals < 0 || decimals > CTUI_NUMINPUT_MAX_DECIMALS)
		return false;
	bool negative = false;
	if (*s == '+' || *s == '-') {
		negative = *s == '-';
		s++;
	}
	long long mantissa = 0;
	int significant = 0, int_digits = 0, frac_digits = 0;
	for (; *s >= '0' && *s <= '9'; s++, int_digits++) {
		if (mantissa || *s != '0') {
			if (++significant > CTUI_NUMINPUT_MAX_DIGITS)
				return false;
			mantissa = mantissa * 10 + (*s - '0');
		}
	}
	if (int_digits == 0)
		return false;
	if (*s == '.') {
		s++;
		for (; *s >= '0' && *s <= '9'; s++, frac_digits++) {
			if (frac_digits >= decimals)
				return false;
			if (mantissa || *s != '0') {
				if (++significant > CTUI_NUMINPUT_MAX_DIGITS)
					return false;
				mantissa = mantissa * 10 + (*s - '0');
			}
		}
		if (frac_digits == 0)
			return false;
	}
	if (*s)
		return false;
	double v = (double)mantissa / POW10[frac_digits];
	if (out)
		*out = negative && mantissa ? -v : v;
	return true;
}

char *ctui_numinput_format(double value, int decimals, char *buf, size_t cap)
{
	if (!buf || !isfinite(value) || decimals < 0 ||
	    decimals > CTUI_NUMINPUT_MAX_DECIMALS)
		return NULL;
	double scaled = round(fabs(value) * POW10[decimals]);
	if (scaled > (double)MAX_MANTISSA)
		return NULL;
	long long m = (long long)scaled;
	char digits[24];
	size_t n = 0;
	do {
		digits[n++] = (char)('0' + m % 10);
		m /= 10;
	} while (m);
	while ((int)n <= decimals) /* at least one digit before the point */
		digits[n++] = '0';
	size_t need = n + (decimals > 0) + (value < 0 && scaled > 0) + 1;
	if (need > cap)
		return NULL;
	size_t o = 0;
	if (value < 0 && scaled > 0)
		buf[o++] = '-';
	for (size_t i = n; i > 0; i--) {
		buf[o++] = digits[i - 1];
		if (decimals > 0 && (int)(i - 1) == decimals)
			buf[o++] = '.';
	}
	buf[o] = '\0';
	return buf;
}

/* ---- value ------------------------------------------------------ */

static double limit(int decimals)
{
	return (double)MAX_MANTISSA / POW10[decimals];
}

static double snap(const ctui_numinput *in, double v)
{
	double lim = limit(in->decimals);
	v = round(v * POW10[in->decimals]) / POW10[in->decimals];
	if (v < in->min)
		v = in->min;
	if (v > in->max)
		v = in->max;
	if (v > lim)
		v = lim;
	if (v < -lim)
		v = -lim;
	return v == 0 ? 0 : v; /* no negative zero */
}

void ctui_numinput_init(ctui_numinput *in, double min, double max, double step,
			double value, int decimals)
{
	if (decimals < 0)
		decimals = 0;
	if (decimals > CTUI_NUMINPUT_MAX_DECIMALS)
		decimals = CTUI_NUMINPUT_MAX_DECIMALS;
	double lim = limit(decimals);
	if (!isfinite(min) || min < -lim)
		min = -lim;
	if (min > lim)
		min = lim;
	if (!isfinite(max) || max > lim)
		max = lim;
	if (max < min)
		max = min;
	if (!isfinite(step) || step <= 0)
		step = 1 / POW10[decimals];
	*in = (ctui_numinput){
	    .min = min, .max = max, .step = step, .decimals = decimals};
	in->value = snap(in, isfinite(value) ? value : min);
}

bool ctui_numinput_set(ctui_numinput *in, double value)
{
	in->editing = false;
	in->edit[0] = '\0';
	if (!isfinite(value))
		return false;
	double before = in->value;
	in->value = snap(in, value);
	return in->value != before;
}

bool ctui_numinput_add(ctui_numinput *in, long steps)
{
	in->editing = false;
	in->edit[0] = '\0';
	/* Work in units of the last decimal so that 0.1 steps do not drift. */
	double scale = POW10[in->decimals];
	double unit = round(in->step * scale);
	if (unit < 1)
		unit = 1;
	double delta = (double)steps * unit;
	double lim =
	    limit(in->decimals) * scale; /* the whole range, in units */
	if (delta > 2 * lim)
		delta = 2 * lim;
	if (delta < -2 * lim)
		delta = -2 * lim;
	double before = in->value;
	in->value = snap(in, (round(in->value * scale) + delta) / scale);
	return in->value != before;
}

/* ---- editing ---------------------------------------------------- */

/* Value of the typed text if it is a complete number. */
static bool edit_value(const ctui_numinput *in, double *out)
{
	char tmp[CTUI_NUMINPUT_EDIT + 2];
	size_t n = strlen(in->edit);
	if (n == 0)
		return false;
	memcpy(tmp, in->edit, n + 1);
	if (tmp[n - 1] == '.') /* "12." is a number being typed */
		tmp[n - 1] = '\0';
	char *digits = tmp + (tmp[0] == '+' || tmp[0] == '-');
	if (*digits == '.') { /* ".5" */
		memmove(digits + 1, digits, strlen(digits) + 1);
		*digits = '0';
	}
	return ctui_numinput_parse(tmp, in->decimals, out);
}

static void begin_edit(ctui_numinput *in)
{
	in->editing = true;
	in->edit[0] = '\0';
	in->before = in->value;
}

static bool apply_edit(ctui_numinput *in)
{
	double v;
	if (!edit_value(in, &v))
		return false;
	double was = in->value;
	in->value = snap(in, v);
	return in->value != was;
}

bool ctui_numinput_commit(ctui_numinput *in)
{
	if (!in->editing)
		return false;
	in->editing = false;
	in->edit[0] = '\0';
	return in->value != in->before;
}

static ctui_numinput_event type_char(ctui_numinput *in, int key)
{
	bool fresh = !in->editing;
	if (fresh)
		begin_edit(in);
	char tmp[CTUI_NUMINPUT_EDIT];
	size_t n = strlen(in->edit);
	bool ok = n + 2 <= sizeof tmp;
	if (ok) {
		memcpy(tmp, in->edit, n);
		tmp[n] = (char)key;
		tmp[n + 1] = '\0';
		const char *p = tmp;
		if (key == '-' || key == '+')
			ok = n == 0 && (key == '+' || in->min < 0);
		else if (key == '.')
			ok = in->decimals > 0 && !strchr(in->edit, '.');
		if (ok) {
			if (*p == '+' || *p == '-')
				p++;
			const char *dot = strchr(p, '.');
			size_t frac = dot ? strlen(dot + 1) : 0;
			ok = frac <= (size_t)in->decimals;
			size_t digits = 0;
			for (const char *q = p; *q; q++)
				digits += *q != '.' && (digits || *q != '0');
			ok = ok && digits <= CTUI_NUMINPUT_MAX_DIGITS;
		}
	}
	if (ok) {
		double v;
		char saved[CTUI_NUMINPUT_EDIT];
		memcpy(saved, in->edit, sizeof saved);
		memcpy(in->edit, tmp, n + 2);
		if (edit_value(in, &v) &&
		    ((v > in->max && v >= 0) || (v < in->min && v < 0)))
			ok = false;
		if (!ok)
			memcpy(in->edit, saved, sizeof saved);
	}
	if (!ok) {
		if (fresh)
			in->editing = false;
		return CTUI_NUMINPUT_NONE;
	}
	return apply_edit(in) ? CTUI_NUMINPUT_CHANGED : CTUI_NUMINPUT_NONE;
}

static ctui_numinput_event backspace(ctui_numinput *in)
{
	if (!in->editing) {
		char shown[CTUI_NUMINPUT_EDIT];
		if (!ctui_numinput_format(in->value, in->decimals, shown,
					  sizeof shown))
			return CTUI_NUMINPUT_NONE;
		begin_edit(in);
		memcpy(in->edit, shown, sizeof in->edit);
	}
	size_t n = strlen(in->edit);
	if (n)
		in->edit[n - 1] = '\0';
	return apply_edit(in) ? CTUI_NUMINPUT_CHANGED : CTUI_NUMINPUT_NONE;
}

/* End the edit (if any), then step; changed compared with the value
 * before the edit began. */
static ctui_numinput_event step_key(ctui_numinput *in, long steps)
{
	double start = in->editing ? in->before : in->value;
	ctui_numinput_commit(in);
	ctui_numinput_add(in, steps);
	return in->value != start ? CTUI_NUMINPUT_CHANGED : CTUI_NUMINPUT_NONE;
}

ctui_numinput_event ctui_numinput_key(ctui_numinput *in, int key)
{
	switch (key) {
	case KEY_UP:
		return step_key(in, 1);
	case KEY_DOWN:
		return step_key(in, -1);
	case KEY_PPAGE:
		return step_key(in, PAGE_STEPS);
	case KEY_NPAGE:
		return step_key(in, -PAGE_STEPS);
	case KEY_HOME:
	case KEY_END: {
		double start = in->editing ? in->before : in->value;
		ctui_numinput_commit(in);
		ctui_numinput_set(in, key == KEY_HOME ? in->min : in->max);
		return in->value != start ? CTUI_NUMINPUT_CHANGED
					  : CTUI_NUMINPUT_NONE;
	}
	case KEY_BACKSPACE:
	case 127:
	case 8:
		return backspace(in);
	case '\n':
	case '\r':
	case KEY_ENTER:
		ctui_numinput_commit(in);
		return CTUI_NUMINPUT_SUBMITTED;
	case 27:
		if (in->editing) {
			double now = in->value;
			in->value = in->before;
			in->editing = false;
			in->edit[0] = '\0';
			return in->value != now ? CTUI_NUMINPUT_CHANGED
						: CTUI_NUMINPUT_NONE;
		}
		return CTUI_NUMINPUT_CANCELLED;
	default:
		if ((key >= '0' && key <= '9') || key == '.' || key == '-' ||
		    key == '+')
			return type_char(in, key);
		return CTUI_NUMINPUT_NONE;
	}
}

ctui_numinput_event ctui_numinput_click(ctui_numinput *in, int width, int y,
					int x)
{
	if (y != 0 || width < 3 || x < 0 || x >= width)
		return CTUI_NUMINPUT_NONE;
	if (x == 0)
		return step_key(in, -1);
	if (x == width - 1)
		return step_key(in, 1);
	return CTUI_NUMINPUT_NONE;
}

ctui_numinput_event ctui_numinput_wheel(ctui_numinput *in, int notches)
{
	if (notches == 0)
		return CTUI_NUMINPUT_NONE;
	return step_key(in, notches);
}

/* ---- display ---------------------------------------------------- */

char *ctui_numinput_text(const ctui_numinput *in, char *buf, size_t cap)
{
	if (in->editing) {
		if (!buf || strlen(in->edit) + 1 > cap)
			return NULL;
		memcpy(buf, in->edit, strlen(in->edit) + 1);
		return buf;
	}
	return ctui_numinput_format(in->value, in->decimals, buf, cap);
}

void ctui_numinput_draw(const ctui_numinput *in, int y, int x, int width,
			bool focused)
{
	if (width < 1)
		return;
	ctui_style_begin(in->style);
	int attr = ctui_attr(focused ? CTUI_PAIR_SELECTION : CTUI_PAIR_DIALOG,
			     focused);
	int button = ctui_attr(CTUI_PAIR_MUTED, false);
	ctui_fill(y, x, width, attr);
	char num[CTUI_NUMINPUT_EDIT + 4];
	if (!ctui_numinput_text(in, num, sizeof num))
		num[0] = '\0';
	bool has_unit = in->unit && *in->unit;
	bool cursor = focused && in->editing;
	char line[CTUI_TEXT_MAX];
	snprintf(line, sizeof line, "%s%s%s", num,
		 has_unit || cursor ? " " : "", has_unit ? in->unit : "");
	if (width >= 3) {
		ctui_put(y, x, 1, "-", focused ? attr : button);
		ctui_put(y, x + width - 1, 1, "+", focused ? attr : button);
	}
	int room =
	    width >= 3 ? width - 4 : width; /* a margin beside each button */
	int cols = (int)ctui_utf8_columns(line);
	int left = x + (width >= 3 ? 2 : 0);
	const char *shown = line;
	if (cols <= room)
		left += room - cols;
	while (*shown && (int)ctui_utf8_columns(shown) >
			     room) { /* keep the end of the number */
		unsigned long cp;
		shown += ctui_utf8_decode(shown, &cp);
	}
	ctui_put(y, left, room, shown, attr);
	if (cursor) {
		/* The cursor takes the blank after the number. */
		int cx = left + (int)ctui_utf8_columns(shown) -
			 (has_unit ? (int)ctui_utf8_columns(in->unit) : 0) - 1;
		if (cx >= left && cx < left + room)
			ctui_put(y, cx, 1, " ",
				 (int)((unsigned)attr | (unsigned)A_REVERSE));
	}
	ctui_style_end();
}
