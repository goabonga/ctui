/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Chris <goabonga@pm.me> */
#include "check.h"
#include "screen.h"

#include <curses.h>

#include <ctui/theme.h>

static void test_resolve_rich(void)
{
	CHECK(ctui_theme_resolve(235, 256) == 235);
	CHECK(ctui_theme_resolve(-1, 256) == -1);
	CHECK(ctui_theme_resolve(-1, 8) == -1);
}

static void test_resolve_basic(void)
{
	CHECK(ctui_theme_resolve(CTUI_C_GREEN, 8) == COLOR_GREEN);
	CHECK(ctui_theme_resolve(CTUI_C_RED, 8) == COLOR_RED);
	CHECK(ctui_theme_resolve(CTUI_C_YELLOW, 8) == COLOR_YELLOW);
	CHECK(ctui_theme_resolve(CTUI_C_TEXT, 8) == COLOR_WHITE);
	CHECK(ctui_theme_resolve(CTUI_C_INACTIVE, 8) == COLOR_WHITE);
	CHECK(ctui_theme_resolve(CTUI_C_SOFT, 8) == COLOR_BLUE);
	CHECK(ctui_theme_resolve(CTUI_C_BAR, 8) == COLOR_BLACK);
	CHECK(ctui_theme_resolve(CTUI_C_DARK, 8) == COLOR_BLACK);
}

static void test_glyph_sets(void)
{
	const ctui_glyphs *u = ctui_theme_glyphs_for(true),
			  *a = ctui_theme_glyphs_for(false);
	CHECK(u->unicode && !a->unicode);
	CHECK_STR(u->hard_right, "\xee\x82\xb0");
	CHECK_STR(a->hard_right, ">");
	CHECK_STR(a->soft_right, "|");
	CHECK_STR(a->hard_left, "<");
	CHECK_STR(u->gauge_filled, "\xe2\x96\x88");
	CHECK_STR(a->gauge_filled, "#");
	CHECK_STR(a->gauge_empty, "-");
	CHECK_STR(u->box_tl, "\xe2\x95\xad");
	CHECK_STR(a->box_tl, "+");
}

static void test_tone_backgrounds(void)
{
	CHECK(ctui_tone_background(CTUI_TONE_BAR) == CTUI_C_BAR);
	CHECK(ctui_tone_background(CTUI_TONE_DARK) == CTUI_C_DARK);
	CHECK(ctui_tone_background(CTUI_TONE_OK) == CTUI_C_GREEN);
	CHECK(ctui_tone_background(CTUI_TONE_BAD) == CTUI_C_RED);
	CHECK(ctui_tone_background(CTUI_TONE_BUSY) == CTUI_C_YELLOW);
}

static void test_palette(void)
{
	ctui_palette d = ctui_palette_default();
	CHECK(d.bar == CTUI_C_BAR && d.icon[CTUI_ICON_BLUE] == 75);
	CHECK(ctui_theme_palette()->green == CTUI_C_GREEN);

	ctui_palette p = d;
	p.bar = 17;
	p.green = 28;
	p.soft = 33;
	ctui_theme_set_palette(&p);
	CHECK(ctui_tone_background(CTUI_TONE_BAR) == 17);
	CHECK(ctui_tone_background(CTUI_TONE_OK) == 28);
	CHECK(ctui_theme_resolve(28, 8) == COLOR_GREEN);
	CHECK(ctui_theme_resolve(17, 8) == COLOR_BLACK);
	CHECK(ctui_theme_resolve(33, 8) == COLOR_BLUE);
	CHECK(ctui_theme_resolve(28, 256) == 28);

	/* an index with no role goes to the nearest basic colour */
	CHECK(ctui_theme_resolve(9, 8) == COLOR_RED);
	CHECK(ctui_theme_resolve(46, 8) == COLOR_GREEN); /* pure green */
	CHECK(ctui_theme_resolve(21, 8) == COLOR_BLUE);	 /* pure blue */
	CHECK(ctui_theme_resolve(226, 8) == COLOR_YELLOW);
	CHECK(ctui_theme_resolve(201, 8) == COLOR_MAGENTA);
	CHECK(ctui_theme_resolve(51, 8) == COLOR_CYAN);
	CHECK(ctui_theme_resolve(255, 8) == COLOR_WHITE);
	CHECK(ctui_theme_resolve(233, 8) == COLOR_BLACK);

	ctui_theme_set_palette(NULL);
	CHECK(ctui_tone_background(CTUI_TONE_BAR) == CTUI_C_BAR);
	CHECK(ctui_tone_background(CTUI_TONE_OK) == CTUI_C_GREEN);
}

