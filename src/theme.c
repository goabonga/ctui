/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */

/*
 * theme.c - the powerline Solarized palette and glyphs (theme.h).
 *
 * Curses state is process-global by nature; the few values chosen at init
 * live here beside it.
 */
#include <ctui/theme.h>

#include <curses.h>
#include <langinfo.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define ICON_BASE ((int)CTUI_PAIR_COUNT + (int)CTUI_TONE_COUNT)
#define ICON_PAIRS (2 * (int)CTUI_ICON_COLOR_COUNT)

static const ctui_glyphs UNICODE_GLYPHS = {
    "\xee\x82\xb0",
    "\xee\x82\xb1",
    "\xee\x82\xb2",
    "\xee\x82\xb3",
    "\xe2\x96\x88",
    "\xe2\x96\x91",
    "\xe2\x94\x80",
    "\xe2\x94\x80",
    "\xe2\x94\x82",
    "\xe2\x95\xad",
    "\xe2\x95\xae",
    "\xe2\x95\xb0",
    "\xe2\x95\xaf",
    "\xe2\x94\x82",
    "\xe2\x96\x88",
    "\xc3\x97",
    "\xe2\x9c\x93",
    "\xe2\x9c\x97",
    "i",
    "!",
    "\xe2\x96\xbe",
    "\xe2\x96\xb8",
    "\xe2\x96\xb4",
    "\xe2\x96\xbe",
    "\xe2\x97\x8f",
    "\xe2\x94\x81",
    "\xe2\x89\xa1",
    "\xe2\x80\xa6",
    true,
    .submenu = "\xe2\x80\xba",
};

static const ctui_glyphs ASCII_GLYPHS = {
    ">", "|", "<", "|", "#", "-", "-", "-",   "|",   "+",
    "+", "+", "+", "|", "#", "x", "+", "x",   "i",   "!",
    "v", ">", "^", "v", "O", "=", "=", "...", false, .submenu = ">",
};

static ctui_glyphs g_unicode_glyphs = UNICODE_GLYPHS,
		   g_ascii_glyphs = ASCII_GLYPHS;

static const short ICON_BASIC[CTUI_ICON_COLOR_COUNT] = {
    COLOR_BLUE, COLOR_CYAN,    COLOR_GREEN, COLOR_YELLOW,
    COLOR_RED,	COLOR_MAGENTA, COLOR_WHITE,
};

ctui_palette ctui_palette_default(void)
{
	return (ctui_palette){
	    .bar = CTUI_C_BAR,
	    .dark = CTUI_C_DARK,
	    .soft = CTUI_C_SOFT,
	    .inactive = CTUI_C_INACTIVE,
	    .text = CTUI_C_TEXT,
	    .green = CTUI_C_GREEN,
	    .red = CTUI_C_RED,
	    .yellow = CTUI_C_YELLOW,
	    .info = CTUI_C_INFO,
	    .green_bright = 82,
	    .yellow_bright = 220,
	    .red_bright = 196,
	    .accent = 75,
	    .cyan = 80,
	    .black = 16,
	    .icon = {75, 80, 114, 221, 209, 141, 250},
	};
}

static ctui_palette g_palette = {
    .bar = CTUI_C_BAR,
    .dark = CTUI_C_DARK,
    .soft = CTUI_C_SOFT,
    .inactive = CTUI_C_INACTIVE,
    .text = CTUI_C_TEXT,
    .green = CTUI_C_GREEN,
    .red = CTUI_C_RED,
    .yellow = CTUI_C_YELLOW,
    .info = CTUI_C_INFO,
    .green_bright = 82,
    .yellow_bright = 220,
    .red_bright = 196,
    .accent = 75,
    .cyan = 80,
    .black = 16,
    .icon = {75, 80, 114, 221, 209, 141, 250},
};

void ctui_theme_set_palette(const ctui_palette *palette)
{
	g_palette = palette ? *palette : ctui_palette_default();
}

const ctui_palette *ctui_theme_palette(void)
{
	return &g_palette;
}

