/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * knob.h - a rotary knob, as on a synth or a drum machine.
 *
 * The knob holds a value in [min, max] that moves on a grid of `step`. It
 * is drawn compactly in CTUI_KNOB_HEIGHT rows: a 5 x 3 ring of
 * CTUI_KNOB_POSITIONS cells open at the bottom (the dial runs clockwise from
 * the bottom left, through the top, to the bottom right), then the label
 * and the value text below it, both centred on the knob. The cells before
 * the pointer are lit, the pointer has its own glyph and the cells after it
 * are unlit. The glyphs come from a Unicode set or, when the theme is not
 * UTF-8, from an ASCII one (ctui_knob_glyph_set()).
 *
 * A bipolar knob has its detent at the middle of the range: the grid starts
 * there, so the centre is always reachable, and the lit cells run from the
 * centre to the pointer. The value text then carries a sign.
 *
 * The mapping between value and dial position is made of pure functions
 * (ctui_knob_ratio(), ctui_knob_value_at(), ctui_knob_position(),
 * ctui_knob_position_value(), ctui_knob_cell()), usable without a screen.
 *
 * Keys: Left / Down and Right / Up move by one step, PgDn / PgUp by ten
 * steps, Home / End jump to the minimum or the maximum; the shifted arrows
 * (KEY_SLEFT, KEY_SRIGHT, KEY_SF, KEY_SR) fine-adjust by a tenth of a step
 * and may leave the step grid; r and Delete restore the default value.
 *
 * Mouse: ctui_knob_press() grabs the knob, ctui_knob_drag() turns it by the
 * rows the pointer has travelled (up raises the value; the full range takes
 * CTUI_KNOB_DRAG_ROWS rows) and ctui_knob_release() lets go. ctui_knob_wheel()
 * moves by steps. Positions given to ctui_knob_hit() and ctui_knob_press() are
 * relative to the origin given to ctui_knob_draw().
 */
#ifndef CTUI_KNOB_H
#define CTUI_KNOB_H

#include <stdbool.h>
#include <stddef.h>

#include <ctui/theme.h>

#define CTUI_KNOB_WIDTH 5      /* columns of the dial */
#define CTUI_KNOB_HEIGHT 5     /* rows: 3 of dial, the label, the value */
#define CTUI_KNOB_POSITIONS 9  /* cells of the dial ring */
#define CTUI_KNOB_DRAG_ROWS 20 /* rows of mouse travel for the whole range */

typedef struct ctui_knob {
	const char *label; /* drawn under the dial, may be NULL */
	const char
	    *unit; /* appended to the value text ("Hz", "%"), may be NULL */
	double min, max; /* max > min */
	double step;	 /* grid of the keys, the wheel and the drag, > 0 */
	double value;	 /* min .. max */
	double def;	 /* value restored by ctui_knob_reset() */
	int decimals;  /* decimals of the value text, derived from step by init
			*/
	bool bipolar;  /* detent and grid origin at the middle of the range */
	bool dragging; /* a mouse drag is in progress */
	int drag_y;    /* row of the press */
	double drag_from; /* value at the press */
	const ctui_style
	    *style; /* colours of this instance, may be NULL; set after init */
} ctui_knob;

/* The glyphs of the dial. */
typedef struct ctui_knob_glyphs {
	const char *lit, *unlit, *pointer, *detent;
} ctui_knob_glyphs;

/* The Unicode or the ASCII glyphs. */
const ctui_knob_glyphs *ctui_knob_glyph_set(bool unicode);

/* A knob over [min, max] on a grid of step, set to value (which becomes
 * the default). A max not above min becomes min + 1 and a step that is not
 * positive becomes a hundredth of the range. */
void ctui_knob_init(ctui_knob *k, const char *label, double min, double max,
		    double step, double value);

/* Value of a ratio 0..1 of the range, on the grid and in range. */
double ctui_knob_value_at(const ctui_knob *k, double ratio);

/* Ratio 0..1 of the current value. */
double ctui_knob_ratio(const ctui_knob *k);

/* Middle of the range. */
double ctui_knob_centre(const ctui_knob *k);

/* Dial position 0 .. CTUI_KNOB_POSITIONS - 1 of the current value, and the
 * value a position stands for. */
int ctui_knob_position(const ctui_knob *k);
double ctui_knob_position_value(const ctui_knob *k, int position);

/* Row and column (0..2, 0..4) of a dial position; false outside the ring. */
bool ctui_knob_cell(int position, int *row, int *col);

/* Set the value, clamped to the range; true when it changed. */
bool ctui_knob_set(ctui_knob *k, double value);

/* Restore the default value; true when it changed. */
bool ctui_knob_reset(ctui_knob *k);

/* The value text ("12.5Hz", "+3" for a bipolar knob); returns buf. */
char *ctui_knob_format(const ctui_knob *k, char *buf, size_t cap);

/* Apply a key; true when it is one the knob handles. */
bool ctui_knob_key(ctui_knob *k, int key);

/* Move by notches steps (positive raises the value); true when it changed. */
bool ctui_knob_wheel(ctui_knob *k, int notches);

/* Whether (y, x) is on the dial, in a draw area of the given width. */
bool ctui_knob_hit(const ctui_knob *k, int width, int y, int x);

/* Left button pressed: grabs the knob when it hits. True when it hit. */
bool ctui_knob_press(ctui_knob *k, int width, int y, int x);

/* Pointer moved with the button down; true when the value changed. */
bool ctui_knob_drag(ctui_knob *k, int y);

/* Button released; true when a drag ended. */
bool ctui_knob_release(ctui_knob *k);

/* Draw the knob in an area of width columns and CTUI_KNOB_HEIGHT rows at
 * (y, x), centred; focused draws the label emphasised. */
void ctui_knob_draw(const ctui_knob *k, int y, int x, int width, bool focused);

#endif /* CTUI_KNOB_H */
