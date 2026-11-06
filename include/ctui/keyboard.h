/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * keyboard.h - a piano keyboard: white and black keys over a range of
 * octaves, with held notes, a cursor, a selection and the mapping of the
 * computer keyboard to MIDI notes.
 *
 * Notes are MIDI numbers (60 is C4, 0 is C-1, 127 is G9). The keyboard
 * shows `octaves` octaves from the note `first` (a C in most uses, but any
 * note is accepted); the last key is first + 12 * octaves - 1, or 127.
 *
 * Layout: every white key is `key_width` columns wide, its last column
 * being the divider with the next key. A black key sits on the border of
 * two white keys, is about two thirds of a white key wide and covers the
 * upper half of the rows (at least one). The bottom row names the C keys
 * and the cursor key. Drawing needs at least 3 rows; a window narrower
 * than ctui_keyboard_width() shows the left part of the keys.
 *
 * Held notes are the pressed keys, drawn highlighted. A terminal reports
 * no key release, so the caller releases the notes it triggered when it
 * sees fit (a timer, the end of a mouse press); `last` names the note a key
 * or a mouse press triggered last, for the caller to play it.
 *
 * Keys: Left/Right move the cursor by a semitone, Up/Down by an octave,
 * Home/End jump to the ends, Shift+Left/Right extend a selection of
 * notes, Enter and Space hold or release the cursor note, z and x shift
 * the octave of the typing keys down and up. The typing keys follow a DAW:
 *
 *      w e   t y u   o p        black keys
 *     a s d f g h j k l         white keys   (a is the note `base`)
 *
 * Mouse: a press on a key holds it and puts the cursor there; dragging
 * glides over the other keys, holding the one under the pointer only, and
 * the release lets go. Coordinates are relative to the keyboard.
 */
#ifndef CTUI_KEYBOARD_H
#define CTUI_KEYBOARD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <ctui/theme.h>

#define CTUI_KEYBOARD_MIN_HEIGHT 3
#define CTUI_KEYBOARD_NOTES 128

typedef struct ctui_keyboard {
	int first;     /* lowest note */
	int octaves;   /* at least 1 */
	int key_width; /* columns of a white key, 2..8 (default 4) */
	int cursor;    /* the note under the cursor, within the range */
	bool selection;
	int sel_first,
	    sel_last; /* selected notes, both included, when selection is set */
	int anchor;   /* where a selection started */
	int base;     /* note of the typing key a */
	int last; /* note triggered last by a key or the mouse, -1 for none */
	bool dragging;	    /* a mouse press is in progress */
	int mouse_note;	    /* the note held by the mouse, -1 for none */
	bool mouse_latched; /* mouse_note was already held when the mouse took
			       it */
	uint32_t held[(CTUI_KEYBOARD_NOTES + 31) / 32];
	const ctui_style *style; /* colours and glyphs of this instance, may be
				    NULL; set after init */
} ctui_keyboard;

/* A keyboard of `octaves` octaves from the note `first` (clamped to
 * 0..127; octaves to 1..10), with the cursor on the first key and the typing
 * keys starting there. */
void ctui_keyboard_init(ctui_keyboard *kb, int first, int octaves);

/* Pure helpers on note numbers. */
bool ctui_keyboard_is_black(int note);

/* "C4", "C#4", "A-1"... written into buf (empty for a note outside 0..127).
 * Returns buf. */
char *ctui_keyboard_name(char *buf, size_t cap, int note);

/* The note typed with a computer key (a w s e d f t g y h u j k o l p are
 * the offsets 0 to 15 from base), or -1 when the key is not mapped or the
 * note falls outside 0..127. */
int ctui_keyboard_key_note(int key, int base);

/* Range and size of the keyboard. */
int ctui_keyboard_last(const ctui_keyboard *kb);
bool ctui_keyboard_contains(const ctui_keyboard *kb, int note);
int ctui_keyboard_whites(const ctui_keyboard *kb);
int ctui_keyboard_width(const ctui_keyboard *kb); /* columns for every key */

/* Left column and width of the key of a note, or -1 when the note is not
 * on the keyboard. */
int ctui_keyboard_key_x(const ctui_keyboard *kb, int note, int *width);

/* The note under (y, x) of a keyboard `height` rows high, or -1. */
int ctui_keyboard_note_at(const ctui_keyboard *kb, int y, int x, int height);

/* Held notes. */
void ctui_keyboard_set_held(ctui_keyboard *kb, int note, bool held);
bool ctui_keyboard_is_held(const ctui_keyboard *kb, int note);
int ctui_keyboard_held_count(const ctui_keyboard *kb);
void ctui_keyboard_release_all(ctui_keyboard *kb);

/* Move the cursor by `delta` notes, kept in the range; true when it moved.
 * With extend, the selection stretches from where it started to the cursor. */
bool ctui_keyboard_move(ctui_keyboard *kb, int delta, bool extend);

/* Drop the selection. */
void ctui_keyboard_clear_selection(ctui_keyboard *kb);

/* Apply a key. Returns true when handled; `last` is set when the key held
 * a note. */
bool ctui_keyboard_key(ctui_keyboard *kb, int key);

/* Mouse: press, drag and release at (y, x) of a keyboard `height` rows
 * high. ctui_keyboard_press() returns true when a key was pressed, the others
 * when a press had started. */
bool ctui_keyboard_press(ctui_keyboard *kb, int y, int x, int height);
bool ctui_keyboard_drag(ctui_keyboard *kb, int y, int x, int height);
bool ctui_keyboard_release(ctui_keyboard *kb);

/* Draw the keyboard in a width x height area. */
void ctui_keyboard_draw(const ctui_keyboard *kb, int y, int x, int width,
			int height);

#endif /* CTUI_KEYBOARD_H */