static const ctui_glyphs *g_glyphs = &g_unicode_glyphs;
static bool g_colors;
static short g_divider[CTUI_TONE_COUNT][CTUI_TONE_COUNT];
static short g_next_pair;
static struct {
	int fg, bg;
	short pair;
} g_custom[32];
static size_t g_custom_count;
static unsigned g_generation;

/* The nearest of the 8 basic colours to an xterm-256 index. */
static short nearest_basic(int index)
{
	static const int LEVEL[6] = {0, 95, 135, 175, 215, 255};
	int r, g, b;
	if (index < 8)
		return (short)(index < 0 ? 0 : index);
	if (index < 16)
		return (short)(index - 8);
	if (index < 232) {
		int c = index - 16;
		r = LEVEL[c / 36];
		g = LEVEL[c / 6 % 6];
		b = LEVEL[c % 6];
	} else {
		r = g = b = 8 + 10 * ((index > 255 ? 255 : index) - 232);
	}
	int max = r > g ? (r > b ? r : b) : (g > b ? g : b);
	int min = r < g ? (r < b ? r : b) : (g < b ? g : b);
	if (max < 64)
		return COLOR_BLACK;
	if (max - min < 40)
		return max >= 128 ? COLOR_WHITE : COLOR_BLACK;
	return (short)((r * 5 >= max * 3 ? 1 : 0) + (g * 5 >= max * 3 ? 2 : 0) +
		       (b * 5 >= max * 3 ? 4 : 0));
}

short ctui_theme_resolve(int index, int colors)
{
	if (index < 0)
		return -1;
	if (colors >= 256)
		return (short)index;
	const ctui_palette *p = &g_palette;
	if (index == p->green || index == p->green_bright ||
	    index == p->icon[CTUI_ICON_GREEN])
		return COLOR_GREEN;
	if (index == p->red || index == p->red_bright ||
	    index == p->icon[CTUI_ICON_ORANGE])
		return COLOR_RED;
	if (index == p->yellow || index == p->yellow_bright ||
	    index == p->icon[CTUI_ICON_YELLOW])
		return COLOR_YELLOW;
	if (index == p->cyan || index == p->icon[CTUI_ICON_CYAN])
		return COLOR_CYAN;
	if (index == p->icon[CTUI_ICON_PURPLE])
		return COLOR_MAGENTA;
	if (index == p->accent || index == p->soft || index == p->info ||
	    index == p->icon[CTUI_ICON_BLUE])
		return COLOR_BLUE;
	if (index == p->black || index == p->bar || index == p->dark)
		return COLOR_BLACK;
	if (index == p->inactive || index == p->text ||
	    index == p->icon[CTUI_ICON_GRAY])
		return COLOR_WHITE;
	return nearest_basic(index);
}

int ctui_tone_background(ctui_tone tone)
{
	switch (tone) {
	case CTUI_TONE_DARK:
		return g_palette.dark;
	case CTUI_TONE_IDLE:
		return g_palette.soft;
	case CTUI_TONE_OK:
		return g_palette.green;
	case CTUI_TONE_BAD:
		return g_palette.red;
	case CTUI_TONE_BUSY:
		return g_palette.yellow;
	case CTUI_TONE_INFO:
		return g_palette.info;
	case CTUI_TONE_BAR:
	default:
		return g_palette.bar;
	}
}

static int tone_foreground(ctui_tone tone)
{
	return tone == CTUI_TONE_BAR || tone == CTUI_TONE_DARK
		   ? g_palette.inactive
		   : g_palette.text;
}

typedef struct pair_def {
	ctui_pair pair;
	int fg, bg;
} pair_def;

