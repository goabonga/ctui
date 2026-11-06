/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * theme.h - the powerline Solarized look shared by every component.
 *
 * Colours are xterm-256 indexes. With fewer than 256 colours they map to
 * the 8 basic colours; without colours only attributes (reverse, dim) are
 * used. When the locale is not UTF-8, or CTUI_ASCII is set, every glyph
 * falls back to ASCII. NO_COLOR disables colours.
 *
 * Powerline segments are described by a tone: the tone gives the segment
 * its background, and the divider between two tones is drawn with the
 * previous background as foreground and the next one as background.
 *
 * Call ctui_theme_init() once, after initscr(). Pairs are plain ints usable
 * with COLOR_PAIR() through ctui_attr(); no curses type appears here.
 */
#ifndef CTUI_THEME_H
#define CTUI_THEME_H

#include <stdbool.h>

#define CTUI_C_BAR 235
#define CTUI_C_DARK 234
#define CTUI_C_SOFT 240
#define CTUI_C_INACTIVE 245
#define CTUI_C_TEXT 230
#define CTUI_C_GREEN 64
#define CTUI_C_RED 160
#define CTUI_C_YELLOW 136
#define CTUI_C_INFO 60

typedef enum ctui_pair {
	CTUI_PAIR_BODY = 1, /* default on default */
	CTUI_PAIR_BAR,	    /* inactive text on the bar */
	CTUI_PAIR_SOFT,	    /* soft dividers and disabled text on the bar */
	CTUI_PAIR_NOTICE_WARNING, /* yellow on the bar */
	CTUI_PAIR_NOTICE_ERROR,	  /* red on the bar */
	CTUI_PAIR_GAUGE_GREEN,
	CTUI_PAIR_GAUGE_GREEN_BRIGHT,
	CTUI_PAIR_GAUGE_YELLOW,
	CTUI_PAIR_GAUGE_YELLOW_BRIGHT,
	CTUI_PAIR_GAUGE_RED,
	CTUI_PAIR_GAUGE_RED_BRIGHT,
	CTUI_PAIR_GAUGE_EMPTY,	   /* soft grey on default */
	CTUI_PAIR_HEADING,	   /* default on default, drawn bold */
	CTUI_PAIR_MUTED,	   /* inactive grey on default */
	CTUI_PAIR_DIALOG,	   /* text on the bar: dialogs, cards, panels */
	CTUI_PAIR_DIALOG_BORDER,   /* soft grey on the bar */
	CTUI_PAIR_SELECTION,	   /* text on the info tone: selected rows */
	CTUI_PAIR_BORDER,	   /* soft grey on default */
	CTUI_PAIR_BORDER_SELECTED, /* blue on default */
	CTUI_PAIR_ERROR,	   /* red on default */
	CTUI_PAIR_CURSOR,	   /* cyan on default */
	CTUI_PAIR_CURSOR_SELECTED, /* cyan on the info tone */
	CTUI_PAIR_PLAYHEAD,	   /* black on yellow */
	CTUI_PAIR_COUNT
} ctui_pair;

typedef enum ctui_tone {
	CTUI_TONE_BAR = 0, /* the bar background */
	CTUI_TONE_DARK,	   /* the darker right-hand segments */
	CTUI_TONE_IDLE,	   /* neutral grey: current tab, idle state */
	CTUI_TONE_OK,	   /* green */
	CTUI_TONE_BAD,	   /* red */
	CTUI_TONE_BUSY,	   /* yellow */
	CTUI_TONE_INFO,	   /* blue-grey */
	CTUI_TONE_COUNT
} ctui_tone;

typedef enum ctui_icon_color {
	CTUI_ICON_BLUE,
	CTUI_ICON_CYAN,
	CTUI_ICON_GREEN,
	CTUI_ICON_YELLOW,
	CTUI_ICON_ORANGE,
	CTUI_ICON_PURPLE,
	CTUI_ICON_GRAY,
	CTUI_ICON_COLOR_COUNT
} ctui_icon_color;

/* Every colour the theme uses, as xterm-256 indexes (-1 = terminal default).
 * The defaults are the CTUI_C_* values and the icon colours of the file tree.
 */
typedef struct ctui_palette {
	int bar, dark, soft, inactive, text, green, red, yellow, info;
	int green_bright, yellow_bright, red_bright;
	int accent; /* selected borders and the blue icon */
	int cyan;   /* cursor */
	int black;  /* text on the playhead */
	int icon[CTUI_ICON_COLOR_COUNT];
} ctui_palette;