static void test_custom_glyphs(void)
{
	const ctui_glyphs *u = ctui_theme_glyphs_for(true),
			  *a = ctui_theme_glyphs_for(false);
	CHECK_STR(u->scroll_thumb, "\xe2\x96\x88");
	CHECK_STR(a->scroll_thumb, "#");
	CHECK_STR(u->close, "\xc3\x97");
	CHECK_STR(a->close, "x");
	CHECK_STR(a->ellipsis, "...");
	CHECK_STR(a->submenu, ">");
	CHECK_STR(u->submenu, "\xe2\x80\xba");

	ctui_glyphs mine = *u;
	mine.scroll_thumb = "@";
	mine.close = "X";
	mine.unicode = false; /* the flag follows the slot, not the struct */
	ctui_theme_set_glyphs(&mine, NULL);
	CHECK_STR(ctui_theme_glyphs_for(true)->scroll_thumb, "@");
	CHECK(ctui_theme_glyphs_for(true)->unicode);
	CHECK_STR(ctui_theme_glyphs()->close,
		  "X"); /* before init the Unicode set is active */
	CHECK_STR(ctui_theme_glyphs_for(false)->scroll_thumb, "#");

	ctui_glyphs plain = *a;
	plain.scroll_thumb = "=";
	ctui_theme_set_glyphs(NULL, &plain);
	CHECK_STR(ctui_theme_glyphs_for(true)->scroll_thumb, "\xe2\x96\x88");
	CHECK_STR(ctui_theme_glyphs_for(false)->scroll_thumb, "=");
	CHECK(!ctui_theme_glyphs_for(false)->unicode);

	ctui_theme_set_glyphs(NULL, NULL);
	CHECK_STR(ctui_theme_glyphs_for(false)->scroll_thumb, "#");
}

static void test_before_init(void)
{
	CHECK(ctui_theme_glyphs()->unicode);
	CHECK(!ctui_theme_has_colors());
	CHECK(ctui_attr(CTUI_PAIR_BAR, true) == (int)A_BOLD);
	CHECK(ctui_attr(CTUI_PAIR_SELECTION, false) == (int)A_REVERSE);
	CHECK(ctui_attr_divider(CTUI_TONE_BAR, CTUI_TONE_OK) == 0);
}

static void test_style(void)
{
	ctui_style style = {0};
	style.attr[CTUI_PAIR_BORDER] = (int)A_UNDERLINE;
	style.glyphs.close = "x";
	CHECK(ctui_attr(CTUI_PAIR_BORDER, false) != (int)A_UNDERLINE);
	ctui_style_begin(&style);
	CHECK(ctui_attr(CTUI_PAIR_BORDER, false) == (int)A_UNDERLINE);
	CHECK(ctui_attr(CTUI_PAIR_BORDER, true) == (int)(A_UNDERLINE | A_BOLD));
	CHECK(ctui_attr(CTUI_PAIR_BAR, true) ==
	      (int)A_BOLD); /* unset pairs keep the theme's */
	CHECK_STR(ctui_theme_glyphs()->close, "x");
	CHECK_STR(ctui_theme_glyphs()->check,
		  ctui_theme_glyphs_for(true)->check);
	CHECK(ctui_theme_glyphs()->unicode);

	ctui_style inner = {0};
	inner.glyphs.check = "v";
	ctui_style_begin(&inner);
	CHECK_STR(ctui_theme_glyphs()->check, "v");
	CHECK_STR(ctui_theme_glyphs()->close,
		  "x"); /* the outer style still applies */
	ctui_style_end();
	CHECK_STR(ctui_theme_glyphs()->check,
		  ctui_theme_glyphs_for(true)->check);
	ctui_style_end();
	CHECK_STR(ctui_theme_glyphs()->close,
		  ctui_theme_glyphs_for(true)->close);

	ctui_style_begin(NULL); /* a NULL style changes nothing */
	CHECK_STR(ctui_theme_glyphs()->close,
		  ctui_theme_glyphs_for(true)->close);
	ctui_style_end();
	ctui_style_end();		   /* unbalanced calls are harmless */
	CHECK(ctui_attr_color(1, 2) == 0); /* no colours before init */
}

static void test_pairs_reserve(void)
{
	int got = -1;
	CHECK(ctui_theme_pairs_reserve(10, &got) == 0 &&
	      got == 0); /* before init */
	screen_open(24, 80);
	unsigned generation = ctui_theme_generation();
	int first = ctui_theme_pairs_reserve(100, &got);
	CHECK(first > 0 && got == 100);
	int second = ctui_theme_pairs_reserve(5, &got);
	CHECK(second == first + 100 && got == 5);
	CHECK(ctui_theme_pairs_reserve(0, &got) == 0 && got == 0);
	CHECK(ctui_theme_pairs_reserve(0x40000000, &got) > 0 &&
	      got <= COLOR_PAIRS - 32);
	ctui_theme_init();
	CHECK(ctui_theme_generation() == generation + 1);
	CHECK(ctui_theme_pairs_reserve(1, &got) > 0 && got == 1);
}

int main(void)
{
	RUN(test_resolve_rich);
	RUN(test_resolve_basic);
	RUN(test_glyph_sets);
	RUN(test_tone_backgrounds);
	RUN(test_palette);
	RUN(test_custom_glyphs);
	RUN(test_before_init);
	RUN(test_style);
	RUN(test_pairs_reserve);
	return CHECK_DONE();
}