static size_t pair_defs(pair_def out[CTUI_PAIR_COUNT])
{
	const ctui_palette *p = &g_palette;
	const pair_def defs[] = {
	    {CTUI_PAIR_BODY, -1, -1},
	    {CTUI_PAIR_BAR, p->inactive, p->bar},
	    {CTUI_PAIR_SOFT, p->soft, p->bar},
	    {CTUI_PAIR_NOTICE_WARNING, p->yellow, p->bar},
	    {CTUI_PAIR_NOTICE_ERROR, p->red, p->bar},
	    {CTUI_PAIR_GAUGE_GREEN, p->green, -1},
	    {CTUI_PAIR_GAUGE_GREEN_BRIGHT, p->green_bright, -1},
	    {CTUI_PAIR_GAUGE_YELLOW, p->yellow, -1},
	    {CTUI_PAIR_GAUGE_YELLOW_BRIGHT, p->yellow_bright, -1},
	    {CTUI_PAIR_GAUGE_RED, p->red, -1},
	    {CTUI_PAIR_GAUGE_RED_BRIGHT, p->red_bright, -1},
	    {CTUI_PAIR_GAUGE_EMPTY, p->soft, -1},
	    {CTUI_PAIR_HEADING, -1, -1},
	    {CTUI_PAIR_MUTED, p->inactive, -1},
	    {CTUI_PAIR_DIALOG, p->text, p->bar},
	    {CTUI_PAIR_DIALOG_BORDER, p->soft, p->bar},
	    {CTUI_PAIR_SELECTION, p->text, p->info},
	    {CTUI_PAIR_BORDER, p->soft, -1},
	    {CTUI_PAIR_BORDER_SELECTED, p->accent, -1},
	    {CTUI_PAIR_ERROR, p->red, -1},
	    {CTUI_PAIR_CURSOR, p->cyan, -1},
	    {CTUI_PAIR_CURSOR_SELECTED, p->cyan, p->info},
	    {CTUI_PAIR_PLAYHEAD, p->black, p->yellow},
	};
	size_t n = sizeof defs / sizeof defs[0];
	for (size_t i = 0; i < n; i++)
		out[i] = defs[i];
	return n;
}

static void register_pair(short id, int fg, int bg, bool defaults)
{
	short f = ctui_theme_resolve(fg, COLORS),
	      b = ctui_theme_resolve(bg, COLORS);
	if (!defaults) { /* no default colours: body is white on black */
		if (f < 0)
			f = COLOR_WHITE;
		if (b < 0)
			b = COLOR_BLACK;
	}
	if (id < COLOR_PAIRS)
		init_pair(id, f, b);
}

const ctui_glyphs *ctui_theme_glyphs_for(bool unicode)
{
	return unicode ? &g_unicode_glyphs : &g_ascii_glyphs;
}

void ctui_theme_set_glyphs(const ctui_glyphs *unicode, const ctui_glyphs *ascii)
{
	g_unicode_glyphs = unicode ? *unicode : UNICODE_GLYPHS;
	g_unicode_glyphs.unicode = true;
	g_ascii_glyphs = ascii ? *ascii : ASCII_GLYPHS;
	g_ascii_glyphs.unicode = false;
	g_glyphs = g_glyphs->unicode ? &g_unicode_glyphs : &g_ascii_glyphs;
}

void ctui_theme_init(void)
{
	const char *codeset = nl_langinfo(CODESET);
	bool utf8 = codeset && (strcasestr(codeset, "UTF-8") ||
				strcasestr(codeset, "UTF8"));
	g_glyphs = ctui_theme_glyphs_for(utf8 && !getenv("CTUI_ASCII"));
	g_colors = has_colors() && !getenv("NO_COLOR");
	memset(g_divider, 0, sizeof g_divider);
	g_next_pair = (short)(ICON_BASE + ICON_PAIRS);
	g_custom_count = 0;
	g_generation++;
	if (!g_colors)
		return;
	start_color();
	bool defaults = use_default_colors() == OK;
	pair_def defs[CTUI_PAIR_COUNT];
	size_t n = pair_defs(defs);
	for (size_t i = 0; i < n; i++)
		register_pair((short)defs[i].pair, defs[i].fg, defs[i].bg,
			      defaults);
	for (int t = 0; t < CTUI_TONE_COUNT; t++)
		register_pair((short)(CTUI_PAIR_COUNT + t),
			      tone_foreground((ctui_tone)t),
			      ctui_tone_background((ctui_tone)t), defaults);
	for (int c = 0; c < CTUI_ICON_COLOR_COUNT; c++) {
		int fg = COLORS >= 256 ? g_palette.icon[c] : ICON_BASIC[c];
		if (COLORS >= 256) {
			register_pair((short)(ICON_BASE + c), fg, -1, defaults);
			register_pair(
			    (short)(ICON_BASE + CTUI_ICON_COLOR_COUNT + c), fg,
			    g_palette.info, defaults);
		} else {
			short b = defaults ? -1 : COLOR_BLACK;
			if (ICON_BASE + c < COLOR_PAIRS)
				init_pair((short)(ICON_BASE + c), (short)fg, b);
			if (ICON_BASE + CTUI_ICON_COLOR_COUNT + c < COLOR_PAIRS)
				init_pair((short)(ICON_BASE +
						  CTUI_ICON_COLOR_COUNT + c),
					  (short)fg, COLOR_BLUE);
		}
	}
}

