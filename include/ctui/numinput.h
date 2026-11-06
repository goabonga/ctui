/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * numinput.h - a number field, integer or decimal, with a range, a step
 * and an optional unit suffix.
 *
 * The value is a double that always sits inside [min, max] and is rounded
 * to `decimals` digits after the point (0 = integer). Numbers are read and
 * written by the library itself, never through the C locale: the decimal
 * point is always '.', there are no thousands separators and no exponent.
 * At most 15 significant digits are accepted, which keeps every value
 * exactly representable; min and max are limited to that range.
 *
 * Keys: Up / Down add or subtract one step, PgUp / PgDn ten steps, Home /
 * End go to the minimum / maximum. Digits, '.', '-' and '+' are typed
 * (the first one replaces the shown number); a character that cannot be
 * part of a number in range is ignored: '-' only when min is negative, '.'
 * only with decimals, no more fraction digits than `decimals`, nothing
 * that already exceeds max (or min, for a negative number). The value
 * follows the typed text while it is a complete number, kept inside the
 * range (so a number typed below min shows as min). Backspace deletes the last
 * typed character. Enter ends the edit and is reported as submitted; Esc during
 * an edit puts the number back as it was, otherwise it is reported as
 * cancelled. Up / Down / PgUp / PgDn / Home / End end the edit too, and the
 * caller calls ctui_numinput_commit() when the focus leaves the field.
 *
 * Mouse: coordinates are relative to the top-left cell of the field as
 * drawn: a click on the "-" at its left end or the "+" at its right end
 * steps the value, and the wheel steps it (up adds).
 *
 * ctui_numinput_parse() and ctui_numinput_format() are pure.
 */
#ifndef CTUI_NUMINPUT_H
#define CTUI_NUMINPUT_H

#include <stdbool.h>
#include <stddef.h>

#include <ctui/theme.h>

#define CTUI_NUMINPUT_MAX_DECIMALS 8
#define CTUI_NUMINPUT_MAX_DIGITS 15 /* significant digits of a number */
#define CTUI_NUMINPUT_EDIT 24	    /* bytes of the typed text, with its NUL */

typedef struct ctui_numinput {
	double value, min, max, step;
	int decimals; /* digits after the point, 0..CTUI_NUMINPUT_MAX_DECIMALS
		       */
	const char *unit; /* shown after the number ("kg", "%"), may be NULL */
	bool editing;	  /* `edit` holds the text being typed */
	char edit[CTUI_NUMINPUT_EDIT];
	double before; /* value when the edit began, for Esc */
	const ctui_style
	    *style; /* colours of this instance, may be NULL; set after init */
} ctui_numinput;

typedef enum ctui_numinput_event {
	CTUI_NUMINPUT_NONE = 0,
	CTUI_NUMINPUT_CHANGED,	 /* the value changed */
	CTUI_NUMINPUT_SUBMITTED, /* Enter */
	CTUI_NUMINPUT_CANCELLED	 /* Esc outside an edit */
} ctui_numinput_event;

/* ---- pure functions ---- */

/* Read "[+-]digits[.digits]" with at most `decimals` fraction digits and at
 * most CTUI_NUMINPUT_MAX_DIGITS significant digits into *out. Nothing else is
 * accepted: no spaces, no exponent, no ".5" or "5.", no "," as a decimal
 * point. False, leaving *out alone, for anything else (overflow included).
 * "-0" reads as 0. */
bool ctui_numinput_parse(const char *s, int decimals, double *out);

/* Write value with exactly `decimals` digits after the point into buf and
 * return it ("-12.50"); NULL when the value is not finite, needs more than
 * CTUI_NUMINPUT_MAX_DIGITS digits or does not fit in cap. */
char *ctui_numinput_format(double value, int decimals, char *buf, size_t cap);

/* ---- component ---- */

/* A field over [min, max] with the given step and decimals (clamped to
 * 0..CTUI_NUMINPUT_MAX_DECIMALS), set to value. A max below min becomes min,
 * a step that is not positive becomes one unit of the last decimal, and
 * min and max are limited to what 15 digits can hold. */
void ctui_numinput_init(ctui_numinput *in, double min, double max, double step,
			double value, int decimals);

/* Set the value, clamped and rounded; false for NaN or infinity. Ends an
 * edit. True when the value changed. */
bool ctui_numinput_set(ctui_numinput *in, double value);

/* Add steps * step (negative subtracts) to the value, clamped, without
 * drifting on decimal steps; true when the value changed. */
bool ctui_numinput_add(ctui_numinput *in, long steps);

/* End an edit; the value already follows the typed text, and stays as it
 * is when the text is not a number. True when it differs from the value at
 * the start of the edit. */
bool ctui_numinput_commit(ctui_numinput *in);

/* The number as shown, without the unit: the typed text while editing. */
char *ctui_numinput_text(const ctui_numinput *in, char *buf, size_t cap);

ctui_numinput_event ctui_numinput_key(ctui_numinput *in, int key);

/* Left press at (y, x) relative to the field of width columns. */
ctui_numinput_event ctui_numinput_click(ctui_numinput *in, int width, int y,
					int x);

/* Wheel: positive notches add steps. */
ctui_numinput_event ctui_numinput_wheel(ctui_numinput *in, int notches);

/* Draw in width columns at (y, x): "-", the number and unit right-aligned,
 * "+". While editing and focused a cursor follows the typed text. */
void ctui_numinput_draw(const ctui_numinput *in, int y, int x, int width,
			bool focused);

#endif /* CTUI_NUMINPUT_H */