typedef struct ctui_glyphs {
	const char *hard_right, *soft_right, *hard_left, *soft_left;
	const char *gauge_filled, *gauge_empty, *rule;
	const char *box_h, *box_v, *box_tl, *box_tr, *box_bl, *box_br;
	/* Single-column glyphs of the components. */
	const char *scroll_track, *scroll_thumb; /* scrollbar */
	const char *close;	   /* close button, framed by spaces */
	const char *check, *cross; /* done step, ok toast / error toast */
	const char *info, *warn;   /* info and warning toasts */
	const char *tree_open, *tree_shut; /* expanded and collapsed folder */
	const char *select_open,
	    *select_shut;	  /* select field, list open or closed */
	const char *knob, *heavy; /* slider knob, slider fill and tab rule */
	const char *burger;	  /* burger button */
	const char *ellipsis;	  /* cut text (may be several columns) */
	bool unicode;
	const char *submenu; /* marker of a menu entry that opens a submenu */
} ctui_glyphs;

/* Colours and glyphs for one component instance, on top of the global
 * theme: attr[pair] replaces the attribute of that pair (0 = the theme's),
 * and a glyph that is not NULL replaces the theme's glyph. Components with
 * a `style` pointer apply it while they draw; for the others, wrap the
 * drawing calls in ctui_style_begin() / ctui_style_end(). Colours of the
 * tones (tab and status bars) are not covered; the palette sets those. */
typedef struct ctui_style {
	int attr[CTUI_PAIR_COUNT];
	ctui_glyphs glyphs;
} ctui_style;

/* The Solarized palette. */
ctui_palette ctui_palette_default(void);

/* Replace the palette used by the next ctui_theme_init() and by
 * ctui_theme_resolve() and ctui_tone_background(); NULL restores the default.
 */
void ctui_theme_set_palette(const ctui_palette *palette);
const ctui_palette *ctui_theme_palette(void);

/* Register colours and pairs, pick the glyph set from the locale. */
void ctui_theme_init(void);

bool ctui_theme_has_colors(void);
bool ctui_theme_unicode(void);

/* Replace the glyph sets used from now on (the ASCII one is used when the
 * locale is not UTF-8); NULL restores the built-in set. The strings must
 * outlive the program. Call before ctui_theme_init(). */
void ctui_theme_set_glyphs(const ctui_glyphs *unicode,
			   const ctui_glyphs *ascii);

/* The glyph set for a given encoding, independent of init. */
const ctui_glyphs *ctui_theme_glyphs_for(bool unicode);

/* The glyph set chosen at init (unicode before init). */
const ctui_glyphs *ctui_theme_glyphs(void);

/* An xterm-256 index, or its basic-colour stand-in when colours < 256 (the
 * role of a palette colour decides it, any other index goes to the nearest
 * of the 8 colours).
 * A negative index is the terminal default (-1). */
short ctui_theme_resolve(int index, int colors);

/* Background colour index of a tone. */
int ctui_tone_background(ctui_tone tone);

/* curses attribute for a pair; bold adds A_BOLD. */
int ctui_attr(ctui_pair pair, bool bold);

/* An attribute for a foreground and a background (xterm-256 indexes, -1 =
 * terminal default), to put in ctui_style.attr; 0 without colours. Call
 * after ctui_theme_init(). A few dozen pairs can be made. */
int ctui_attr_color(int fg, int bg);

/* Set aside up to `want` consecutive colour pairs for a component that
 * needs many (an image), leaving 32 for the theme. Returns the first pair
 * id and stores the number reserved in *got, or returns 0 with *got = 0
 * without colours or when none are left. Reserved pairs are the caller's
 * to init_pair(). */
int ctui_theme_pairs_reserve(int want, int *got);

/* Incremented by every ctui_theme_init(), which forgets all reservations. */
unsigned ctui_theme_generation(void);

/* Apply style (NULL = none) to ctui_attr() and ctui_theme_glyphs() until the
 * matching ctui_style_end(); they nest, four deep. */
void ctui_style_begin(const ctui_style *style);
void ctui_style_end(void);

/* Text on a tone's background (inactive text on the bar tones). */
int ctui_attr_segment(ctui_tone tone, bool bold);

/* Divider glyph colours: previous tone as foreground, next as background. */
int ctui_attr_divider(ctui_tone from, ctui_tone to);

/* Icon of a given colour, on the default or the selection background. */
int ctui_attr_icon(ctui_icon_color color, bool selected);

#endif /* CTUI_THEME_H */