bool ctui_theme_has_colors(void)
{
	return g_colors;
}

bool ctui_theme_unicode(void)
{
	return g_glyphs->unicode;
}

#define STYLE_DEPTH 4

static const ctui_style *g_styles[STYLE_DEPTH];
static ctui_glyphs g_merged[STYLE_DEPTH];
static int g_style_depth;

const ctui_glyphs *ctui_theme_glyphs(void)
{
	return g_style_depth > 0 && g_style_depth <= STYLE_DEPTH
		   ? &g_merged[g_style_depth - 1]
		   : g_glyphs;
}

static const char *own(const char *style, const char *base)
{
	return style ? style : base;
}

void ctui_style_begin(const ctui_style *style)
{
	int d = g_style_depth++;
	if (d >= STYLE_DEPTH)
		return;
	g_styles[d] = style;
	const ctui_glyphs *base = d > 0 ? &g_merged[d - 1] : g_glyphs;
	ctui_glyphs g = *base;
	if (style) {
		const ctui_glyphs *s = &style->glyphs;
		g.hard_right = own(s->hard_right, base->hard_right);
		g.soft_right = own(s->soft_right, base->soft_right);
		g.hard_left = own(s->hard_left, base->hard_left);
		g.soft_left = own(s->soft_left, base->soft_left);
		g.gauge_filled = own(s->gauge_filled, base->gauge_filled);
		g.gauge_empty = own(s->gauge_empty, base->gauge_empty);
		g.rule = own(s->rule, base->rule);
		g.box_h = own(s->box_h, base->box_h);
		g.box_v = own(s->box_v, base->box_v);
		g.box_tl = own(s->box_tl, base->box_tl);
		g.box_tr = own(s->box_tr, base->box_tr);
		g.box_bl = own(s->box_bl, base->box_bl);
		g.box_br = own(s->box_br, base->box_br);
		g.scroll_track = own(s->scroll_track, base->scroll_track);
		g.scroll_thumb = own(s->scroll_thumb, base->scroll_thumb);
		g.close = own(s->close, base->close);
		g.check = own(s->check, base->check);
		g.cross = own(s->cross, base->cross);
		g.info = own(s->info, base->info);
		g.warn = own(s->warn, base->warn);
		g.tree_open = own(s->tree_open, base->tree_open);
		g.tree_shut = own(s->tree_shut, base->tree_shut);
		g.select_open = own(s->select_open, base->select_open);
		g.select_shut = own(s->select_shut, base->select_shut);
		g.knob = own(s->knob, base->knob);
		g.heavy = own(s->heavy, base->heavy);
		g.burger = own(s->burger, base->burger);
		g.ellipsis = own(s->ellipsis, base->ellipsis);
		g.submenu = own(s->submenu, base->submenu);
	}
	g_merged[d] = g;
}

void ctui_style_end(void)
{
	if (g_style_depth > 0)
		g_style_depth--;
}

static int style_attr(ctui_pair pair)
{
	for (int d = g_style_depth > STYLE_DEPTH ? STYLE_DEPTH : g_style_depth;
	     d-- > 0;)
		if (g_styles[d] && (int)pair > 0 &&
		    (int)pair < CTUI_PAIR_COUNT && g_styles[d]->attr[pair])
			return g_styles[d]->attr[pair];
	return 0;
}

int ctui_attr_color(int fg, int bg)
{
	if (!g_colors)
		return 0;
	for (size_t i = 0; i < g_custom_count; i++)
		if (g_custom[i].fg == fg && g_custom[i].bg == bg)
			return (int)COLOR_PAIR(g_custom[i].pair);
	if (g_custom_count >= sizeof g_custom / sizeof g_custom[0] ||
	    g_next_pair >= COLOR_PAIRS)
		return 0;
	g_custom[g_custom_count].fg = fg;
	g_custom[g_custom_count].bg = bg;
	g_custom[g_custom_count].pair = g_next_pair++;
	register_pair(g_custom[g_custom_count].pair, fg, bg, true);
	return (int)COLOR_PAIR(g_custom[g_custom_count++].pair);
}

int ctui_theme_pairs_reserve(int want, int *got)
{
	*got = 0;
	int free_pairs = COLOR_PAIRS - g_next_pair - 32;
	if (!g_colors || want < 1 || free_pairs < 1)
		return 0;
	if (want > free_pairs)
		want = free_pairs;
	if (g_next_pair + want > 32767)
		want = 32767 - g_next_pair;
	int first = g_next_pair;
	g_next_pair = (short)(g_next_pair + want);
	*got = want;
	return first;
}

unsigned ctui_theme_generation(void)
{
	return g_generation;
}

int ctui_attr(ctui_pair pair, bool bold)
{
	unsigned attr = bold ? (unsigned)A_BOLD : 0U;
	int custom = style_attr(pair);
	if (custom)
		return (int)(attr | (unsigned)custom);
	if (g_colors && (int)pair < COLOR_PAIRS)
		return (int)(attr | (unsigned)COLOR_PAIR(pair));
	switch (pair) {
	case CTUI_PAIR_SELECTION:
	case CTUI_PAIR_CURSOR_SELECTED:
	case CTUI_PAIR_PLAYHEAD:
		attr |= (unsigned)A_REVERSE;
		break;
	case CTUI_PAIR_SOFT:
	case CTUI_PAIR_MUTED:
	case CTUI_PAIR_GAUGE_EMPTY:
	case CTUI_PAIR_BORDER:
	case CTUI_PAIR_DIALOG_BORDER:
		attr |= (unsigned)A_DIM;
		break;
	default:
		break;
	}
	return (int)attr;
}

int ctui_attr_segment(ctui_tone tone, bool bold)
{
	unsigned attr = bold ? (unsigned)A_BOLD : 0U;
	int pair = (int)CTUI_PAIR_COUNT + (int)tone;
	if (g_colors && pair < COLOR_PAIRS)
		return (int)(attr | (unsigned)COLOR_PAIR(pair));
	if (tone != CTUI_TONE_BAR && tone != CTUI_TONE_DARK)
		attr |= (unsigned)A_REVERSE;
	return (int)attr;
}

int ctui_attr_divider(ctui_tone from, ctui_tone to)
{
	if (!g_colors || from < 0 || from >= CTUI_TONE_COUNT || to < 0 ||
	    to >= CTUI_TONE_COUNT)
		return 0;
	short *slot = &g_divider[from][to];
	if (!*slot) {
		if (g_next_pair >= COLOR_PAIRS)
			return 0;
		*slot = g_next_pair++;
		register_pair(*slot, ctui_tone_background(from),
			      ctui_tone_background(to), true);
	}
	return (int)COLOR_PAIR(*slot);
}

int ctui_attr_icon(ctui_icon_color color, bool selected)
{
	int pair =
	    ICON_BASE + (selected ? CTUI_ICON_COLOR_COUNT : 0) + (int)color;
	if (g_colors && pair < COLOR_PAIRS)
		return (int)COLOR_PAIR(pair);
	return selected ? (int)A_REVERSE : 0;
}
